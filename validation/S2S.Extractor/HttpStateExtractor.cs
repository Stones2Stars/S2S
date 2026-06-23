namespace S2S.Extractor;

/// <summary>
/// <see cref="IStateExtractor"/> implemented over HTTP against the live surface
/// (default <c>http://127.0.0.1:7227</c>). Holds no file/console concerns — it returns raw JSON
/// for any purpose, so it can sit behind a CLI, an API, or anything else.
/// </summary>
public sealed class HttpStateExtractor : IStateExtractor, IDisposable
{
    public const string DefaultBaseUrl = "http://127.0.0.1:7227";

    private readonly HttpClient _http;
    private readonly bool _ownsClient;

    /// <summary>Use a caller-supplied <see cref="HttpClient"/> (the host owns its lifetime).</summary>
    public HttpStateExtractor(HttpClient http) => _http = http;

    /// <summary>Convenience: own an <see cref="HttpClient"/> pointed at <paramref name="baseUrl"/>.</summary>
    public HttpStateExtractor(string baseUrl = DefaultBaseUrl)
    {
        _http = new HttpClient { BaseAddress = new Uri(baseUrl), Timeout = TimeSpan.FromSeconds(60) };
        _ownsClient = true;
    }

    public async Task<bool> IsAliveAsync(CancellationToken ct = default)
    {
        try
        {
            string body = await _http.GetStringAsync("/", ct).ConfigureAwait(false);
            return body.Contains("hello world", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    public Task<string> FetchGameStateAsync(CancellationToken ct = default)
        => FetchAsync("/extractor/gamestate", ct);

    public Task<string> FetchAsync(string path, CancellationToken ct = default)
        => _http.GetStringAsync(path, ct);

    public void Dispose()
    {
        if (_ownsClient)
        {
            _http.Dispose();
        }
    }
}
