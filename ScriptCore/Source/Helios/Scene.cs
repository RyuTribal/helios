namespace Helios;

/// <summary>
/// Runtime scene management.
/// </summary>
public static class Scene
{
    /// <summary>
    /// Load a scene, replacing the current one.
    /// Path relative to asset root (e.g. "Scenes/Main_Scene.hvescn").
    /// </summary>
    public static void Load(string path) => NativeAPI.SceneLoad(path);

    /// <summary>
    /// Instantiate a scene as a sub-scene under the current active scene.
    /// </summary>
    public static void Instantiate(string path) => NativeAPI.SceneInstantiate(path);

    /// <summary>
    /// Preload a scene's assets and check readiness.
    /// First call parses the scene file and kicks off async asset loads.
    /// Returns true when all assets are loaded and the scene is ready to switch to.
    /// Use in OnUpdate: if (Scene.IsReady("Scenes/lion.hvescn")) Scene.Load(...);
    /// </summary>
    public static bool IsReady(string path) => NativeAPI.SceneIsReady(path);
}
