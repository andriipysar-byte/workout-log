//! Where the archive lives. The core declares `SessionStorage` as the seam
//! (ADR-004); this is the desktop side of it, plus the browser working copy
//! ADR-007 falls back to where there is no folder to point at.

use workout_log_core::io::session_storage::SessionStorage;

/// `exercises.json` and `cycles.json` compiled in, so a folder that has no copy
/// of its own still starts with the catalogue the app shipped with.
pub const BUNDLED_CATALOGUE: &str = include_str!("../../../exercises.json");
pub const BUNDLED_CYCLES: &str = include_str!("../../../cycles.json");
pub const MAP_TEMPLATE: &str = include_str!("../../../assets/muscle-map.svg");

/// Reads and writes the two hand-maintained files beside the session folder.
/// `read` returning None means "use the bundled copy", not "missing".
pub trait ReferenceStore {
    fn can_write(&self) -> bool;
    fn read(&self, name: &str) -> Option<String>;
    fn write(&self, name: &str, contents: &str) -> Result<(), String>;
}

#[cfg(not(feature = "desktop"))]
pub struct BundledReferenceStore;

#[cfg(not(feature = "desktop"))]
impl ReferenceStore for BundledReferenceStore {
    fn can_write(&self) -> bool {
        false
    }

    fn read(&self, _name: &str) -> Option<String> {
        None
    }

    fn write(&self, _name: &str, _contents: &str) -> Result<(), String> {
        Err("this platform ships the plan read-only (ADR-007)".into())
    }
}

pub struct SessionFolder {
    pub storage: Box<dyn SessionStorage>,
    pub references: Box<dyn ReferenceStore>,
    pub can_choose_folder: bool,
    pub is_working_copy: bool,
}

#[cfg(feature = "desktop")]
mod native {
    use std::fs;
    use std::path::{Path, PathBuf};

    use workout_log_core::io::session_storage::{SessionStorage, StorageError, StorageResult};

    use super::{ReferenceStore, SessionFolder};

    pub struct DirectoryStorage {
        directory: PathBuf,
    }

    impl SessionStorage for DirectoryStorage {
        fn list_ids(&self) -> StorageResult<Vec<String>> {
            if !self.directory.is_dir() {
                return Ok(Vec::new());
            }
            Ok(fs::read_dir(&self.directory)
                .map_err(|e| StorageError(e.to_string()))?
                .filter_map(Result::ok)
                .filter(|entry| entry.path().is_file())
                .map(|entry| entry.file_name().to_string_lossy().into_owned())
                .collect())
        }

        fn read(&self, id: &str) -> StorageResult<String> {
            fs::read_to_string(self.directory.join(id)).map_err(|e| StorageError(e.to_string()))
        }

        /// Write-then-rename: a crash mid-write leaves the previous session
        /// intact rather than a truncated file.
        fn write(&mut self, id: &str, contents: &str) -> StorageResult<()> {
            fs::create_dir_all(&self.directory).map_err(|e| StorageError(e.to_string()))?;
            let path = self.directory.join(id);
            let temporary = path.with_extension("json.tmp");
            fs::write(&temporary, contents).map_err(|e| StorageError(e.to_string()))?;
            fs::rename(temporary, path).map_err(|e| StorageError(e.to_string()))
        }

        fn delete(&mut self, id: &str) -> StorageResult<()> {
            let path = self.directory.join(id);
            if path.exists() {
                fs::remove_file(path).map_err(|e| StorageError(e.to_string()))?;
            }
            Ok(())
        }

        fn label(&self) -> String {
            self.directory.display().to_string()
        }
    }

    /// `exercises.json` and `cycles.json` sit one level above the session
    /// folder, which is the repository layout: `<repo>/data/*.json` beside
    /// `<repo>/*.json`.
    pub struct FileReferenceStore {
        directory: PathBuf,
    }

    impl ReferenceStore for FileReferenceStore {
        fn can_write(&self) -> bool {
            self.directory.is_dir()
        }

        fn read(&self, name: &str) -> Option<String> {
            fs::read_to_string(self.directory.join(name)).ok()
        }

        fn write(&self, name: &str, contents: &str) -> Result<(), String> {
            let path = self.directory.join(name);
            let temporary = path.with_extension("json.tmp");
            fs::write(&temporary, contents).map_err(|e| e.to_string())?;
            fs::rename(temporary, path).map_err(|e| e.to_string())
        }
    }

