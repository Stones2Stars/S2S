using S2S.Application;

namespace S2S.Tally;

/// <summary>
/// The TECH tally. Consumes <see cref="IResearchCompleted"/> events (one per player completing a tech) into a
/// per-PLAYER tech set — the internal SOURCE OF TRUTH — and DERIVES from those, on each event:
/// <list type="bullet">
///   <item>the per-TEAM held set (a team holds a tech once any of its players does), and</item>
///   <item>the WORLD tech count (how many teams hold each tech — incremented the first time a team gains it).</item>
/// </list>
/// The enabler reads the derived team held set + world count; nothing here reads the live snapshot. Replaying the
/// snapshot's held techs through <see cref="ResearchCompleted"/> is how the whole tech layer is dry-modelled.
/// </summary>
public sealed class TechTally : IResearchCompleted
{
    private readonly IReadOnlyDictionary<int, int> _playerTeam;             // playerId -> teamId
    private readonly Dictionary<int, HashSet<string>> _playerTechs = new(); // SOURCE OF TRUTH (per player)
    private readonly Dictionary<int, HashSet<string>> _teamTechs = new();   // derived (per team)
    private readonly WorldTally _world = new();                             // derived (teams-per-tech count)

    public TechTally(IReadOnlyDictionary<int, int> playerTeam) => _playerTeam = playerTeam;

    public void ResearchCompleted(int playerId, string tech)
    {
        // Source of truth: this player now holds the tech.
        if (!_playerTechs.TryGetValue(playerId, out HashSet<string>? pt))
            _playerTechs[playerId] = pt = new(StringComparer.Ordinal);
        pt.Add(tech);

        // Derive: the player's team holds it; if the team gained it just now, one more team holds it worldwide.
        int teamId = _playerTeam.TryGetValue(playerId, out int t) ? t : playerId;
        if (!_teamTechs.TryGetValue(teamId, out HashSet<string>? tt))
            _teamTechs[teamId] = tt = new(StringComparer.Ordinal);
        if (tt.Add(tech))
            _world.Add(tech);
    }

    /// <summary>The team's held techs, derived from its players' tallies.</summary>
    public IReadOnlySet<string> TeamHeld(int teamId)
        => _teamTechs.TryGetValue(teamId, out HashSet<string>? t) ? t : Empty;

    /// <summary>The derived world tech count — <c>Count(tech)</c> = number of teams holding it.</summary>
    public IWorldTally World => _world;

    private static readonly IReadOnlySet<string> Empty = new HashSet<string>(StringComparer.Ordinal);
}
