//! The single observable store. Flutter's `ChangeNotifier` notified every
//! listener on every change; one signal holding the whole state is the same
//! contract, and this app is small enough that it costs nothing.

use std::collections::BTreeMap;

use workout_log_core::analytics::muscle_activation::{MuscleActivation, Scores, WeightingMode};
use workout_log_core::analytics::muscle_group::MuscleGroup;
use workout_log_core::analytics::muscle_map_svg::colorize;
use workout_log_core::cycles::cycle_day::{CycleDay, DayKind};
use workout_log_core::cycles::{cycle_generator, cycle_templates};
use workout_log_core::io::session_store::SessionStore;
use workout_log_core::{Catalogue, Cycle, CycleCatalogue, CycleSession, Session};

use crate::platform::{self, BUNDLED_CATALOGUE, BUNDLED_CYCLES, MAP_TEMPLATE, ReferenceStore};

#[derive(Debug, Clone, PartialEq)]
pub struct DayInfo {
    pub id: String,
    pub cycle_day: String,
    pub group: Option<MuscleGroup>,
}

/// Exercises down the side, cycle days across the top, presence in the cells.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct CycleMatrix {
    pub days: Vec<String>,
    pub exercises: Vec<String>,
    pub cells: Vec<Vec<bool>>,
    pub groups: Vec<Vec<MuscleGroup>>,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Tab {
    List,
    Cycle,
    Plan,
}

pub struct AppState {
    store: SessionStore,
    references: Box<dyn ReferenceStore>,
    pub can_choose_folder: bool,
    pub is_working_copy: bool,

    pub catalogue: Option<Catalogue>,
    pub cycles: Option<CycleCatalogue>,

    pub files: Vec<String>,
    pub selection: Option<String>,
    pub session: Option<Session>,
    pub status: String,
    pub mode: WeightingMode,
    pub calendar: BTreeMap<String, DayInfo>,
    pub cycle: CycleMatrix,
    pub cycle_sessions: Vec<Session>,

    /// The id the open session came from, so editing the header *moves* the
    /// file instead of orphaning it.
    loaded_id: Option<String>,
    decoded: BTreeMap<String, Option<Session>>,
}

const ACTIVATION: MuscleActivation = MuscleActivation {
    primary_weight: 1.0,
    secondary_weight: 0.5,
};

impl AppState {
    pub fn start() -> Self {
        let folder = platform::open_default_folder();
        let mut state = Self {
            store: SessionStore::new(folder.storage),
            references: folder.references,
            can_choose_folder: folder.can_choose_folder,
            is_working_copy: folder.is_working_copy,
            catalogue: None,
            cycles: None,
            files: Vec::new(),
            selection: None,
            session: None,
            status: String::new(),
            mode: WeightingMode::SetCount,
            calendar: BTreeMap::new(),
            cycle: CycleMatrix::default(),
            cycle_sessions: Vec::new(),
            loaded_id: None,
            decoded: BTreeMap::new(),
        };
        state.load_reference_files();
        state.refresh();
        state
    }

    pub fn folder_label(&self) -> String {
        self.store.storage.label()
    }

    pub fn can_edit_plan(&self) -> bool {
        self.references.can_write()
    }

    /// The folder's own copy wins; the bundled copy is the fallback, so the app
    /// still has a catalogue when pointed at a bare folder of sessions.
    fn load_reference_files(&mut self) {
        let catalogue = self
            .references
            .read("exercises.json")
            .unwrap_or_else(|| BUNDLED_CATALOGUE.to_string());
        let cycles = self
            .references
            .read("cycles.json")
            .unwrap_or_else(|| BUNDLED_CYCLES.to_string());
        self.catalogue = serde_json::from_str(&catalogue).ok();
        self.cycles = serde_json::from_str(&cycles).ok();
        if self.catalogue.is_none() || self.cycles.is_none() {
            self.status = "Catalogue load failed".into();
        }
    }

    /// An unreadable folder must never crash the app: it reports and carries on.
    pub fn refresh(&mut self) {
        match self.store.list_ids() {
            Ok(ids) => {
                self.decoded.retain(|id, _| ids.contains(id));
                for id in &ids {
                    if !self.decoded.contains_key(id) {
                        let session = self.store.load(id).ok();
                        self.decoded.insert(id.clone(), session);
                    }
                }
                self.files = ids;
            }
            Err(error) => {
                self.files.clear();
                self.status = format!("Cannot read {}: {error}", self.folder_label());
            }
        }
        self.rebuild_derived();
    }

