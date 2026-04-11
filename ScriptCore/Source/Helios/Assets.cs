namespace Helios;

/// <summary>
/// Handle to a loaded asset. Returned by Assets.Load().
/// </summary>
public readonly struct AssetHandle
{
    public readonly ulong Id;
    public AssetHandle(ulong id) => Id = id;
    public bool IsValid => Id != 0;
    public bool IsLoaded => NativeAPI.AssetIsLoaded(Id);
}

/// <summary>
/// Generic asset loading API. Mirrors the C++ AssetServer.
/// Load assets by path, get a handle, check readiness.
/// </summary>
public static class Assets
{
    /// <summary>
    /// Load an asset asynchronously by path (relative to asset root).
    /// Returns a handle immediately. Poll IsLoaded or use in OnUpdate.
    /// </summary>
    public static AssetHandle Load(string path) => new(NativeAPI.AssetLoad(path));
}
