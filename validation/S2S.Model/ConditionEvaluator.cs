using System.Text.Json;

namespace S2S.Model;

/// <summary>Evaluates a <see cref="Condition"/> tree against an <see cref="EvalState"/> (+ optional plot).</summary>
public interface IConditionEvaluator
{
    bool Evaluate(Condition? condition, EvalState state, Plot? plot = null);
}

/// <summary>
/// The condition tree-walk (data-model.md §2.4–2.6), faithful to the legacy atom semantics. A null condition
/// is vacuously true; an unknown bare predicate is IGNORED (treated true) so a retired system's references go
/// quiet rather than spuriously disable (enabler-spec §3).
/// </summary>
public sealed class ConditionEvaluator : IConditionEvaluator
{
    /// <summary>When true, a <c>{STATE_RELIGION: X}</c> predicate is the STRICT build gate (getPrereqStateReligion,
    /// CvPlayer:6676 — the player's state religion must MATCH; infotype-translation.md:1439). When false (default),
    /// it is the lenient MODIFIER compound (CvCity~12498: matches OR no state religion OR non-state-commerce;
    /// migration-renames.md:279). Set true only for the <c>requires.build</c> evaluation.</summary>
    public bool StrictStateReligionForBuild { get; init; }

    public bool Evaluate(Condition? condition, EvalState state, Plot? plot = null) => condition switch
    {
        null               => true,
        ConditionGroup g   => EvalGroup(g, state, plot),
        Atom a             => EvalAtom(a, state, plot),
        BarePredicate p    => EvalPredicate(p.Name, state, plot) ?? true,
        RawCondition r     => EvalRaw(r.Raw, state, plot),
        _                  => true,
    };

    private bool EvalGroup(ConditionGroup g, EvalState s, Plot? p)
    {
        if (g.All is not null && !g.All.All(c => Evaluate(c, s, p))) return false;
        if (g.Any is not null && !g.Any.All(grp => grp.Any(c => Evaluate(c, s, p)))) return false;
        if (g.NoneOf is not null && g.NoneOf.Any(c => Evaluate(c, s, p))) return false;
        if (g.Enabled is not null && !Evaluate(g.Enabled, s, p)) return false;
        if (g.Disabled is not null && Evaluate(g.Disabled, s, p)) return false;
        return true;
    }

    private static bool EvalAtom(Atom a, EvalState s, Plot? p)
    {
        // PROPERTY_* BAND atom — the city's current value gated by min/max (absent min = no lower bound).
        if (a.Type.StartsWith("PROPERTY_", StringComparison.Ordinal))
        {
            int val = s.Counts.GetValueOrDefault(a.Type);
            return (a.Min is null || val >= a.Min) && (a.Max is null || val <= a.Max);
        }
        // count atom with an explicit band → count(type) at scope.
        if (a.Min is not null || a.Max is not null)
        {
            int n = CountOf(a.Type, a.Scope ?? "empire", s, p);
            return (a.Min is null || n >= a.Min) && (a.Max is null || n <= a.Max);
        }
        // presence (min:1).
        return Present(a.Type, a.Connection, s, p);
    }

    private static bool Present(string type, string? connection, EvalState s, Plot? p)
    {
        if (type.StartsWith("TECH_", StringComparison.Ordinal)) return s.Techs.Contains(type);
        if (type.StartsWith("CIVIC_", StringComparison.Ordinal)) return s.Civics.Contains(type);
        if (type.StartsWith("TRAIT_", StringComparison.Ordinal)) return s.Traits.Contains(type);
        if (type.StartsWith("RELIGION_", StringComparison.Ordinal)) return s.Religions.Contains(type);
        if (type.StartsWith("HERITAGE_", StringComparison.Ordinal)) return s.Heritages.Contains(type);
        if (type.StartsWith("PROJECT_", StringComparison.Ordinal)) return s.Projects.Contains(type);
        if (type.StartsWith("BUILDING_", StringComparison.Ordinal)) return s.ConstructedBuildings.Contains(type);
        if (type.StartsWith("CORPORATION_", StringComparison.Ordinal)) return s.PresentCorporations.Contains(type);
        if (type.StartsWith("BONUS_", StringComparison.Ordinal)) return BonusPresent(type, connection, s, p);
        if (type.StartsWith("MAPCATEGORY_", StringComparison.Ordinal)) return MapCategoryValid(type, p);
        if (type.StartsWith("FEATURE_", StringComparison.Ordinal)) return p?.Feature == type;
        if (type.StartsWith("TERRAIN_", StringComparison.Ordinal)) return p?.Terrain == type;
        if (type.StartsWith("IMPROVEMENT_", StringComparison.Ordinal)) return p?.Improvement == type;
        if (type.StartsWith("ROUTE_", StringComparison.Ordinal)) return p?.Route == type;
        return false;
    }

    /// <summary>Bonus presence honoring <c>connection</c> (CvCascadeCondition semantics): vicinity → vicinity set,
    /// trade → trade set, a bare atom against a PLOT → that plot's own bonus, else either set.</summary>
    private static bool BonusPresent(string type, string? conn, EvalState s, Plot? p)
    {
        if (conn == "vicinity") return s.VicinityBonuses.Contains(type);
        if (conn == "trade") return s.Bonuses.Contains(type);
        if (p is not null && conn is null) return p.Bonus == type;
        return s.Bonuses.Contains(type) || s.VicinityBonuses.Contains(type);
    }

    private static bool MapCategoryValid(string type, Plot? p)
        => p is null || p.MapCategories.Count == 0 || p.MapCategories.Contains(type);

