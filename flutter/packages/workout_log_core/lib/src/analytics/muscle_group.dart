/// Coarse categorical grouping of the fine-grained catalogue muscles, each with a
/// stable colour code. Distinct from the sequential heat-map ramp (`MuscleMapSVG`):
/// this answers "which region" (categorical), not "how hard" (intensity).
enum MuscleGroup {
  chest,
  back,
  shoulders,
  arms,
  legs,
  core;

  String get label => '${name[0].toUpperCase()}${name.substring(1)}';

  String get hex => switch (this) {
        MuscleGroup.chest => '#d1495b',
        MuscleGroup.back => '#00798c',
        MuscleGroup.shoulders => '#edae49',
        MuscleGroup.arms => '#8e5ea2',
        MuscleGroup.legs => '#30638e',
        MuscleGroup.core => '#58a65c',
      };

  static MuscleGroup? of(String muscle) => switch (muscle) {
        'chest' => MuscleGroup.chest,
        'lats' || 'rhomboids' || 'traps' || 'spinal_erectors' => MuscleGroup.back,
        'front_delts' || 'side_delts' || 'rear_delts' => MuscleGroup.shoulders,
        'biceps' || 'triceps' || 'forearms' => MuscleGroup.arms,
        'quads' ||
        'hamstrings' ||
        'glutes' ||
        'calves' ||
        'adductors' ||
        'hip_flexors' =>
          MuscleGroup.legs,
        'abs' || 'obliques' => MuscleGroup.core,
        _ => null,
      };

  /// The group carrying the most activation across a `[muscle: score]` map.
  ///
  /// Ties resolve by declaration order so the calendar badge for a balanced day
  /// does not flicker between runs.
  static MuscleGroup? dominant(Map<String, double> scores) {
    final byGroup = <MuscleGroup, double>{};
    for (final entry in scores.entries) {
      final group = of(entry.key);
      if (group == null) continue;
      byGroup[group] = (byGroup[group] ?? 0) + entry.value;
    }
    MuscleGroup? best;
    var bestScore = double.negativeInfinity;
    for (final group in MuscleGroup.values) {
      final score = byGroup[group];
      if (score != null && score > bestScore) {
        best = group;
        bestScore = score;
      }
    }
    return best;
  }
}
