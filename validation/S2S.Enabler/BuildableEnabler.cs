using S2S.Model;

namespace S2S.Enabler;

/// <summary>
/// The buildable enabler — the GENERATE pass, per enabler.md §2/§2.0. The FIXED run order is
/// <c>replaces → disables → enables → obsoletes</c>:
/// <list type="bullet">
///   <item><b>replaces / disables</b> change POSSESSION — the superseded/banned target leaves HAS, so a replaced
///         or disabled *enabler* no longer enables its own targets (the §2.0 possession-propagation a flat
///         frontier-subtract misses).</item>
///   <item><b>enables</b> GENERATES the frontier from the corrected HAS.</item>
///   <item><b>obsoletes</b> PRUNES the frontier only — obsoleted ≠ gone: an existing instance persists and keeps
///         enabling, but no NEW builds are offered.</item>
/// </list>
/// All reads are forward, source-side. The requires-gate is the separate next step.
/// </summary>
public static class BuildableEnabler
{
    public enum Edge { Enables, Obsoletes, Replaces, Disables }

    /// <summary>Forward index: source-type → the targets of <paramref name="kind"/> it touches via <paramref name="edge"/>.</summary>
    public static Dictionary<string, List<string>> Index(IReadOnlyList<EnablerEdges> sources, string kind, Edge edge)
    {
        var index = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        foreach (EnablerEdges s in sources)
        {
            Dictionary<string, List<string>>? map = edge switch
            {
                Edge.Enables => s.Enables,
                Edge.Disables => s.Disables,
                _ => null,   // Obsoletes/Replaces are TARGET-side (obsoletedBy/replacedBy) now — use Reverse()
            };
            if (map is not null && map.TryGetValue(kind, out List<string>? targets) && targets.Count > 0)
            {
                if (!index.TryGetValue(s.Type, out List<string>? list)) index[s.Type] = list = [];
                list.AddRange(targets);
            }
        }
        return index;
    }

    /// <summary>
    /// The REVERSE MAPPING — built in-memory at load, NEVER stored in the JSON (owner ruling 2026-06-22). The
    /// JSON authors supersession TARGET-side (<c>obsoletedBy</c>/<c>replacedBy</c> on the superseded entity, the
    /// natural XML/engine direction). This is the "traverse the list a 2nd time and push the id onto the other
    /// side" pass: for each target entity, push its id onto each superseder's bucket, yielding the source-side
    /// index <c>superseder → [entities it obsoletes/replaces]</c> that <see cref="CanGet"/> consumes exactly as a
    /// natively source-side index would. Pass the TARGET-kind entities (e.g. only buildings) as <paramref name="targets"/>.
    /// </summary>
    public static Dictionary<string, List<string>> Reverse(IEnumerable<EnablerEdges> targets, Edge edge)
    {
        var rev = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        foreach (EnablerEdges a in targets)
        {
            Dictionary<string, List<string>>? map = edge switch
            {
                Edge.Obsoletes => a.ObsoletedBy,
                Edge.Replaces => a.ReplacedBy,
                _ => null,
            };
            if (map is null) continue;
            foreach (List<string> supersederBucket in map.Values)
                foreach (string superseder in supersederBucket)
                {
                    if (!rev.TryGetValue(superseder, out List<string>? list)) rev[superseder] = list = [];
                    list.Add(a.Type);
                }
        }
        return rev;
    }

    /// <summary>The city's CAN GET frontier for one kind, run in the §2.0 order.</summary>
    public static HashSet<string> CanGet(
        IReadOnlySet<string> has,
        IReadOnlyDictionary<string, List<string>> enables,
        IReadOnlyDictionary<string, List<string>> obsoletes,
        IReadOnlyDictionary<string, List<string>> replaces,
        IReadOnlyDictionary<string, List<string>> disables)
    {
        // §2.0 EXACT ORDER — replaces → disables → enables → obsoletes.
        // (1) replaces FIRST — collapse succession chains (TRANSITIVE): every predecessor a HAS member replaces,
        //     recursively (A→A′→A″), leaves possession, so HAS holds only the final survivors.
        var replaced = new HashSet<string>(StringComparer.Ordinal);
        var queue = new Queue<string>(has);
        while (queue.Count > 0)
            if (replaces.TryGetValue(queue.Dequeue(), out List<string>? rep))
                foreach (string r in rep)
                    if (replaced.Add(r)) queue.Enqueue(r);
        var has1 = new HashSet<string>(has, StringComparer.Ordinal);
        has1.ExceptWith(replaced);

        // (2) disables SECOND — drop banned/destroyed things from the succession-collapsed possession.
        var disabled = new HashSet<string>(StringComparer.Ordinal);
        foreach (string h in has1)
            if (disables.TryGetValue(h, out List<string>? dis)) disabled.UnionWith(dis);
        var has2 = new HashSet<string>(has1, StringComparer.Ordinal);
        has2.ExceptWith(disabled);

        // (3) enables THIRD — generate the frontier from the corrected possession (a replaced/disabled enabler no
        //     longer enables); the replaced/disabled targets themselves can't be built.
        var frontier = new HashSet<string>(StringComparer.Ordinal);
        foreach (string h in has2)
            if (enables.TryGetValue(h, out List<string>? en)) frontier.UnionWith(en);
        frontier.ExceptWith(replaced);
        frontier.ExceptWith(disabled);

        // (4) obsoletes LAST — prune the frontier only (obsoleted ≠ gone: instance persists, just no NEW builds).
        foreach (string h in has2)
            if (obsoletes.TryGetValue(h, out List<string>? obs)) frontier.ExceptWith(obs);

        return frontier;
    }
}
