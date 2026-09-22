//! The MCP surface over the log. Every rule it applies lives in
//! `workout-log-core`; this layer parses arguments, calls the core and formats
//! JSON back (ADR-004 again, with an LLM as the UI).

use std::sync::Arc;

use rmcp::handler::server::tool::ToolRouter;
use rmcp::handler::server::wrapper::Parameters;
use rmcp::model::{
    CallToolResult, Content, Implementation, ProtocolVersion, ServerCapabilities, ServerInfo,
};
use rmcp::{ErrorData as McpError, ServerHandler, tool, tool_handler, tool_router};
use serde_json::Value;
use tokio::sync::Mutex;

use crate::arguments::ToolResult;
use crate::requests::*;
use crate::tools::{self, tool_json};
use crate::workspace::Workspace;

const INSTRUCTIONS: &str = "\
A hybrid strength + conditioning training log: one JSON file per session in a
folder, which is the source of truth (ADR-001). Exercise names are Ukrainian and
resolve through exercises.json, which also carries aliases, movement patterns and
muscle targets; cycles.json holds the reusable session templates. A session is
identified by its file name, `YYYY-MM-DD_<cycle day>.json`, where the cycle day
is A1…F2 — the letter groups workouts by emphasis, 1 is a conditioning day and 2
is a heavy day.

Sets can be written in the log's own notation rather than as objects:
`6 × [70, 80, 90, 100, 110]` is five sets of six at ascending weights,
`6 × [70, 80] + 6 × [30]` makes the trailing group back-off sets, `90(4)` is a
per-set rep override, `5+5+4+3+3 (20)` is a cluster, and `54c` is a 54-second
hold. Use log_sets for that; use write_session for anything the notation cannot
express.

The analysis tools speak in the training principles the log is built on
(docs/01-training-principles.md): rep bands are independent progress tracks (P6),
the top set is the signal and back-offs are volume (P2), explosive lifts belong
at 2-3 reps (P3), and a block where volume slots take over is what produced an
involuntary deload (P5). The tools report; the programming decisions are the
athlete's.
";

/// Success is one text block of pretty JSON; failure is one text block of plain
/// prose. A refusal is never a protocol error — the model reads the message and
/// corrects itself.
fn respond(result: ToolResult<Value>) -> CallToolResult {
    match result {
        Ok(value) => CallToolResult::success(vec![Content::text(tool_json(&value))]),
        Err(failure) => CallToolResult::error(vec![Content::text(failure.0)]),
    }
}

#[derive(Clone)]
pub struct WorkoutLogServer {
    workspace: Arc<Mutex<Workspace>>,
    tool_router: ToolRouter<Self>,
}

#[tool_router]
impl WorkoutLogServer {
    pub fn new(workspace: Workspace) -> Self {
        Self {
            workspace: Arc::new(Mutex::new(workspace)),
            tool_router: Self::tool_router(),
        }
    }