    private static int CountOf(string type, string scope, EvalState s, Plot? p)
    {
        if (s.Counts.TryGetValue(type, out var c)) return c;
        if (type == "POPULATION") return s.Population;
        if (type == "CITY") return s.CityCount;
        if (type == "TEAM") return s.TeamCount;
        if (type.StartsWith("RELIGION_", StringComparison.Ordinal) && s.ReligionLevels.TryGetValue(type, out var rl)) return rl;
        if (scope is "empire" or "team")
        {
            if (type.StartsWith("BUILDING_", StringComparison.Ordinal)) return s.BuildingCounts.GetValueOrDefault(type);
            if (type.StartsWith("UNIT_", StringComparison.Ordinal)) return s.UnitCounts.GetValueOrDefault(type);
        }
        if (scope == "plot" && p is not null)
            return p.Bonus == type || p.Feature == type || p.Terrain == type || p.Improvement == type || p.Route == type ? 1 : 0;
        return Present(type, null, s, p) ? 1 : 0;   // presence fallback (N=1)
    }

    private static bool? EvalPredicate(string name, EvalState s, Plot? p) => name switch
    {
        "HAS_RIVER"          => p?.River ?? false,
        "HAS_IRRIGATION"     => p?.Irrigation ?? false,
        "HAS_HILLS"          => p?.Hills ?? false,
        "HAS_PEAK"           => p?.Peak ?? false,
        "IS_FLATLANDS"       => !(p?.Hills ?? false) && !(p?.Peak ?? false),
        "IS_WATER"           => p?.Water ?? false,
        "IS_LAND"            => !(p?.Water ?? false),
        "HAS_COAST"          => p?.Coast ?? false,
        "HAS_FRESHWATER"     => (p?.Freshwater ?? false) || (p?.River ?? false),
        "IS_CAPITAL"         => s.IsCapital,
        "HAS_POWER"          => s.IsPowered,
        "IS_GOLDEN_AGE"      => s.IsGoldenAge,
        "HAS_STATE_RELIGION" => s.StateReligion is not null,
        "STATE_RELIGION_IN_CITY" => s.StateReligion is not null && s.Religions.Contains(s.StateReligion),
        _                    => null,   // unknown predicate → ignored (caller treats as true)
    };

    /// <summary>Object-form predicates not given a typed record yet (membership sugar + single-key forms),
    /// evaluated faithfully so they are never silently ignored when they appear in a condition.</summary>
    private bool EvalRaw(JsonElement e, EvalState s, Plot? p)
    {
        if (e.ValueKind != JsonValueKind.Object) return true;

        // membership sugar: {terrain|feature|improvement|bonus: [..]} → any-of
        if (e.TryGetProperty("terrain", out var tl) && tl.ValueKind == JsonValueKind.Array)
            return tl.EnumerateArray().Any(t => p?.Terrain == t.GetString());
        if (e.TryGetProperty("feature", out var fl) && fl.ValueKind == JsonValueKind.Array)
            return fl.EnumerateArray().Any(t => p?.Feature == t.GetString());
        if (e.TryGetProperty("improvement", out var il) && il.ValueKind == JsonValueKind.Array)
            return il.EnumerateArray().Any(t => p?.Improvement == t.GetString());
        if (e.TryGetProperty("bonus", out var bl) && bl.ValueKind == JsonValueKind.Array)
        {
            string? conn = GetStr(e, "connection");
            return bl.EnumerateArray().Any(t => BonusPresent(t.GetString()!, conn, s, p));
        }

        // {latitude:{min,max}} — city plot absolute-latitude band (PRED_LATITUDE: min <= getLatitude() <= max).
        if (e.TryGetProperty("latitude", out var lat) && lat.ValueKind == JsonValueKind.Object)
        {
            int lo = lat.TryGetProperty("min", out var mn) && mn.ValueKind == JsonValueKind.Number ? mn.GetInt32() : int.MinValue;
            int hi = lat.TryGetProperty("max", out var mx) && mx.ValueKind == JsonValueKind.Number ? mx.GetInt32() : int.MaxValue;
            return s.Latitude >= lo && s.Latitude <= hi;
        }

        foreach (var prop in e.EnumerateObject())
        {
            switch (prop.Name)
            {
                case "scope" or "per" or "value" or "ai" or "connection" or "each" or "min" or "max" or "enabled" or "disabled":
                    continue;
                case "HAS_BONUS":       return BonusPresent(prop.Value.GetString()!, GetStr(e, "connection"), s, p);
                case "HAS_FEATURE":     return p?.Feature == prop.Value.GetString();
                case "HAS_TERRAIN":     return p?.Terrain == prop.Value.GetString();
                case "HAS_IMPROVEMENT": return p?.Improvement == prop.Value.GetString();
                case "HAS_RELIGION":    return s.Religions.Contains(prop.Value.GetString()!);
                case "HAS_CORPORATION": return s.Corporations.Contains(prop.Value.GetString()!);
                case "HOLY_CITY":       return s.HolyCityReligions.Contains(prop.Value.GetString()!);
                case "STATE_RELIGION":
                    string? want = prop.Value.GetString();
                    // BUILD gate is strict (state religion must MATCH); modifier form is the lenient compound.
                    return StrictStateReligionForBuild
                        ? s.StateReligion == want
                        : (s.StateReligion == want || s.StateReligion is null || s.NonStateReligionCommerce);
                case "HAS_RIVER" or "HAS_IRRIGATION" or "HAS_HILLS" or "HAS_PEAK" or "HAS_COAST"
                  or "HAS_FRESHWATER" or "IS_WATER" or "IS_LAND" or "IS_FLATLANDS":
                    bool wantBool = prop.Value.ValueKind != JsonValueKind.False;
                    return (EvalPredicate(prop.Name, s, p) ?? false) == wantBool;
            }
        }
        return true;   // unknown object predicate → ignored
    }

    private static string? GetStr(JsonElement e, string name)
        => e.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() : null;
}
