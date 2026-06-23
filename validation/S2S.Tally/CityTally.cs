using S2S.Application;

namespace S2S.Tally;

/// <summary>
/// The CITY tally — the per-city leaf HAVE. Consumes <see cref="IBuildingCompleted"/> / <see cref="IBonusPresent"/>
/// events into per-city sets of present buildings + present bonuses (the source of truth at city scope). This is
/// what feeds a city's enabler generation (in-city buildings + bonuses are enabling sources); the empire/team/world
/// building counts roll UP from these leaves (enabler.md §4). Nothing reads the live snapshot — the snapshot's
/// per-city state is replayed through the events.
/// </summary>
public sealed class CityTally : IBuildingCompleted, IBonusPresent
{
    private readonly Dictionary<long, HashSet<string>> _buildings = new();
    private readonly Dictionary<long, HashSet<string>> _bonuses = new();

    public void BuildingCompleted(int playerId, long cityId, string building) => Set(_buildings, cityId).Add(building);
    public void BonusPresent(long cityId, string bonus) => Set(_bonuses, cityId).Add(bonus);

    /// <summary>The city's present buildings (a leaf HAVE — an enabling source for its generation).</summary>
    public IReadOnlySet<string> Buildings(long cityId) => _buildings.TryGetValue(cityId, out HashSet<string>? b) ? b : Empty;

    /// <summary>The city's present (connected/vicinity) bonuses.</summary>
    public IReadOnlySet<string> Bonuses(long cityId) => _bonuses.TryGetValue(cityId, out HashSet<string>? b) ? b : Empty;

    private static HashSet<string> Set(Dictionary<long, HashSet<string>> map, long cityId)
        => map.TryGetValue(cityId, out HashSet<string>? s) ? s : map[cityId] = new(StringComparer.Ordinal);

    private static readonly IReadOnlySet<string> Empty = new HashSet<string>(StringComparer.Ordinal);
}
