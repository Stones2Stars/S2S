using S2S.Application;

namespace S2S.Tally;

/// <summary>
/// Shared push-built count accumulator behind the scoped tallies. Abstract — consumers depend on the
/// <see cref="ITally"/> contracts in S2S.Application, never on this concrete.
/// </summary>
public abstract class TallyBase : ITally
{
    private readonly Dictionary<string, int> _have = new(StringComparer.Ordinal);

    public void Add(string item) => _have[item] = _have.GetValueOrDefault(item) + 1;
    public int Count(string item) => _have.GetValueOrDefault(item);
    public bool Has(string item) => _have.ContainsKey(item);
    public IReadOnlyCollection<string> Items => _have.Keys;
}
