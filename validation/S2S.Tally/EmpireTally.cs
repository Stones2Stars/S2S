using S2S.Application;

namespace S2S.Tally;

/// <summary>EMPIRE-scope tally: civics, buildings, units, connected resources (counts). Scope-correct "empire",
/// never the legacy mislabel "global".</summary>
public sealed class EmpireTally : TallyBase, IEmpireTally { }
