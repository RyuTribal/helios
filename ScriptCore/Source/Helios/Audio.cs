using System.Numerics;

namespace Helios;

/// <summary>
/// Audio playback API for scripts.
/// </summary>
public static class Audio
{
    /// <summary>
    /// Play a pre-loaded audio asset at a 3D position.
    /// Load the asset in OnCreate via Assets.Load().
    /// </summary>
    public static void Play(AssetHandle sound, Vector3 position, float volume = 1.0f, bool loop = false)
        => NativeAPI.AudioPlayHandle(sound.Id, position.X, position.Y, position.Z, volume, loop);

    /// <summary>Play a pre-loaded audio asset at origin.</summary>
    public static void Play(AssetHandle sound, float volume = 1.0f, bool loop = false)
        => NativeAPI.AudioPlayHandle(sound.Id, 0, 0, 0, volume, loop);

    /// <summary>
    /// Convenience: load + play by path. First call may be silent if not cached.
    /// Prefer Assets.Load() in OnCreate + Play(handle) in OnUpdate/OnCollision.
    /// </summary>
    public static void Play(string path, Vector3 position, float volume = 1.0f, bool loop = false)
        => NativeAPI.AudioPlayFile(path, position.X, position.Y, position.Z, volume, loop);
}
