using S2S.Application;

namespace S2S.Tally;

/// <summary>
/// The units-produced tally — per player, how many of each unit type the player has built. Consumes
/// <see cref="IUnitProduced"/> events (derived from state for the dry-calc — the snapshot's unit counts). Units are
/// leaf actions (enabler.md §6) so they leave the model once built, but their COUNT is tallied here for the
/// required side (unit-count thresholds).
/// </summary>
public sealed class UnitTally : IUnitProduced
{
    private readonly Dictionary<int, Dictionary<string, int>> _units = new();

    public void UnitProduced(int playerId, string unit)
    {
        if (!_units.TryGetValue(playerId, out Dictionary<string, int>? counts))
            _units[playerId] = counts = new(StringComparer.Ordinal);
        counts[unit] = counts.GetValueOrDefault(unit) + 1;
    }

    /// <summary>How many of this unit type the player has produced (0 if none).</summary>
    public int Count(int playerId, string unit)
        => _units.TryGetValue(playerId, out Dictionary<string, int>? c) ? c.GetValueOrDefault(unit) : 0;
}
