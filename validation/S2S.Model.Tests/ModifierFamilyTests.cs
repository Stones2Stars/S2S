using S2S.Model;
using Xunit;

namespace S2S.Model.Tests;

/// <summary>Verifies the modifier families parse out of the raw bag into the typed tree, on real data.</summary>
public sealed class ModifierFamilyTests
{
    private static readonly InfoRepository Repo = InfoLoader.LoadBuildings(TestPaths.AssetsDataDir());

    [Fact]
    public void Bakery_Food_Family_Tree()
    {
        ModifierFamily food = Repo.Buildings["BUILDING_BAKERY"].Modifiers["food"];
        ModifierNode city = food.Root.Children["city"];

        Assert.Contains(city.Magnitudes, m => m.Unit == "flat" && m.Value == 3);

        ModifierNode farm = city.Children["improvements"].Children["IMPROVEMENT_FARM"];
        Assert.Contains(farm.Magnitudes, m => m.Unit == "flat" && m.Value == 1);
    }

    [Fact]
    public void Bakery_Commerce_Is_A_Conditioned_List()
    {
        ModifierFamily commerce = Repo.Buildings["BUILDING_BAKERY"].Modifiers["commerce"];
        var flats = commerce.Root.Children["city"].Magnitudes.Where(m => m.Unit == "flat").ToList();

        Assert.Equal(4, flats.Count);                      // one per vicinity fruit bonus
        Assert.All(flats, m => Assert.Equal(1, m.Value));
        Assert.All(flats, m => Assert.IsType<Atom>(m.Enabled));   // each conditioned on a vicinity-bonus atom
    }

    [Fact]
    public void Athenian_Culture_Flat()
    {
        ModifierFamily culture = Repo.Buildings["BUILDING_CULTURE_ATHENIAN"].Modifiers["culture"];
        Assert.Contains(culture.Root.Children["city"].Magnitudes, m => m.Unit == "flat" && m.Value == 5);
    }

    [Fact]
    public void All_Buildings_Modifiers_Parse_Without_Error()
    {
        long families = 0;
        foreach (BuildingInfo b in Repo.Buildings.Values)
            families += b.Modifiers.Count;   // reaching here means ParseAll succeeded for every building
        Assert.True(families > 0, "expected at least some modifier families across the corpus");
    }
}
