using S2S.Model;

namespace S2S.Parity;

/// <summary>Projects a snapshot city (with its inherited team/empire/world scope) into the evaluator's
/// <see cref="EvalState"/>. Pure mapping of observed facts — no derived/calculated values.</summary>
public static class EvalStateBuilder
{
    public static EvalState Build(World w, Team t, Empire e, Area a, City c)
    {
        var counts = new Dictionary<string, int>(c.Properties, StringComparer.Ordinal)
        {
            ["AREA_SIZE"] = a.AreaSize,
        };

        return new EvalState
        {
            Techs = Set(t.Techs),
            Projects = Set(t.Projects),
            Civics = Set(e.Civics),
            Traits = Set(e.Traits),
            Heritages = Set(e.Heritages),
            StateReligion = e.StateReligion,
            CityCount = e.CityCount,
            NonStateReligionCommerce = e.NonStateReligionCommerce,
            ConstructedBuildings = Set(c.Buildings),
            Bonuses = Set(c.Bonuses),
            VicinityBonuses = Set(c.VicinityBonuses),
            Religions = Set(c.Religions),
            HolyCityReligions = Set(c.HolyCity),
            PresentCorporations = Set(c.PresentCorporations),
            Corporations = Set(c.Corporations),
            IsCapital = c.IsCapital,
            IsPowered = c.IsPowered,
            IsGoldenAge = c.IsGoldenAge,
            Population = c.Population,
            Latitude = c.Latitude,
            Options = Set(w.Options),
            ReligionLevels = w.ReligionLevels,
            BuildingCounts = e.BuildingCounts,
            UnitCounts = e.UnitCounts,
            Counts = counts,
        };
    }

    private static IReadOnlySet<string> Set(List<string> items) => new HashSet<string>(items, StringComparer.Ordinal);
}
