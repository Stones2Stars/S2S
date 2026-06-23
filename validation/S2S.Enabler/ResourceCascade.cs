using S2S.Model;

namespace S2S.Enabler;

/// <summary>
/// Computes which resources a civ can SEE (are unlocked) IN ISOLATION: a bonus is revealed by the forward
/// <c>tech.enables.bonuses</c> edge (or has none → default-visible like food resources), and is removed once a
/// held tech obsoletes it via <c>tech.obsoletes.bonuses</c>. This is the tech-driven REVEAL set — distinct from
/// "available/connected", which is pure observed state and not computed here.
/// </summary>
public static class ResourceCascade
{
    /// <summary>Reverse index: bonus -> the tech(s) whose <c>enables.bonuses</c> reveal it.</summary>
    public static Dictionary<string, List<string>> RevealedBy(IReadOnlyDictionary<string, TechInfo> techs)
        => Index(techs, obsolete: false);

    /// <summary>Reverse index: bonus -> the tech(s) whose <c>obsoletes.bonuses</c> retire it.</summary>
    public static Dictionary<string, List<string>> ObsoletedBy(IReadOnlyDictionary<string, TechInfo> techs)
        => Index(techs, obsolete: true);

    private static Dictionary<string, List<string>> Index(IReadOnlyDictionary<string, TechInfo> techs, bool obsolete)
    {
        var index = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        foreach (TechInfo t in techs.Values)
        {
            Dictionary<string, List<string>>? map = obsolete ? t.Obsoletes : t.Enables;
            if (map is not null && map.TryGetValue("bonuses", out List<string>? bonuses))
                foreach (string b in bonuses)
                {
                    if (!index.TryGetValue(b, out List<string>? list)) index[b] = list = [];
                    list.Add(t.Type);
                }
        }
        return index;
    }

    public static HashSet<string> Seen(
        IReadOnlyList<string> bonusTypes,
        IReadOnlyDictionary<string, List<string>> revealedBy,
        IReadOnlyDictionary<string, List<string>> obsoletedBy,
        IReadOnlySet<string> held)
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (string b in bonusTypes)
        {
            bool revealed = !revealedBy.TryGetValue(b, out List<string>? rt) || rt.Any(held.Contains);
            bool obsoleted = obsoletedBy.TryGetValue(b, out List<string>? ot) && ot.Any(held.Contains);
            if (revealed && !obsoleted) seen.Add(b);
        }
        return seen;
    }
}
