using S2S.Model;
using Xunit;

namespace S2S.Model.Tests;

/// <summary>Evaluates the condition tree-walk against hand-built states, using REAL building conditions where
/// possible (bakery operate, athenian build) plus synthetic atoms for band/count/group coverage.</summary>
public sealed class ConditionEvaluatorTests
{
    private static readonly InfoRepository Repo = InfoLoader.LoadBuildings(TestPaths.AssetsDataDir());
    private static readonly ConditionEvaluator Eval = new();

    [Fact]
    public void Bakery_Operate_Needs_Connected_Flour()
    {
        Condition? op = Repo.Buildings["BUILDING_BAKERY"].Requires!.Operate;

        Assert.True(Eval.Evaluate(op, new EvalState { Bonuses = Set("BONUS_FLOUR") }));         // trade-connected
        Assert.True(Eval.Evaluate(op, new EvalState { VicinityBonuses = Set("BONUS_FLOUR") })); // vicinity
        Assert.False(Eval.Evaluate(op, new EvalState()));                                        // none → dormant
    }

    [Fact]
    public void Athenian_Build_Gate()
    {
        Condition? build = Repo.Buildings["BUILDING_CULTURE_ATHENIAN"].Requires!.Build;
        var state = new EvalState
        {
            Techs = Set("TECH_COPPER_WORKING"),
            ConstructedBuildings = Set("BUILDING_C_AC_EUROPEAN", "BUILDING_C_L_EUROPEAN"),
        };
        var coast = new Plot { Coast = true };   // uncategorized map → MAPCATEGORY_EARTH passes

        Assert.True(Eval.Evaluate(build, state, coast));
        Assert.False(Eval.Evaluate(build, state, new Plot { Coast = false }));                          // no coast
        Assert.False(Eval.Evaluate(build, new EvalState { Techs = Set("TECH_COPPER_WORKING") }, coast)); // missing prereq buildings
    }

    [Fact]
    public void Property_Band_Atom()
    {
        var band = new ConditionGroup(All: [new Atom("PROPERTY_CRIME", "city", Min: 400, Max: 1950)]);
        Assert.True(Eval.Evaluate(band, new EvalState { Counts = Counts(("PROPERTY_CRIME", 500)) }));
        Assert.False(Eval.Evaluate(band, new EvalState { Counts = Counts(("PROPERTY_CRIME", 100)) }));  // below band
        Assert.False(Eval.Evaluate(band, new EvalState { Counts = Counts(("PROPERTY_CRIME", 2000)) })); // above band
    }

    [Fact]
    public void Count_Atom_Population_Band()
    {
        var pop5 = new Atom("POPULATION", "city", Min: 5);
        Assert.True(Eval.Evaluate(pop5, new EvalState { Population = 6 }));
        Assert.False(Eval.Evaluate(pop5, new EvalState { Population = 3 }));
    }

    [Fact]
    public void Group_All_Any_NoneOf()
    {
        var cond = new ConditionGroup(
            All: [new Atom("TECH_POTTERY", "team")],
            Any: [[new Atom("CIVIC_SLAVERY", "empire"), new Atom("CIVIC_SERFDOM", "empire")]],
            NoneOf: [new Atom("BUILDING_FORGE", "city")]);

        Assert.True(Eval.Evaluate(cond, new EvalState { Techs = Set("TECH_POTTERY"), Civics = Set("CIVIC_SERFDOM") }));
        Assert.False(Eval.Evaluate(cond, new EvalState { Techs = Set("TECH_POTTERY") }));   // no any-group member
        Assert.False(Eval.Evaluate(cond, new EvalState
        {
            Techs = Set("TECH_POTTERY"),
            Civics = Set("CIVIC_SLAVERY"),
            ConstructedBuildings = Set("BUILDING_FORGE"),   // noneOf hit
        }));
    }

    private static IReadOnlySet<string> Set(params string[] items) => new HashSet<string>(items);

    private static IReadOnlyDictionary<string, int> Counts(params (string Key, int Value)[] items)
        => items.ToDictionary(x => x.Key, x => x.Value);
}
