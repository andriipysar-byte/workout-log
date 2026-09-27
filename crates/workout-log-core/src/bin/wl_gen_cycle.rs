//! Generates session stubs from a cycle template in `cycles.json`. Existing
//! files are left alone unless `--force` is given.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use workout_log_core::CycleCatalogue;
use workout_log_core::cycles::cycle_generator::generate;
use workout_log_core::io::session_store::SessionStore;

const USAGE: &str = "usage: wl_gen_cycle [--cycle <id>] [--repo <dir>] [--out <dir>] [--force]";

fn main() -> ExitCode {
    let (mut cycle_id, mut repo, mut out, mut force) =
        (String::from("hybrid-8"), None, None, false);
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        let mut value = || {
            arguments
                .next()
                .ok_or(format!("missing value for {argument}"))
        };
        let parsed = match argument.as_str() {
            "--force" => {
                force = true;
                Ok(())
            }
            "--help" | "-h" => {
                println!("{USAGE}");
                return ExitCode::SUCCESS;
            }
            "--cycle" => value().map(|v| cycle_id = v),
            "--repo" => value().map(|v| repo = Some(PathBuf::from(v))),
            "--out" => value().map(|v| out = Some(PathBuf::from(v))),
            other => Err(format!("unknown argument \"{other}\"\n{USAGE}")),
        };
        if let Err(message) = parsed {
            eprintln!("error: {message}");
            return ExitCode::from(2);
        }
    }

    let Some(root) = repo.or_else(repo_root) else {
        eprintln!("error: no repository found — pass --repo");
        return ExitCode::from(2);
    };
    let out = out.unwrap_or_else(|| root.join("data"));

    let text = match fs::read_to_string(root.join("cycles.json")) {
        Ok(text) => text,
        Err(error) => {
            eprintln!("error: {}/cycles.json: {error}", root.display());
            return ExitCode::from(2);
        }
    };
    let catalogue: CycleCatalogue = match serde_json::from_str(&text) {
        Ok(catalogue) => catalogue,
        Err(error) => {
            eprintln!("error: cycles.json: {error}");
            return ExitCode::from(2);
        }
    };
    let Some(cycle) = catalogue.by_id(&cycle_id) else {
        let ids: Vec<&str> = catalogue.cycles.iter().map(|c| c.id.as_str()).collect();
        eprintln!("error: no cycle \"{cycle_id}\" — have {}", ids.join(", "));
        return ExitCode::from(2);
    };

    let sessions = match generate(cycle) {
        Ok(sessions) => sessions,
        Err(error) => {
            eprintln!("error: {error}");
            return ExitCode::from(2);
        }
    };
    if let Err(error) = fs::create_dir_all(&out) {
        eprintln!("error: {}: {error}", out.display());
        return ExitCode::from(2);
    }

    for session in &sessions {
        let id = SessionStore::id_for(session);
        let path = out.join(&id);
        if path.exists() && !force {
            println!("skipped {id} (exists — pass --force to overwrite)");
            continue;
        }
        if let Err(error) = fs::write(&path, SessionStore::encode(session)) {
            eprintln!("error: {}: {error}", path.display());
            return ExitCode::from(2);
        }
        println!("wrote {id}");
    }
    ExitCode::SUCCESS
}

fn repo_root() -> Option<PathBuf> {
    std::env::current_dir()
        .ok()?
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .map(Path::to_path_buf)
}
