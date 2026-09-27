//! WorkoutLog — a Dioxus front end over `workout-log-core`.
//!
//! All domain logic lives in the core (ADR-004); this layer is presentation.

mod platform;
mod state;
mod ui;

fn main() {
    #[cfg(feature = "desktop")]
    {
        use dioxus::desktop::{Config, LogicalSize, WindowBuilder};
        dioxus::LaunchBuilder::desktop()
            .with_cfg(
                Config::new().with_window(
                    WindowBuilder::new()
                        .with_title("WorkoutLog")
                        .with_inner_size(LogicalSize::new(1280.0, 800.0)),
                ),
            )
            .launch(ui::App);
    }

    #[cfg(not(feature = "desktop"))]
    dioxus::launch(ui::App);
}
