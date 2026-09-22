//! MCP server over the workout log, speaking the protocol on stdio.
//!
//! stdout carries the protocol and nothing else; every diagnostic goes to
//! stderr, or it would corrupt the stream.

use std::path::PathBuf;
use std::process::ExitCode;

use rmcp::ServiceExt;
use rmcp::transport::stdio;
use workout_log_mcp::server::WorkoutLogServer;
use workout_log_mcp::workspace::Workspace;

const USAGE: &str = "\
usage: workout-log-mcp [--repo <dir>] [--data <dir>]

  --repo <dir>  repository holding exercises.json and cycles.json
  --data <dir>  session folder (default: <repo>/data, or $WORKOUTLOG_DATA)
";

struct Options {
    repo: Option<PathBuf>,
    data: Option<PathBuf>,
}

fn parse_arguments() -> Result<Option<Options>, String> {
    let mut options = Options {
        repo: None,
        data: None,
    };
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        let mut value = || {
            arguments
                .next()
                .ok_or_else(|| format!("missing value for {argument}"))
                .map(PathBuf::from)
        };
        match argument.as_str() {
            "--help" | "-h" => return Ok(None),
            "--repo" => options.repo = Some(value()?),
            "--data" => options.data = Some(value()?),
            other => return Err(format!("unknown argument \"{other}\"\n{USAGE}")),
        }
    }
    Ok(Some(options))
}

#[tokio::main]
async fn main() -> ExitCode {
    let options = match parse_arguments() {
        Ok(Some(options)) => options,
        Ok(None) => {
            print!("{USAGE}");
            return ExitCode::SUCCESS;
        }
        Err(message) => {
            eprintln!("error: {message}");
            return ExitCode::from(2);
        }
    };

    let workspace = Workspace::open(
        options.repo,
        options.data,
        std::env::var("WORKOUTLOG_DATA").ok(),
    );
    if !workspace.cycles_file().is_file() {
        eprintln!(
            "warning: no cycles.json under {} — pass --repo",
            workspace.root.display()
        );
    }
    eprintln!("workout-log: sessions in {}", workspace.label());

    match WorkoutLogServer::new(workspace).serve(stdio()).await {
        Ok(service) => {
            if let Err(error) = service.waiting().await {
                eprintln!("error: {error}");
                return ExitCode::FAILURE;
            }
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!("error: {error}");
            ExitCode::FAILURE
        }
    }
}
