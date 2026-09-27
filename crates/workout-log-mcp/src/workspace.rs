//! The repository the server is pointed at: the session folder plus the two
//! hand-maintained reference files beside it.

use std::fs;
use std::path::{Path, PathBuf};

use workout_log_core::coding::encode_json;
use workout_log_core::io::session_storage::{SessionStorage, StorageError, StorageResult};
use workout_log_core::io::session_store::SessionStore;
use workout_log_core::{Catalogue, CycleCatalogue, Session};

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
#[error("{0}")]
pub struct WorkspaceError(pub String);

pub struct DirectoryStorage {
    directory: PathBuf,
}

impl DirectoryStorage {
    pub fn new(directory: PathBuf) -> Self {
        Self { directory }
    }

    fn file(&self, id: &str) -> StorageResult<PathBuf> {
        if id.contains('/') || id.contains("..") {
            return Err(StorageError(format!(
                "Invalid argument(s) (id): must be a bare file name: \"{id}\""
            )));
        }
        Ok(self.directory.join(id))
    }
}

impl SessionStorage for DirectoryStorage {
    fn list_ids(&self) -> StorageResult<Vec<String>> {
        if !self.directory.is_dir() {
            return Ok(Vec::new());
        }
        let entries = fs::read_dir(&self.directory).map_err(|e| StorageError(e.to_string()))?;
        Ok(entries
            .filter_map(Result::ok)
            .filter(|e| e.path().is_file())
            .map(|e| e.file_name().to_string_lossy().into_owned())
            .collect())
    }

    fn read(&self, id: &str) -> StorageResult<String> {
        fs::read_to_string(self.file(id)?).map_err(|e| StorageError(e.to_string()))
    }

    /// Write-then-rename: a crash mid-write leaves the previous session intact
    /// rather than a truncated file.
    fn write(&mut self, id: &str, contents: &str) -> StorageResult<()> {
        let path = self.file(id)?;
        fs::create_dir_all(&self.directory).map_err(|e| StorageError(e.to_string()))?;
        let temporary = path.with_extension("json.tmp");
        fs::write(&temporary, contents).map_err(|e| StorageError(e.to_string()))?;
        fs::rename(&temporary, &path).map_err(|e| StorageError(e.to_string()))
    }

    fn delete(&mut self, id: &str) -> StorageResult<()> {
        let path = self.file(id)?;
        if path.exists() {
            fs::remove_file(path).map_err(|e| StorageError(e.to_string()))?;
        }
        Ok(())
    }

    fn label(&self) -> String {
        self.directory.display().to_string()
    }
}

pub struct Workspace {
    pub root: PathBuf,
    pub data_directory: PathBuf,
    pub sessions: SessionStore,
}

pub struct LoadOutcome {
    pub sessions: Vec<Session>,
    pub failures: Vec<(String, String)>,
}

impl Workspace {
    /// `--repo` wins, else the nearest ancestor holding `cycles.json`, else the
    /// working directory. `--data` wins over `$WORKOUTLOG_DATA`, else `<root>/data`.
    pub fn open(
        root: Option<PathBuf>,
        data: Option<PathBuf>,
        from_environment: Option<String>,
    ) -> Self {
        let root = root
            .or_else(find_root)
            .unwrap_or_else(|| PathBuf::from("."));
        let data_directory = data
            .or_else(|| {
                from_environment
                    .filter(|v| !v.is_empty())
                    .map(PathBuf::from)
            })
            .unwrap_or_else(|| root.join("data"));
        Self {
            sessions: SessionStore::new(Box::new(DirectoryStorage::new(data_directory.clone()))),
            root,
            data_directory,
        }
    }

    pub fn label(&self) -> String {
        self.data_directory.display().to_string()
    }

    pub fn catalogue_file(&self) -> PathBuf {
        self.root.join("exercises.json")
    }

    pub fn cycles_file(&self) -> PathBuf {
        self.root.join("cycles.json")
    }

    pub fn load_catalogue(&self) -> Result<Catalogue, WorkspaceError> {
        let text = self.read_reference(&self.catalogue_file())?;
        serde_json::from_str(&text).map_err(|e| WorkspaceError(e.to_string()))
    }

    pub fn load_cycles(&self) -> Result<CycleCatalogue, WorkspaceError> {
        let text = self.read_reference(&self.cycles_file())?;
        serde_json::from_str(&text).map_err(|e| WorkspaceError(e.to_string()))
    }

    pub fn save_catalogue(&self, catalogue: &Catalogue) -> Result<(), WorkspaceError> {
        self.write_reference(&self.catalogue_file(), catalogue)
    }

    pub fn save_cycles(&self, cycles: &CycleCatalogue) -> Result<(), WorkspaceError> {
        self.write_reference(&self.cycles_file(), cycles)
    }

    /// Range bounds are inclusive and compared as text, which is what ISO dates
    /// are for. Parse failures are reported, never thrown away.
    pub fn load_all(&self, from: Option<&str>, to: Option<&str>) -> LoadOutcome {
        let loaded = match self.sessions.load_all() {
            Ok(loaded) => loaded,
            Err(error) => {
                return LoadOutcome {
                    sessions: Vec::new(),
                    failures: vec![(self.label(), error.to_string())],
                };
            }
        };
        let mut sessions: Vec<Session> = loaded
            .sessions
            .into_iter()
            .filter(|s| in_range(&s.date, from, to))
            .collect();
        sessions.sort_by(|a, b| a.date.cmp(&b.date));
        LoadOutcome {
            sessions,
            failures: loaded
                .failures
                .into_iter()
                .map(|f| (f.id, f.error))
                .collect(),
        }
    }

    fn read_reference(&self, path: &Path) -> Result<String, WorkspaceError> {
        fs::read_to_string(path).map_err(|_| {
            WorkspaceError(format!(
                "no {} under {} — point the server at the repository with --repo",
                path.file_name().unwrap_or_default().to_string_lossy(),
                self.root.display()
            ))
        })
    }

    fn write_reference<T: serde::Serialize>(
        &self,
        path: &Path,
        value: &T,
    ) -> Result<(), WorkspaceError> {
        let text =
            encode_json(&serde_json::to_value(value).map_err(|e| WorkspaceError(e.to_string()))?);
        let temporary = path.with_extension("json.tmp");
        fs::write(&temporary, text).map_err(|e| WorkspaceError(e.to_string()))?;
        fs::rename(&temporary, path).map_err(|e| WorkspaceError(e.to_string()))
    }
}

fn in_range(date: &str, from: Option<&str>, to: Option<&str>) -> bool {
    from.is_none_or(|f| date >= f) && to.is_none_or(|t| date <= t)
}

/// The nearest ancestor of the working directory that holds `cycles.json`.
pub fn find_root() -> Option<PathBuf> {
    std::env::current_dir()
        .ok()?
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .map(Path::to_path_buf)
}
