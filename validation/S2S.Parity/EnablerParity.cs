using S2S.Model;

namespace S2S.Parity;

/// <summary>Diffs the cascade's enabler active-set verdict against the engine's own <c>dormantBuildings</c>
/// (the independent oracle), per city.</summary>
public interface IEnablerParity
{
    ParityReport Check(IInfoRepository repo, Snapshot snap);
}

/// <summary>
/// For each city: a constructed building is dry-dormant if a present building replaces it
/// (<c>replaces.buildings</c>) OR its <c>requires.operate</c> condition is not satisfied by the live state.
/// The diff against the engine's <c>dormantBuildings</c> is the verification — the engine is the constraint
/// we cannot fit. BAR (one-directional): the cascade must NEVER keep active what the engine dormants.
/// </summary>
public sealed class EnablerParity(IConditionEvaluator evaluator) : IEnablerParity
{
    public ParityReport Check(IInfoRepository repo, Snapshot snap)
    {
        var report = new ParityReport();

        foreach (Team team in snap.World.Teams)
        foreach (Empire emp in team.Empires)
        foreach (Area area in emp.Areas)
        foreach (City city in area.Cities)
        {
            report.Cities++;
            report.Constructed += city.Buildings.Count;

            EvalState state = EvalStateBuilder.Build(snap.World, team, emp, area, city);
            var constructed = new HashSet<string>(city.Buildings, StringComparer.Ordinal);
            var engineDormant = new HashSet<string>(city.DormantBuildings, StringComparer.Ordinal);

            // Replacement: Y is dormant if a present building X declares Y in replaces.buildings.
            var replaced = new HashSet<string>(StringComparer.Ordinal);
            foreach (string x in city.Buildings)
            {
                if (repo.Buildings.TryGetValue(x, out BuildingInfo? bx)
                    && bx.Replaces is not null
                    && bx.Replaces.TryGetValue("buildings", out List<string>? ys))
                {
                    foreach (string y in ys) replaced.Add(y);
                }
            }

            var dryDormant = new HashSet<string>(StringComparer.Ordinal);
            foreach (string b in city.Buildings)
            {
                bool dorm;
                if (replaced.Contains(b))
                    dorm = true;
                else if (repo.Buildings.TryGetValue(b, out BuildingInfo? bi))
                    dorm = !evaluator.Evaluate(bi.Requires?.Operate, state);
                else
                    dorm = false;   // building not in definitions -> cannot judge; treat as active
                if (dorm) dryDormant.Add(b);
            }

            int missedBefore = report.Missed, falseBefore = report.FalseDormant;

            foreach (string b in engineDormant)
                if (constructed.Contains(b) && !dryDormant.Contains(b))
                    report.AddMissed(city.Name, b, repo);   // engine dormant, dry ACTIVE = BAR VIOLATION

            foreach (string b in dryDormant)
                if (!engineDormant.Contains(b))
                    report.AddFalseDormant(city.Name, b, replaced.Contains(b));

            if (report.Missed == missedBefore && report.FalseDormant == falseBefore)
                report.ExactCities++;
        }

        return report;
    }
}

/// <summary>The parity result — counts + cause histograms + a bounded set of sample divergences.</summary>
public sealed class ParityReport
{
    public int Cities;
    public int ExactCities;
    public long Constructed;
    public int Missed;        // engine dormant, dry ACTIVE  -> BAR VIOLATION (target 0)
    public int FalseDormant;  // dry dormant, engine ACTIVE

    public readonly Dictionary<string, int> MissedByCause = new(StringComparer.Ordinal);
    public readonly Dictionary<string, int> FalseByCause = new(StringComparer.Ordinal);
    public readonly List<string> MissedSamples = [];
    public readonly List<string> FalseSamples = [];

    public void AddMissed(string? city, string building, IInfoRepository repo)
    {
        Missed++;
        string cause = MissedCause(building, repo);
        MissedByCause[cause] = MissedByCause.GetValueOrDefault(cause) + 1;
        if (MissedSamples.Count < 25) MissedSamples.Add($"{city}/{building}  [{cause}]");
    }

    public void AddFalseDormant(string? city, string building, bool replaced)
    {
        FalseDormant++;
        string cause = replaced ? "replaced" : "operate-false";
        FalseByCause[cause] = FalseByCause.GetValueOrDefault(cause) + 1;
        if (FalseSamples.Count < 15) FalseSamples.Add($"{city}/{building}  [{cause}]");
    }

    private static string MissedCause(string building, IInfoRepository repo)
    {
        if (!repo.Buildings.TryGetValue(building, out BuildingInfo? bi)) return "unknown-building";
        Condition? op = bi.Requires?.Operate;
        if (op is null) return "no-operate";
        if (MentionsTypePrefix(op, "PROPERTY_EDUCATION")) return "education-band (by-design remodel)";
        if (MentionsTypePrefix(op, "PROPERTY_")) return "other-property-band";
        return "other-operate";
    }

    private static bool MentionsTypePrefix(Condition? c, string prefix) => c switch
    {
        ConditionGroup g => (g.All?.Any(x => MentionsTypePrefix(x, prefix)) ?? false)
                         || (g.Any?.Any(grp => grp.Any(x => MentionsTypePrefix(x, prefix))) ?? false)
                         || (g.NoneOf?.Any(x => MentionsTypePrefix(x, prefix)) ?? false),
        Atom a => a.Type.StartsWith(prefix, StringComparison.Ordinal),
        _ => false,
    };
}
