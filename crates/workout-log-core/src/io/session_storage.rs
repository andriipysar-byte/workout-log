//! The byte-level backend a [`super::session_store::SessionStore`] sits on.
//!
//! Declared here, implemented per platform: this is the seam that keeps the
//! filesystem out of the core. An id is a bare filename, `2026-08-06_D2.json`.

use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
#[error("{0}")]
pub struct StorageError(pub String);

pub type StorageResult<T> = Result<T, StorageError>;

pub trait SessionStorage: Send + Sync {
    fn list_ids(&self) -> StorageResult<Vec<String>>;
    fn read(&self, id: &str) -> StorageResult<String>;
    fn write(&mut self, id: &str, contents: &str) -> StorageResult<()>;
    fn delete(&mut self, id: &str) -> StorageResult<()>;
    /// A human-readable name for the backing location, for the status bar.
    fn label(&self) -> String;
}

/// In-memory backend: the test double, and the whole store wherever there is no
/// folder to point at.
#[derive(Debug, Clone)]
pub struct MemoryStorage {
    files: BTreeMap<String, String>,
    label: String,
}

impl Default for MemoryStorage {
    fn default() -> Self {
        Self {
            files: BTreeMap::new(),
            label: "in-memory".into(),
        }
    }
}

impl MemoryStorage {
    pub fn new(label: impl Into<String>) -> Self {
        Self {
            files: BTreeMap::new(),
            label: label.into(),
        }
    }

    pub fn seeded(seed: BTreeMap<String, String>, label: impl Into<String>) -> Self {
        Self {
            files: seed,
            label: label.into(),
        }
    }

    pub fn snapshot(&self) -> &BTreeMap<String, String> {
        &self.files
    }
}

impl SessionStorage for MemoryStorage {
    fn list_ids(&self) -> StorageResult<Vec<String>> {
        Ok(self.files.keys().cloned().collect())
    }

    fn read(&self, id: &str) -> StorageResult<String> {
        self.files
            .get(id)
            .cloned()
            .ok_or_else(|| StorageError(format!("no such session: {id}")))
    }

    fn write(&mut self, id: &str, contents: &str) -> StorageResult<()> {
        self.files.insert(id.to_string(), contents.to_string());
        Ok(())
    }

    fn delete(&mut self, id: &str) -> StorageResult<()> {
        self.files.remove(id);
        Ok(())
    }

    fn label(&self) -> String {
        self.label.clone()
    }
}