    #[tool(
        description = "List logged sessions, newest last, with a one-line summary of each. Filter by date range, cycle day, kind or exercise."
    )]
    async fn list_sessions(
        &self,
        Parameters(args): Parameters<ListSessions>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::list_sessions(&workspace, &args)))
    }

    #[tool(
        description = "The full JSON of one session as it is on disk, with any validation issues it has."
    )]
    async fn read_session(
        &self,
        Parameters(args): Parameters<Selector>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::read_session(&workspace, &args)))
    }

    #[tool(
        description = "Write a new session file. When a cycle in cycles.json defines that cycle day, the session starts from its template — the planned blocks, exercises and rep schemes — otherwise it is empty. Fails if the file exists unless overwrite is set."
    )]
    async fn create_session(
        &self,
        Parameters(args): Parameters<CreateSession>,
    ) -> Result<CallToolResult, McpError> {
        let mut workspace = self.workspace.lock().await;
        Ok(respond(tools::create_session(&mut workspace, &args)))
    }

    #[tool(
        description = "Replace a session with the document given — the general edit: read_session, change the JSON, write it back. Moving the date or cycle day renames the file. Refuses documents with errors; warnings come back with the result."
    )]
    async fn write_session(
        &self,
        Parameters(args): Parameters<WriteSession>,
    ) -> Result<CallToolResult, McpError> {
        let mut workspace = self.workspace.lock().await;
        Ok(respond(tools::write_session(&mut workspace, &args)))
    }

    #[tool(
        description = "Fill a strength slot from the log notation, e.g. \"6 × [70, 80, 90, 100, 110]\" or \"5+5+4+3+3 (20)\". Names the slot by exercise or by block index."
    )]
    async fn log_sets(
        &self,
        Parameters(args): Parameters<LogSets>,
    ) -> Result<CallToolResult, McpError> {
        let mut workspace = self.workspace.lock().await;
        Ok(respond(tools::log_sets(&mut workspace, &args)))
    }

    #[tool(
        description = "Delete a session file. The folder is the only copy, so this requires confirm: true."
    )]
    async fn delete_session(
        &self,
        Parameters(args): Parameters<DeleteSession>,
    ) -> Result<CallToolResult, McpError> {
        let mut workspace = self.workspace.lock().await;
        Ok(respond(tools::delete_session(&mut workspace, &args)))
    }

    #[tool(
        description = "Turn a notation line into sets without writing anything — check what a line means before logging it."
    )]
    async fn parse_notation(
        &self,
        Parameters(args): Parameters<ParseNotation>,
    ) -> Result<CallToolResult, McpError> {
        Ok(respond(tools::parse_notation(&args)))
    }

    #[tool(
        description = "The exercise catalogue: canonical Ukrainian names, aliases, movement pattern, modality, category and muscle targets. Always returns the muscle vocabulary the catalogue uses."
    )]
    async fn list_exercises(
        &self,
        Parameters(args): Parameters<ListExercises>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::list_exercises(&workspace, &args)))
    }

    #[tool(
        description = "Add an exercise to exercises.json, or replace the entry with that name. Pattern is the load-bearing field: it groups variants of one movement so rotation reads as progress."
    )]
    async fn add_exercise(
        &self,
        Parameters(args): Parameters<AddExercise>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::add_exercise(&workspace, &args)))
    }

    #[tool(
        description = "The cycle definitions in cycles.json — the plan side: which workouts a cycle holds and what each day is built around."
    )]
    async fn list_cycles(
        &self,
        Parameters(args): Parameters<ListCycles>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::list_cycles(&workspace, &args)))
    }

    #[tool(
        description = "Expand a cycle onto the calendar, writing one session stub per template day. Existing files are skipped unless force is set; dry_run reports what it would write."
    )]
    async fn generate_cycle(
        &self,
        Parameters(args): Parameters<GenerateCycle>,
    ) -> Result<CallToolResult, McpError> {
        let mut workspace = self.workspace.lock().await;
        Ok(respond(tools::generate_cycle(&mut workspace, &args)))
    }

    #[tool(
        description = "One session derived: tonnage, sets by rep band, top sets against back-offs, work-to-clock density from the bracket timestamps, metcon round splits with pace decay and heart rate, and the muscles the work landed on."
    )]
    async fn analyze_session(
        &self,
        Parameters(args): Parameters<AnalyzeSession>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::analyze_session(&workspace, &args)))
    }

    #[tool(
        description = "A stretch of sessions read as a block: rep-band distribution by set and by slot, movement-pattern frequency per cycle, strength tonnage beside conditioning time, metcon time domains, and alerts against the training principles (P3, P5, P8, P10). The safety metric the log exists for."
    )]
    async fn analyze_training(
        &self,
        Parameters(args): Parameters<AnalyzeTraining>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::analyze_training(&workspace, &args)))
    }

    #[tool(
        description = "Best set over time for one lift, one track per rep band, with estimated 1RM and back-off volume reported separately. by_pattern widens it to every variant of the same movement pattern."
    )]
    async fn exercise_progress(
        &self,
        Parameters(args): Parameters<ExerciseProgressArgs>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::exercise_progress(&workspace, &args)))
    }

    #[tool(
        description = "Muscle scores for one session or a whole range, normalized so the hottest muscle is 1.0, plus the coarse group rollup. The three weightings deliberately disagree: sets, reps or tonnage."
    )]
    async fn muscle_activation(
        &self,
        Parameters(args): Parameters<MuscleActivationArgs>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::muscle_activation(&workspace, &args)))
    }

    #[tool(
        description = "Check every session against the schema and against what the log means: malformed headers, overlapping blocks, splits that go backwards, exercises no catalogue entry resolves."
    )]
    async fn validate_archive(
        &self,
        Parameters(args): Parameters<ValidateArchive>,
    ) -> Result<CallToolResult, McpError> {
        let workspace = self.workspace.lock().await;
        Ok(respond(tools::validate_archive(&workspace, &args)))
    }
}

#[tool_handler]
impl ServerHandler for WorkoutLogServer {
    fn get_info(&self) -> ServerInfo {
        ServerInfo {
            protocol_version: ProtocolVersion::LATEST,
            capabilities: ServerCapabilities::builder().enable_tools().build(),
            server_info: Implementation {
                name: "workout-log".into(),
                version: "1.0.0".into(),
                ..Default::default()
            },
            instructions: Some(INSTRUCTIONS.into()),
        }
    }
}
