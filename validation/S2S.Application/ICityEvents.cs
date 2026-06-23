namespace S2S.Application;

/// <summary>
/// "Building completed" event — fired (one at a time, carrying the building, its CITY, and the owning PLAYER) as a
/// city gains a building. The CITY tally consumes it into the per-city HAVE that feeds that city's enabler
/// generation (in-city buildings are an enabling source); the empire/team/world building counts roll UP from the
/// city leaves (enabler.md §4). In validation the snapshot's per-city buildings are replayed through this.
/// </summary>
public interface IBuildingCompleted
{
    void BuildingCompleted(int playerId, long cityId, string building);
}

/// <summary>
/// "Unit produced" event — fired per unit a player builds. The units-produced tally counts them per player (units
/// are leaf actions — enabler.md §6 — but their COUNT feeds the required side). Derived from state for the dry-calc.
/// </summary>
public interface IUnitProduced
{
    void UnitProduced(int playerId, string unit);
}

/// <summary>
/// "Bonus added" event — imitates a TRADE-connected resource arriving (empire/team-wide, the way trade makes a
/// bonus available across your cities). The same application-event pattern as the others; it fills the resource
/// HAVE that the REQUIRES side consults (`requires` connection:trade). Player-scope; in validation the snapshot's
/// trade-connected city bonuses are replayed through it.
/// </summary>
public interface IBonusAdded
{
    void BonusAdded(int playerId, string bonus);
}

/// <summary>
/// VICINITY bonus presence — a bonus a city can currently reach in its working radius (adjacency, NOT trade).
/// City-scope, consulted by `requires` connection:vicinity. Distinct from <see cref="IBonusAdded"/> (trade).
/// </summary>
public interface IBonusPresent
{
    void BonusPresent(long cityId, string bonus);
}
