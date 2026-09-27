use super::session_storage::{SessionStorage, StorageError, StorageResult};
use crate::coding::{decode_json, encode_json};
use crate::models::session::Session;

pub struct LoadFailure {
    pub id: String,
    pub error: String,
}

pub struct LoadResult {
    pub sessions: Vec<Session>,
    pub failures: Vec<LoadFailure>,
}

/// Files are the source of truth (ADR-001). Filename: `YYYY-MM-DD_<cycleDay>.json`.
pub struct SessionStore {
    pub storage: Box<dyn SessionStorage>,
}

impl SessionStore {
    pub fn new(storage: Box<dyn SessionStorage>) -> Self {
        Self { storage }
    }

    pub fn id_for(session: &Session) -> String {
        format!("{}_{}.json", session.date, session.cycle_day)
    }

    /// Sorting and the `.json` filter live here rather than in each backend, so
    /// every platform presents the archive in the same order.
    pub fn list_ids(&self) -> StorageResult<Vec<String>> {
        let mut ids: Vec<String> = self
            .storage
            .list_ids()?
            .into_iter()
            .filter(|id| id.ends_with(".json"))
            .collect();
        ids.sort();
        Ok(ids)
    }

    pub fn load(&self, id: &str) -> StorageResult<Session> {
        let text = self.storage.read(id)?;
        serde_json::from_str(&text).map_err(|e| StorageError(e.to_string()))
    }

    /// A corrupted file costs one session, never the archive: parse failures are
    /// collected, not returned as an error.
    pub fn load_all(&self) -> StorageResult<LoadResult> {
        let mut sessions = Vec::new();
        let mut failures = Vec::new();
        for id in self.list_ids()? {
            match self.load(&id) {
                Ok(session) => sessions.push(session),
                Err(error) => failures.push(LoadFailure {
                    id,
                    error: error.to_string(),
                }),
            }
        }
        Ok(LoadResult { sessions, failures })
    }

    pub fn exists(&self, id: &str) -> StorageResult<bool> {
        Ok(self.list_ids()?.iter().any(|existing| existing == id))
    }

    pub fn encode(session: &Session) -> String {
        encode_json(&serde_json::to_value(session).expect("a session is always encodable"))
    }

    /// Writes the session under its derived id and, when the header moved it,
    /// removes the file it used to live in — otherwise editing the date or cycle
    /// day silently orphans the old file.
    pub fn save(&mut self, session: &Session, previous_id: Option<&str>) -> StorageResult<String> {
        let id = Self::id_for(session);
        self.storage.write(&id, &Self::encode(session))?;
        if let Some(previous) = previous_id
            && previous != id
        {
            self.storage.delete(previous)?;
        }
        Ok(id)
    }

    pub fn delete(&mut self, id: &str) -> StorageResult<()> {
        self.storage.delete(id)
    }

    pub fn decode(text: &str) -> Result<Session, String> {
        decode_json(text)
            .map_err(|e| e.to_string())
            .and_then(|v| serde_json::from_value(v).map_err(|e| e.to_string()))
    }
}
