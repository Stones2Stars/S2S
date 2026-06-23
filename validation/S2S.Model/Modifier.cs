namespace S2S.Model;

/// <summary>
/// A modifier family (data-model.md §4) — e.g. <c>food</c>, <c>commerce</c>, <c>culture</c>, <c>PROPERTY_*</c>,
/// <c>maintenance</c>. The <see cref="Root"/> node's children are scopes; below that sit unit magnitudes
/// (flat/percent/…) and further children (targets, members, entity-keys, sub-scopes, state-tables).
/// </summary>
public sealed record ModifierFamily(string Name, ModifierNode Root);

/// <summary>
/// A node in a modifier family's tree: a bag of <see cref="Magnitudes"/> (the recognized unit leaves at this
/// node) plus keyed <see cref="Children"/> (scope / target / member / entity-key / sub-scope sub-nodes).
/// A node parsed from a bare JSON number carries <see cref="BareValue"/> instead (e.g. an
/// <c>allowedSpecialists</c> <c>SPECIALIST_X: 1</c> entry — not a magnitude family).
/// </summary>
public sealed record ModifierNode(
    IReadOnlyList<Magnitude> Magnitudes,
    IReadOnlyDictionary<string, ModifierNode> Children,
    double? BareValue = null);

/// <summary>
/// One deposit magnitude. <see cref="Unit"/> ∈ flat | percent | multiplier | perPopulation | perSpecialist |
/// rawPercent | postMultiplier. Values are human-readable (the ×100 fixed-point is an engine concern, not the
/// model's). Carries the §2.8 entry modifiers (scope / enabled / disabled / per / ai) when authored in the
/// list / object form.
/// </summary>
public sealed record Magnitude(
    string Unit,
    double Value,
    string? Scope = null,
    Condition? Enabled = null,
    Condition? Disabled = null,
    Per? Per = null,
    Magnitude? Ai = null);

/// <summary>The count-scaler (data-model.md §2.6): scale a deposit by <c>count(type) / each</c> at <c>scope</c>.</summary>
public sealed record Per(
    string? Type = null,
    IReadOnlyList<string>? AnyOf = null,
    int? Each = null,
    string? Scope = null);