    fn rebuild_derived(&mut self) {
        self.calendar.clear();
        let mut latest: BTreeMap<String, Session> = BTreeMap::new();
        for (id, decoded) in &self.decoded {
            let Some(session) = decoded else { continue };
            self.calendar.insert(
                session.date.clone(),
                DayInfo {
                    id: id.clone(),
                    cycle_day: session.cycle_day.clone(),
                    group: self.dominant_group(session),
                },
            );
            latest.insert(session.cycle_day.clone(), session.clone());
        }
        self.cycle_sessions = latest.into_values().collect();
        self.cycle = self.build_matrix();
    }

    fn dominant_group(&self, session: &Session) -> Option<MuscleGroup> {
        let catalogue = self.catalogue.as_ref()?;
        MuscleGroup::dominant(&ACTIVATION.for_session(session, catalogue, WeightingMode::SetCount))
    }

    fn build_matrix(&self) -> CycleMatrix {
        let mut days: Vec<String> = self
            .cycle_sessions
            .iter()
            .map(|s| s.cycle_day.clone())
            .collect();
        days.sort();
        days.dedup();

        let mut exercises: Vec<String> = Vec::new();
        let mut per_day: Vec<Vec<String>> = Vec::new();
        for day in &days {
            let names: Vec<String> = self
                .cycle_sessions
                .iter()
                .filter(|s| &s.cycle_day == day)
                .flat_map(session_exercise_names)
                .collect();
            for name in &names {
                if !exercises.contains(name) {
                    exercises.push(name.clone());
                }
            }
            per_day.push(names);
        }

        let cells = exercises
            .iter()
            .map(|exercise| {
                per_day
                    .iter()
                    .map(|names| names.contains(exercise))
                    .collect()
            })
            .collect();
        let groups = exercises
            .iter()
            .map(|exercise| self.primary_groups(exercise))
            .collect();

        CycleMatrix {
            days,
            exercises,
            cells,
            groups,
        }
    }

    pub fn primary_groups(&self, name: &str) -> Vec<MuscleGroup> {
        let Some(exercise) = self.catalogue.as_ref().and_then(|c| c.resolve(name)) else {
            return Vec::new();
        };
        let mut groups = Vec::new();
        for muscle in &exercise.primary_muscles {
            if let Some(group) = MuscleGroup::of(muscle)
                && !groups.contains(&group)
            {
                groups.push(group);
            }
        }
        groups
    }

    // ------------------------------------------------------------- muscle maps

    fn map(&self, scores: &Scores) -> String {
        colorize(MAP_TEMPLATE, scores)
    }

    pub fn day_map(&self) -> Option<String> {
        let (session, catalogue) = (self.session.as_ref()?, self.catalogue.as_ref()?);
        Some(self.map(&ACTIVATION.for_session(session, catalogue, self.mode)))
    }

    pub fn cycle_map(&self) -> Option<String> {
        let catalogue = self.catalogue.as_ref()?;
        if self.cycle_sessions.is_empty() {
            return None;
        }
        Some(self.map(&ACTIVATION.for_sessions(&self.cycle_sessions, catalogue, self.mode)))
    }

    pub fn exercise_map(&self, name: &str) -> Option<String> {
        let exercise = self.catalogue.as_ref()?.resolve(name)?;
        Some(self.map(&ACTIVATION.for_exercise(exercise)))
    }

    /// A plan has no weights, so it is always scored by set count.
    pub fn plan_map(&self, workout: &CycleSession) -> Option<String> {
        let catalogue = self.catalogue.as_ref()?;
        let preview = cycle_generator::preview(workout, None);
        Some(self.map(&ACTIVATION.for_session(&preview, catalogue, WeightingMode::SetCount)))
    }

    pub fn plan_cycle_map(&self, cycle: &Cycle) -> Option<String> {
        let catalogue = self.catalogue.as_ref()?;
        let previews: Vec<Session> = cycle
            .sessions
            .iter()
            .map(|s| cycle_generator::preview(s, None))
            .collect();
        if previews.is_empty() {
            return None;
        }
        Some(self.map(&ACTIVATION.for_sessions(&previews, catalogue, WeightingMode::SetCount)))
    }

    pub fn plan_dominant_group(&self, workout: &CycleSession) -> Option<MuscleGroup> {
        let catalogue = self.catalogue.as_ref()?;
        let preview = cycle_generator::preview(workout, None);
        MuscleGroup::dominant(&ACTIVATION.for_session(&preview, catalogue, WeightingMode::SetCount))
    }

    // -------------------------------------------------------------- mutations

    pub fn set_mode(&mut self, mode: WeightingMode) {
        self.mode = mode;
    }

