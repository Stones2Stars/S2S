namespace S2S.Application;

/// <summary>
/// The fictitious "research completed" event endpoint — fired (one tech at a time, with a PLAYER identifier) when
/// a player finishes researching a tech. For now it is fed the already-completed techs and treats each as a
/// genuine completion. The tech tally consumes these into a per-player source of truth and DERIVES from it the
/// per-team held set and the world tech count. This is the event-spine model: replaying the snapshot's held techs
/// through this endpoint is what lets us fully dry-model the cascade + tally (+ eventually the modifier) — the
/// enabler reads the internally-derived tally, never the live snapshot.
/// </summary>
public interface IResearchCompleted
{
    void ResearchCompleted(int playerId, string tech);
}
