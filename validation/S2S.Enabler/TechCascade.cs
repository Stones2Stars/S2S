using S2S.Model;
using S2S.Application;

namespace S2S.Enabler;

/// <summary>
/// Computes a civ's available-to-research tech set IN ISOLATION (curated definitions + caller-supplied domain
/// inputs only — never the live-state DTOs): a tech is available iff it is not held, not <c>identity.disable</c>,
/// under its cross-referenced <c>allowed.world</c> cap, and its <c>requires.build</c> is satisfied by the held
/// set. The held set and the world-held counts are domain inputs the caller derives (e.g. from the tallies); no
/// engine verdict is consulted here — that is the comparison step's job.
/// </summary>
public static class TechCascade
{
    private static readonly ConditionEvaluator Eval = new();

    public static HashSet<string> Available(
        IReadOnlyDictionary<string, TechInfo> techs,
        IReadOnlySet<string> held,
        IWorldTally world)
    {
        var state = new EvalState { Techs = held };
        var available = new HashSet<string>(StringComparer.Ordinal);
        foreach (TechInfo t in techs.Values)
        {
            if (t.IsDisabled) continue;
            if (held.Contains(t.Type)) continue;
            int cap = t.Allowed is not null && t.Allowed.TryGetValue("world", out int c) ? c : int.MaxValue;
            if (world.Count(t.Type) >= cap) continue;
            if (!Eval.Evaluate(t.Requires?.Build, state)) continue;
            available.Add(t.Type);
        }
        return available;
    }
}