    pub fn open(&mut self, id: &str) {
        match self.store.load(id) {
            Ok(session) => {
                self.session = Some(session);
                self.selection = Some(id.to_string());
                self.loaded_id = Some(id.to_string());
                self.status = format!("Loaded {id}");
            }
            Err(error) => self.status = format!("Load failed: {error}"),
        }
    }

    pub fn save(&mut self) {
        let Some(session) = self.session.clone() else {
            return;
        };
        let previous = self.loaded_id.clone();
        match self.store.save(&session, previous.as_deref()) {
            Ok(id) => {
                self.status = match &previous {
                    Some(from) if from != &id => format!("Renamed {from} → {id}"),
                    _ => format!("Saved {id}"),
                };
                if let Some(from) = previous {
                    self.decoded.remove(&from);
                }
                self.decoded.remove(&id);
                self.selection = Some(id.clone());
                self.loaded_id = Some(id);
                self.refresh();
            }
            Err(error) => self.status = format!("Save failed: {error}"),
        }
    }

    pub fn create(&mut self, session: Session) {
        match self.store.save(&session, None) {
            Ok(id) => {
                self.status = format!("Created {id}");
                self.decoded.remove(&id);
                self.session = Some(session);
                self.selection = Some(id.clone());
                self.loaded_id = Some(id);
                self.refresh();
            }
            Err(error) => self.status = format!("Create failed: {error}"),
        }
    }

    pub fn delete(&mut self, id: &str) {
        match self.store.delete(id) {
            Ok(()) => {
                self.status = format!("Deleted {id}");
                self.decoded.remove(id);
                if self.selection.as_deref() == Some(id) {
                    self.selection = None;
                    self.session = None;
                    self.loaded_id = None;
                }
                self.refresh();
            }
            Err(error) => self.status = format!("Delete failed: {error}"),
        }
    }

    pub fn exists(&self, id: &str) -> bool {
        self.store.exists(id).unwrap_or(false)
    }

    // ------------------------------------------------------------- plan writes

    pub fn cycle_id_taken(&self, id: &str) -> bool {
        self.cycles.as_ref().is_some_and(|c| {
            c.cycles
                .iter()
                .any(|cycle| cycle.id.to_lowercase() == id.trim().to_lowercase())
        })
    }

    /// The only write of `cycles.json`; everything else in the Plan tab is held
    /// in memory until this runs.
    pub fn save_cycles(&mut self) {
        let Some(cycles) = &self.cycles else { return };
        let text = workout_log_core::coding::encode_json(
            &serde_json::to_value(cycles).expect("encodable"),
        );
        self.status = match self.references.write("cycles.json", &text) {
            Ok(()) => "Saved cycles.json".into(),
            Err(error) => format!("Save failed: {error}"),
        };
    }

    pub fn add_exercise(&mut self, exercise: workout_log_core::Exercise) {
        let Some(catalogue) = &self.catalogue else {
            return;
        };
        let name = exercise.name.clone();
        let updated = catalogue.with_exercise(exercise);
        let text = workout_log_core::coding::encode_json(
            &serde_json::to_value(&updated).expect("encodable"),
        );
        match self.references.write("exercises.json", &text) {
            Ok(()) => {
                self.catalogue = Some(updated);
                self.status = format!("Added \"{name}\" to the catalogue");
                self.rebuild_derived();
            }
            Err(error) => self.status = format!("Catalogue save failed: {error}"),
        }
    }

    /// The next code in the A1/A2 progression, with the skeleton that day type
    /// starts from and the weekday its slot lands on.
    pub fn add_workout(&mut self, cycle_index: usize) {
        let Some(cycles) = &mut self.cycles else {
            return;
        };
        let Some(cycle) = cycles.cycles.get_mut(cycle_index) else {
            return;
        };
        let codes: Vec<String> = cycle.sessions.iter().map(|s| s.cycle_day.clone()).collect();
        let day = cycle_templates::next_day(&codes);
        let index = cycle.sessions.len();
        let week = (index / cycle.training_days.len().max(1)) as i64 + 1;
        let weekday = weekday_for(cycle, index);
        cycle
            .sessions
            .push(cycle_templates::session(day, Some(week), weekday));
        self.status = format!("Added {}", day.code());
    }

    pub fn remove_workout(&mut self, cycle_index: usize, workout_index: usize) {
        let Some(cycle) = self.cycle_mut(cycle_index) else {
            return;
        };
        if workout_index < cycle.sessions.len() {
            let code = cycle.sessions.remove(workout_index).cycle_day;
            self.status = format!("Removed {code}");
            self.resync_weekdays(cycle_index);
        }
    }

