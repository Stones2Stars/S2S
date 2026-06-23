using System.Text.Json;
using System.Text.Json.Serialization;

namespace S2S.Model;

/// <summary>
/// A leaf or node of the shared condition vocabulary (data-model.md §2.4): the boolean tree reused by
/// <c>requires.build</c> / <c>requires.operate</c> and the <c>enabled</c>/<c>disabled</c> clauses. A leaf is
/// an <see cref="Atom"/> (count/presence) or a <see cref="BarePredicate"/> (a runtime-state query like
/// <c>HAS_COAST</c>); a node is a <see cref="ConditionGroup"/> (all / any-of-OR-groups / noneOf). Object
/// forms not yet typed are kept raw in <see cref="RawCondition"/> so loading never fails.
/// </summary>
[JsonConverter(typeof(ConditionJsonConverter))]
public abstract record Condition;

/// <summary>A boolean node: holds iff every <see cref="All"/> holds, every <see cref="Any"/> OR-group has at
/// least one member that holds, no <see cref="NoneOf"/> holds, <see cref="Enabled"/> (if present) holds, and
/// <see cref="Disabled"/> (if present) does NOT hold — the enabled/disabled twin (data-model.md §2.8/§3.2,
/// e.g. an education band's <c>requires.operate.disabled</c> "only the highest band operates").</summary>
public sealed record ConditionGroup(
    IReadOnlyList<Condition>? All = null,
    IReadOnlyList<IReadOnlyList<Condition>>? Any = null,
    IReadOnlyList<Condition>? NoneOf = null,
    Condition? Enabled = null,
    Condition? Disabled = null) : Condition;

/// <summary>A count / presence atom — <c>{type, scope, min?, max?, connection?}</c>. Presence is the min:1 case.</summary>
public sealed record Atom(
    string Type,
    string? Scope = null,
    int? Min = null,
    int? Max = null,
    string? Connection = null) : Condition;

/// <summary>A bare-string predicate — a system's runtime-state query, e.g. <c>HAS_COAST</c>, <c>IS_CAPITAL</c>.</summary>
public sealed record BarePredicate(string Name) : Condition;

/// <summary>An object-form predicate not yet given a typed shape (membership sugar like <c>{terrain:[…]}</c>,
/// <c>{HAS_BONUS:…}</c>). Carried raw so loading is lossless; promoted to a typed record as the vocabulary fills in.</summary>
public sealed record RawCondition(JsonElement Raw) : Condition;

/// <summary>Reads the polymorphic condition serialization (string | atom-object | group-object). Read-only —
/// the model is a load target, never serialized back.</summary>
public sealed class ConditionJsonConverter : JsonConverter<Condition>
{
    public override Condition Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        using JsonDocument doc = JsonDocument.ParseValue(ref reader);
        return FromElement(doc.RootElement);
    }

    internal static Condition FromElement(JsonElement e)
    {
        if (e.ValueKind == JsonValueKind.String)
            return new BarePredicate(e.GetString()!);

        if (e.ValueKind != JsonValueKind.Object)
            return new RawCondition(e.Clone());

        bool isGroup = e.TryGetProperty("all", out _)
                    || e.TryGetProperty("any", out _)
                    || e.TryGetProperty("noneOf", out _)
                    || e.TryGetProperty("enabled", out _)
                    || e.TryGetProperty("disabled", out _);
        if (isGroup)
        {
            IReadOnlyList<Condition>? all = e.TryGetProperty("all", out var a)
                ? a.EnumerateArray().Select(FromElement).ToList()
                : null;
            IReadOnlyList<IReadOnlyList<Condition>>? any = e.TryGetProperty("any", out var an)
                ? an.EnumerateArray()
                    .Select(g => (IReadOnlyList<Condition>)g.EnumerateArray().Select(FromElement).ToList())
                    .ToList()
                : null;
            IReadOnlyList<Condition>? none = e.TryGetProperty("noneOf", out var n)
                ? n.EnumerateArray().Select(FromElement).ToList()
                : null;
            Condition? enabled = e.TryGetProperty("enabled", out var en) ? FromElement(en) : null;
            Condition? disabled = e.TryGetProperty("disabled", out var di) ? FromElement(di) : null;
            return new ConditionGroup(all, any, none, enabled, disabled);
        }

        if (e.TryGetProperty("type", out var ty) && ty.ValueKind == JsonValueKind.String)
        {
            string? scope = e.TryGetProperty("scope", out var s) && s.ValueKind == JsonValueKind.String ? s.GetString() : null;
            int? min = e.TryGetProperty("min", out var mn) && mn.ValueKind == JsonValueKind.Number ? mn.GetInt32() : null;
            int? max = e.TryGetProperty("max", out var mx) && mx.ValueKind == JsonValueKind.Number ? mx.GetInt32() : null;
            string? conn = e.TryGetProperty("connection", out var c) && c.ValueKind == JsonValueKind.String ? c.GetString() : null;
            return new Atom(ty.GetString()!, scope, min, max, conn);
        }

        return new RawCondition(e.Clone());
    }

    public override void Write(Utf8JsonWriter writer, Condition value, JsonSerializerOptions options)
        => throw new NotSupportedException("S2S.Model is a load target; conditions are not serialized back.");
}
