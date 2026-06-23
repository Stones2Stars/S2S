using S2S.Model;

namespace S2S.Enabler;

/// <summary>
/// Computes a civ's available-to-adopt civic set IN ISOLATION: a civic is available iff the single tech that
/// enables it (the forward <c>tech.enables.civics</c> edge) is held, OR no tech enables it (a default/starting
/// civic). No engine-calculated verdict is consulted — that is reserved for the comparison step.
/// </summary>
public static class CivicCascade
{
    /// <summary>Reverse index: civic -> the tech that enables it (from <c>tech.enables.civics</c>).</summary>
    public static Dictionary<string, string> EnablingTech(IReadOnlyDictionary<string, TechInfo> techs)
    {
        var index = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (TechInfo t in techs.Values)
            if (t.Enables is not null && t.Enables.TryGetValue("civics", out List<string>? civics))
                foreach (string civic in civics)
                    index[civic] = t.Type;
        return index;
    }

    public static HashSet<string> Available(
        IReadOnlyList<string> civicTypes,
        IReadOnlyDictionary<string, string> enablingTech,
        IReadOnlySet<string> held)
    {
        var available = new HashSet<string>(StringComparer.Ordinal);
        foreach (string civic in civicTypes)
            if (!enablingTech.TryGetValue(civic, out string? tech) || held.Contains(tech))
                available.Add(civic);
        return available;
    }
}
