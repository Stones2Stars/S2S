using System.Text.Json;

namespace S2S.Model;

/// <summary>
/// Materializes the typed <see cref="ModifierFamily"/> tree from the raw <c>[JsonExtensionData]</c> family
/// objects captured on a building (data-model.md §4). Lossless and generic — a non-magnitude "family" (e.g.
/// <c>allowedSpecialists</c>) parses as keyed nodes carrying bare values, never failing the load.
/// </summary>
public static class ModifierFamilyParser
{
    private static readonly HashSet<string> Units = new(StringComparer.Ordinal)
    {
        "flat", "percent", "multiplier", "perPopulation", "perSpecialist", "rawPercent", "postMultiplier",
    };

    private static readonly IReadOnlyDictionary<string, ModifierNode> EmptyChildren =
        new Dictionary<string, ModifierNode>();

    /// <summary>Parse every OBJECT-valued entry of a raw family bag into typed families. Bare-valued keys
    /// (capability flags, per data-model.md §1) are skipped — they are not families.</summary>
    public static IReadOnlyDictionary<string, ModifierFamily> ParseAll(IReadOnlyDictionary<string, JsonElement>? families)
    {
        var result = new Dictionary<string, ModifierFamily>(StringComparer.Ordinal);
        if (families is null) return result;
        foreach (var (name, el) in families)
        {
            if (el.ValueKind != JsonValueKind.Object) continue;   // bare value -> a flag, not a family
            result[name] = new ModifierFamily(name, ParseNode(el));
        }
        return result;
    }

    private static ModifierNode ParseNode(JsonElement e)
    {
        if (e.ValueKind == JsonValueKind.Number)
            return new ModifierNode([], EmptyChildren, e.GetDouble());

        var mags = new List<Magnitude>();
        var children = new Dictionary<string, ModifierNode>(StringComparer.Ordinal);

        if (e.ValueKind == JsonValueKind.Object)
        {
            foreach (var prop in e.EnumerateObject())
            {
                if (Units.Contains(prop.Name))
                    mags.AddRange(ParseMagnitudes(prop.Name, prop.Value));
                else
                    children[prop.Name] = ParseNode(prop.Value);
            }
        }

        return new ModifierNode(mags, children);
    }

    private static IEnumerable<Magnitude> ParseMagnitudes(string unit, JsonElement value)
    {
        switch (value.ValueKind)
        {
            case JsonValueKind.Number:
                yield return new Magnitude(unit, value.GetDouble());
                break;
            case JsonValueKind.Array:
                foreach (var item in value.EnumerateArray())
                    yield return ParseEntry(unit, item);
                break;
            case JsonValueKind.Object:
                yield return ParseEntry(unit, value);
                break;
        }
    }

    private static Magnitude ParseEntry(string unit, JsonElement e)
    {
        if (e.ValueKind == JsonValueKind.Number)
            return new Magnitude(unit, e.GetDouble());

        double val = e.TryGetProperty("value", out var v) && v.ValueKind == JsonValueKind.Number ? v.GetDouble() : 0;
        string? scope = e.TryGetProperty("scope", out var s) && s.ValueKind == JsonValueKind.String ? s.GetString() : null;
        Condition? enabled = e.TryGetProperty("enabled", out var en) ? ConditionJsonConverter.FromElement(en) : null;
        Condition? disabled = e.TryGetProperty("disabled", out var di) ? ConditionJsonConverter.FromElement(di) : null;
        Per? per = e.TryGetProperty("per", out var pr) && pr.ValueKind == JsonValueKind.Object ? ParsePer(pr) : null;
        Magnitude? ai = e.TryGetProperty("ai", out var aiEl) ? ParseEntry(unit, aiEl) : null;
        return new Magnitude(unit, val, scope, enabled, disabled, per, ai);
    }

    private static Per ParsePer(JsonElement e)
    {
        string? type = e.TryGetProperty("type", out var t) && t.ValueKind == JsonValueKind.String ? t.GetString() : null;
        IReadOnlyList<string>? anyOf = e.TryGetProperty("anyOf", out var a) && a.ValueKind == JsonValueKind.Array
            ? a.EnumerateArray().Where(x => x.ValueKind == JsonValueKind.String).Select(x => x.GetString()!).ToList()
            : null;
        int? each = e.TryGetProperty("each", out var ea) && ea.ValueKind == JsonValueKind.Number ? ea.GetInt32() : null;
        string? scope = e.TryGetProperty("scope", out var sc) && sc.ValueKind == JsonValueKind.String ? sc.GetString() : null;
        return new Per(type, anyOf, each, scope);
    }
}
