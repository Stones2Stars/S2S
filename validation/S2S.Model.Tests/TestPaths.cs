namespace S2S.Model.Tests;

/// <summary>Locates the repo's <c>Assets/Data</c> by walking up from the test assembly location.</summary>
internal static class TestPaths
{
    public static string AssetsDataDir()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "Assets", "Data")))
            dir = dir.Parent;
        if (dir is null) throw new DirectoryNotFoundException("could not locate repo root (Assets/Data)");
        return Path.Combine(dir.FullName, "Assets", "Data");
    }
}
