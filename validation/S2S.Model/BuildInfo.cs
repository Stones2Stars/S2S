using System.Text.Json;
using System.Text.Json.Serialization;

namespace S2S.Model;

/// <summary>
/// One curated BUILD definition (<c>Assets/Data/builds/**.json</c>) — the worker action that places an
/// improvement/route. For "can it be built" the load-bearing part is <see cref="Requires"/> (its <c>build</c>
/// = the unlock gate, typically a single tech). The produced improvement's own per-plot placement gate is a
/// separate concern we don't validate here.
/// </summary>
public sealed record BuildInfo
{
    [JsonPropertyName("type")]     public required string Type { get; init; }
    [JsonPropertyName("requires")] public Requires? Requires { get; init; }

    [JsonExtensionData] public Dictionary<string, JsonElement>? Extra { get; init; }
}
