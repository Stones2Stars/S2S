namespace S2S.Model;

/// <summary>
/// The loaded curated entity definitions from <c>Assets/Data/**</c> — the "definitions" plane (distinct from
/// the live-state plane the extractor pulls). The keystone shared by the data-completeness validator,
/// cascade-parity checks, and a future hosted pedia. Built incrementally — buildings first.
/// </summary>
public interface IInfoRepository
{
    /// <summary>All building definitions, keyed by type id (e.g. <c>BUILDING_BAKERY</c>).</summary>
    IReadOnlyDictionary<string, BuildingInfo> Buildings { get; }

    /// <summary>Total entity count loaded.</summary>
    int Count { get; }
}
