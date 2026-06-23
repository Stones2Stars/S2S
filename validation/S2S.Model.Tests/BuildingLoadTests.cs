using S2S.Model;
using Xunit;

namespace S2S.Model.Tests;

/// <summary>Verifies the typed Info model loads the REAL curated data (Assets/Data/buildings/**) completely,
/// and that the cascade-critical sections deserialize to the expected typed shapes.</summary>
public sealed class BuildingLoadTests
{
    // Loaded once for the class (≈2,600 files) rather than per-test.
    private static readonly InfoRepository Repo = InfoLoader.LoadBuildings(TestPaths.AssetsDataDir());

    [Fact]
    public void All_Buildings_Deserialize_Without_Error()
    {
        // LoadBuildings throws (with the offending file) if any file fails — reaching here means all parsed.
        Assert.True(Repo.Count > 2000, $"expected >2000 buildings, got {Repo.Count}");
    }

    [Fact]
    public void Bakery_Parses_Expected_Sections()
    {
        BuildingInfo bakery = Repo.Buildings["BUILDING_BAKERY"];

        Assert.Equal("TXT_KEY_BUILDING_BAKERY", bakery.Description);
        Assert.NotNull(bakery.Enables);
        Assert.Contains("BUILDING_GUILD_BAKERS", bakery.Enables!["buildings"]);

        // requires.operate: connected flour (trade|vicinity)
        var operate = Assert.IsType<ConditionGroup>(bakery.Requires!.Operate);
        var flour = Assert.IsType<Atom>(operate.All![0]);
        Assert.Equal("BONUS_FLOUR", flour.Type);
        Assert.Equal("trade|vicinity", flour.Connection);

        // the food modifier family is captured raw (extension data) for now
        Assert.NotNull(bakery.Families);
        Assert.True(bakery.Families!.ContainsKey("food"));
    }

    [Fact]
    public void Athenian_BarePredicate_And_AllowedCap()
    {
        BuildingInfo ath = Repo.Buildings["BUILDING_CULTURE_ATHENIAN"];

        // requires.build.all contains the bare-string predicate HAS_COAST alongside typed atoms
        var build = Assert.IsType<ConditionGroup>(ath.Requires!.Build);
        Assert.Contains(build.All!, c => c is BarePredicate { Name: "HAS_COAST" });

        // allowed: { world: 1 }
        Assert.NotNull(ath.Allowed);
        Assert.Equal(1, ath.Allowed!["world"]);
    }
}