    pub fn move_workout(&mut self, cycle_index: usize, from: usize, to: usize) {
        let Some(cycle) = self.cycle_mut(cycle_index) else {
            return;
        };
        if from < cycle.sessions.len() && to < cycle.sessions.len() {
            let workout = cycle.sessions.remove(from);
            cycle.sessions.insert(to, workout);
            self.resync_weekdays(cycle_index);
        }
    }

    pub fn retitle_workout(&mut self, cycle_index: usize, workout_index: usize, day: CycleDay) {
        let Some(cycle) = self.cycle_mut(cycle_index) else {
            return;
        };
        if let Some(workout) = cycle.sessions.get_mut(workout_index) {
            workout.cycle_day = day.code();
            workout.session_type = Some(
                match day.kind {
                    DayKind::Conditioning => "metcon",
                    DayKind::Heavy => "heavy",
                }
                .into(),
            );
        }
    }

    /// Every workout's stored weekday is re-derived from its position, or the
    /// generator would refuse the cycle on the next expansion.
    fn resync_weekdays(&mut self, cycle_index: usize) {
        let Some(cycles) = &mut self.cycles else {
            return;
        };
        let Some(cycle) = cycles.cycles.get_mut(cycle_index) else {
            return;
        };
        let snapshot = cycle.clone();
        for (index, workout) in cycle.sessions.iter_mut().enumerate() {
            workout.weekday = weekday_for(&snapshot, index);
        }
    }

    pub fn planned_date(&self, cycle: &Cycle, index: usize) -> Option<String> {
        let start = chrono::NaiveDate::parse_from_str(&cycle.start_date, "%Y-%m-%d").ok()?;
        let dates = cycle_generator::training_dates(start, &cycle.training_days, index + 1).ok()?;
        dates.get(index).map(|d| cycle_generator::iso_date(*d))
    }

    pub fn cycle_mut(&mut self, index: usize) -> Option<&mut Cycle> {
        self.cycles.as_mut()?.cycles.get_mut(index)
    }

    /// Point the app at a different folder: new store, new reference files,
    /// and nothing carried over from the old one.
    pub fn adopt(&mut self, folder: crate::platform::SessionFolder) {
        self.store = SessionStore::new(folder.storage);
        self.references = folder.references;
        self.can_choose_folder = folder.can_choose_folder;
        self.is_working_copy = folder.is_working_copy;
        self.decoded.clear();
        self.session = None;
        self.selection = None;
        self.loaded_id = None;
        self.load_reference_files();
        self.refresh();
        self.status = format!("Folder: {}", self.folder_label());
    }

    /// Decoded before it is written, so a malformed file is rejected at the door
    /// rather than landing in the archive.
    pub fn import(&mut self, files: Vec<(String, String)>) {
        let total = files.len();
        let mut written = 0;
        for (name, contents) in files {
            match SessionStore::decode(&contents) {
                Ok(session) => match self.store.save(&session, None) {
                    Ok(_) => written += 1,
                    Err(error) => self.status = format!("Skipped {name}: {error}"),
                },
                Err(error) => self.status = format!("Skipped {name}: {error}"),
            }
        }
        self.decoded.clear();
        self.refresh();
        self.status = format!("Imported {written} of {total} file(s)");
    }

    pub fn export(&mut self) {
        let files: Vec<(String, String)> = self
            .files
            .iter()
            .filter_map(|id| Some((id.clone(), self.raw(id)?)))
            .collect();
        if files.is_empty() {
            self.status = "Nothing to export".into();
            return;
        }
        self.status = match crate::platform::export_files(&files) {
            Some(destination) => format!("Exported to {destination}"),
            None => "Export cancelled".into(),
        };
    }

    pub fn raw(&self, id: &str) -> Option<String> {
        self.store.storage.read(id).ok()
    }
}

fn weekday_for(cycle: &Cycle, index: usize) -> Option<String> {
    let start = chrono::NaiveDate::parse_from_str(&cycle.start_date, "%Y-%m-%d").ok()?;
    let dates = cycle_generator::training_dates(start, &cycle.training_days, index + 1).ok()?;
    let date = dates.get(index)?;
    use chrono::Datelike;
    Some(
        cycle_generator::WEEKDAY_ABBREVIATIONS[date.weekday().num_days_from_monday() as usize]
            .to_string(),
    )
}

pub fn session_exercise_names(session: &Session) -> Vec<String> {
    use workout_log_core::models::block::Block;
    session
        .blocks
        .iter()
        .flat_map(|block| match block {
            Block::Strength(s) => vec![s.exercise.clone()],
            Block::Metcon(m) => m.exercises.iter().map(|e| e.name.clone()).collect(),
            _ => Vec::new(),
        })
        .filter(|name| !name.trim().is_empty())
        .collect()
}
