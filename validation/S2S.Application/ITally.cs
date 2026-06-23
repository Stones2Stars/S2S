namespace S2S.Application;

/// <summary>
/// The TALLY contract — the cascade's "how many" machine, a push-built count accumulator. Built ONLY by pushing
/// "I have one of this item" (the event-spine model — each push is a "have" event). The enabler depends on THIS
/// abstraction (never a concrete tally) to answer count-based gates: wonder uniqueness (>=1 already built) and
/// thresholds (>=8 universities, >=12 forges). Scope is a property of the concrete; consumers program to the
/// scoped contracts below.
/// </summary>
public interface ITally
{
    /// <summary>Push one "I have this item". Increments its count.</summary>
    void Add(string item);

    /// <summary>How many of this item are tallied (0 if none).</summary>
    int Count(string item);

    /// <summary>Is at least one tallied?</summary>
    bool Has(string item);

    /// <summary>Every distinct item tallied.</summary>
    IReadOnlyCollection<string> Items { get; }
}

/// <summary>TEAM-scope tally contract — techs (team-scoped; static — once pushed, never removed).</summary>
public interface ITeamTally : ITally { }

/// <summary>EMPIRE-scope tally contract — civics, buildings, units, connected resources (counts).</summary>
public interface IEmpireTally : ITally { }

/// <summary>
/// WORLD-scope tally contract — "this item has been added by SOMEONE". Fed by every player's pushes;
/// <see cref="ITally.Has"/> answers "built anywhere in the world" (world-wonder uniqueness).
/// </summary>
public interface IWorldTally : ITally { }
