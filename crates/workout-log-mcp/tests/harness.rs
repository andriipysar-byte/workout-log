//! A temp workspace seeded from the *real* reference files: a fixture copy
//! would only test the copy. The session folder is always temporary, so no test
//! can write into the real `data/`.

use std::fs;
use std::path::{Path, PathBuf};

use serde_json::{Value, json};
use tempfile::TempDir;
use workout_log_mcp::workspace::Workspace;

pub struct TestRepo {
    pub directory: TempDir,
}

pub fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .expect("a repo root holding cycles.json")
        .to_path_buf()
}

impl Default for TestRepo {
    fn default() -> Self {
        Self::new()
    }
}

impl TestRepo {
    pub fn new() -> Self {
        let directory = TempDir::new().expect("a temp dir");
        let root = repo_root();
        fs::create_dir_all(directory.path().join("data")).expect("data/");
        for name in ["exercises.json", "cycles.json"] {
            fs::copy(root.join(name), directory.path().join(name)).expect("copy reference file");
        }
        Self { directory }
    }

    pub fn path(&self) -> &Path {
        self.directory.path()
    }

    pub fn workspace(&self) -> Workspace {
        Workspace::open(Some(self.path().to_path_buf()), None, None)
    }

    pub fn session_file(&self, id: &str) -> PathBuf {
        self.path().join("data").join(id)
    }

    pub fn write_session(&self, id: &str, value: &Value) {
        fs::write(
            self.session_file(id),
            serde_json::to_string_pretty(value).expect("encodable"),
        )
        .expect("write session");
    }

    pub fn write_raw(&self, id: &str, contents: &str) {
        fs::write(self.session_file(id), contents).expect("write raw");
    }

    pub fn read_session(&self, id: &str) -> String {
        fs::read_to_string(self.session_file(id)).expect("readable session")
    }

    pub fn session_ids(&self) -> Vec<String> {
        let mut ids: Vec<String> = fs::read_dir(self.path().join("data"))
            .expect("data/")
            .filter_map(Result::ok)
            .map(|e| e.file_name().to_string_lossy().into_owned())
            .collect();
        ids.sort();
        ids
    }
}

pub fn strength(exercise: &str, sets: Value) -> Value {
    json!({"type": "strength", "exercise": exercise, "sets": sets})
}

pub fn session(date: &str, cycle_day: &str, blocks: Value) -> Value {
    json!({"date": date, "cycle_day": cycle_day, "kind": "training", "blocks": blocks})
}
