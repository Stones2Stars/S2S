using S2S.Application;

namespace S2S.Tally;

/// <summary>WORLD-scope tally: "this item has been added by SOMEONE". Fed by every player's pushes;
/// <see cref="TallyBase.Has"/> answers world-wonder uniqueness (built anywhere). Count = how many exist worldwide.</summary>
public sealed class WorldTally : TallyBase, IWorldTally { }