    fn folder_at(directory: PathBuf) -> SessionFolder {
        let parent = directory
            .parent()
            .map_or_else(|| directory.clone(), Path::to_path_buf);
        SessionFolder {
            storage: Box::new(DirectoryStorage {
                directory: directory.clone(),
            }),
            references: Box::new(FileReferenceStore { directory: parent }),
            can_choose_folder: true,
            is_working_copy: false,
        }
    }

    pub fn open_default_folder() -> SessionFolder {
        folder_at(default_directory())
    }

    fn open_folder(directory: PathBuf) -> SessionFolder {
        remember(&directory);
        folder_at(directory)
    }

    pub fn choose_folder() -> Option<SessionFolder> {
        rfd::FileDialog::new()
            .set_title("Choose session folder")
            .pick_folder()
            .map(open_folder)
    }

    pub fn import_files() -> Vec<(String, String)> {
        rfd::FileDialog::new()
            .set_title("Import session files")
            .add_filter("session", &["json"])
            .pick_files()
            .unwrap_or_default()
            .into_iter()
            .filter_map(|path| {
                let name = path.file_name()?.to_string_lossy().into_owned();
                Some((name, fs::read_to_string(&path).ok()?))
            })
            .collect()
    }

    pub fn export_files(files: &[(String, String)]) -> Option<String> {
        let destination = rfd::FileDialog::new()
            .set_title("Export sessions to folder")
            .pick_folder()?;
        for (name, contents) in files {
            let _ = fs::write(destination.join(name), contents);
        }
        Some(format!(
            "{} ({} file(s))",
            destination.display(),
            files.len()
        ))
    }

    /// `$WORKOUTLOG_DATA`, else the last folder chosen, else `<cwd>/../data`,
    /// else the documents directory (ADR-007).
    fn default_directory() -> PathBuf {
        if let Some(from_environment) = std::env::var("WORKOUTLOG_DATA")
            .ok()
            .filter(|v| !v.is_empty())
        {
            return PathBuf::from(expand_tilde(&from_environment));
        }
        if let Some(remembered) = recall().filter(|path| path.is_dir()) {
            return remembered;
        }
        let sibling = std::env::current_dir()
            .unwrap_or_default()
            .join("..")
            .join("data");
        if sibling.is_dir() {
            return sibling;
        }
        dirs::document_dir()
            .unwrap_or_else(|| PathBuf::from("."))
            .join("WorkoutLog")
    }

    fn expand_tilde(path: &str) -> String {
        match path.strip_prefix("~/") {
            Some(rest) => dirs::home_dir()
                .map(|home| home.join(rest).display().to_string())
                .unwrap_or_else(|| path.to_string()),
            None => path.to_string(),
        }
    }

    fn settings_file() -> Option<PathBuf> {
        Some(dirs::config_dir()?.join("workout-log").join("folder"))
    }

    fn remember(directory: &Path) {
        if let Some(path) = settings_file() {
            let _ = fs::create_dir_all(path.parent().expect("a parent"));
            let _ = fs::write(path, directory.display().to_string());
        }
    }

    fn recall() -> Option<PathBuf> {
        fs::read_to_string(settings_file()?)
            .ok()
            .map(|text| PathBuf::from(text.trim()))
    }
}

#[cfg(feature = "desktop")]
pub use native::{choose_folder, export_files, import_files, open_default_folder};

/// ADR-007: a browser has no folder to point at, so it holds a working copy and
/// the user's folder stays canonical through import and export.
#[cfg(not(feature = "desktop"))]
pub fn open_default_folder() -> SessionFolder {
    use workout_log_core::io::session_storage::MemoryStorage;

    SessionFolder {
        storage: Box::new(MemoryStorage::new("browser working copy")),
        references: Box::new(BundledReferenceStore),
        can_choose_folder: false,
        is_working_copy: true,
    }
}

#[cfg(not(feature = "desktop"))]
pub fn choose_folder() -> Option<SessionFolder> {
    None
}

#[cfg(not(feature = "desktop"))]
pub fn import_files() -> Vec<(String, String)> {
    Vec::new()
}

#[cfg(not(feature = "desktop"))]
pub fn export_files(_files: &[(String, String)]) -> Option<String> {
    None
}
