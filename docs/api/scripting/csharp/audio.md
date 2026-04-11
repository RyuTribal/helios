# Audio

The `Audio` class is a static helper for playing sound effects and background music.

## Methods

| Method | Description |
| :--- | :--- |
| `void Play(AssetHandle, Vector3, float, bool)` | Play a sound at a 3D position with volume and looping. |
| `void Play(AssetHandle, float, bool)` | Play a sound at the origin (0, 0, 0). |
| `void Play(string, Vector3, float, bool)` | Play a sound by its file path at a 3D position. |

## Usage

For the best performance, sounds should be pre-loaded as assets and played using their handles.

```csharp
public class SoundTrigger : ScriptBehaviour {
    private AssetHandle m_explosionSound;

    public override void OnCreate() {
        // Pre-load the sound asset
        m_explosionSound = Assets.Load("Sounds/Explosion.wav");
    }

    public override void OnCollisionEnter(ulong otherId, Vector3 point, Vector3 normal, float impulse) {
        if (impulse > 10.0f) {
            Audio.Play(m_explosionSound, point, 1.0f);
        }
    }
}
```

### Note on 3D Audio
The Helios engine handles 3D spatialization automatically. Sounds played with a position will be attenuated based on the distance between the entity and the audio listener (usually the camera).

## Related Pages

- [Entity](entity.md)
- [Asset Management](../../core/assets/asset_server.md)
- [Audio API (C++)](../../audio/index.md)
