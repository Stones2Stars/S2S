using S2S.Application;

namespace S2S.Tally;

/// <summary>TEAM-scope tally: techs (team-scoped; static — once pushed, never removed, so it only grows).</summary>
public sealed class TeamTally : TallyBase, ITeamTally { }
