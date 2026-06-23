using S2S.Application;

namespace S2S.Tally;

/// <summary>
/// The trade-bonus tally — per player, the TRADE-connected resources the player has (empire/team-wide, imitating
/// what trade makes available). Consumes <see cref="IBonusAdded"/> events; fills the resource HAVE the REQUIRES
/// side consults (`requires` connection:trade). Vicinity bonuses are the separate city-scope <see cref="CityTally"/>.
/// In validation the snapshot's trade-connected city bonuses are replayed through it.
/// </summary>
public sealed class BonusTally : IBonusAdded
{
    private readonly Dictionary<int, HashSet<string>> _bonuses = new();

    public void BonusAdded(int playerId, string bonus)
    {
        if (!_bonuses.TryGetValue(playerId, out HashSet<string>? set))
            _bonuses[playerId] = set = new(StringComparer.Ordinal);
        set.Add(bonus);
    }

    /// <summary>The player's trade-connected bonuses (the resource HAVE for `requires` connection:trade).</summary>
    public IReadOnlySet<string> Bonuses(int playerId)
        => _bonuses.TryGetValue(playerId, out HashSet<string>? b) ? b : Empty;

    private static readonly IReadOnlySet<string> Empty = new HashSet<string>(StringComparer.Ordinal);
}
