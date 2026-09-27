//! Normalises session files after hand-editing them: sorted keys, two-space
//! indent, no incidental diffs on the next save. `--check` fails without
//! writing, which is what CI asks.
//!
//! The library stays free of the filesystem (ADR-004); only this binary reads it.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use workout_log_core::coding::{decode_json, encode_json};

fn main() -> ExitCode {
    let mut check = false;
    let mut directory = None;
    for argument in std::env::args().skip(1) {
        match argument.as_str() {
            "--check" => check = true,
            "--help" | "-h" => {
                println!("usage: wl_fmt [--check] [<session dir>]");
                return ExitCode::SUCCESS;
            }
            other if other.starts_with('-') => {
                eprintln!("error: unknown argument \"{other}\"");
                return ExitCode::from(2);
            }
            other => directory = Some(PathBuf::from(other)),
        }
    }

    let directory = match directory.or_else(default_directory) {
        Some(directory) => directory,
        None => {
            eprintln!("error: no session folder found — pass one");
            return ExitCode::from(2);
        }
    };

    let mut files = match session_files(&directory) {
        Ok(files) => files,
        Err(error) => {
            eprintln!("error: {error}");
            return ExitCode::from(2);
        }
    };
    files.sort();

    let mut stale = Vec::new();
    for path in &files {
        let original = match fs::read_to_string(path) {
            Ok(text) => text,
            Err(error) => {
                eprintln!("error: {}: {error}", path.display());
                return ExitCode::from(2);
            }
        };
        let canonical = match decode_json(&original) {
            Ok(value) => encode_json(&value),
            Err(error) => {
                eprintln!("error: {}: {error}", path.display());
                return ExitCode::from(2);
            }
        };
        if canonical == original {
            continue;
        }
        stale.push(path.clone());
        if !check && let Err(error) = fs::write(path, &canonical) {
            eprintln!("error: {}: {error}", path.display());
            return ExitCode::from(2);
        }
    }

    if stale.is_empty() {
        println!("{} file(s) already canonical", files.len());
        return ExitCode::SUCCESS;
    }
    for path in &stale {
        println!(
            "{} {}",
            if check {
                "would reformat"
            } else {
                "reformatted"
            },
            path.display()
        );
    }
    if check {
        ExitCode::FAILURE
    } else {
        ExitCode::SUCCESS
    }
}

fn session_files(directory: &Path) -> std::io::Result<Vec<PathBuf>> {
    Ok(fs::read_dir(directory)?
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.is_file() && path.extension().is_some_and(|e| e == "json"))
        .collect())
}

fn default_directory() -> Option<PathBuf> {
    Some(repo_root()?.join("data"))
}

fn repo_root() -> Option<PathBuf> {
    std::env::current_dir()
        .ok()?
        .ancestors()
        .find(|dir| dir.join("cycles.json").is_file())
        .map(Path::to_path_buf)
}
