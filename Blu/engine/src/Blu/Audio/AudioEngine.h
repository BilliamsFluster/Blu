#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Blu
{
    // Opaque handle for a loaded sound resource.
    // Value 0 is always invalid.
    using SoundHandle = uint32_t;
    static constexpr SoundHandle kInvalidSound = 0;

    class AudioEngine
    {
    public:
        static AudioEngine& Get();

        void Initialize();
        void Shutdown();
        bool IsBackendCompiled() const;

        // --- Resource management -----------------------------------------
        // Loads a sound from disk; returns a handle that must be released with
        // UnloadSound when no longer needed.  Returns kInvalidSound on failure.
        SoundHandle LoadSound(const std::string& filepath);
        void        UnloadSound(SoundHandle handle);

        // --- Playback ------------------------------------------------------
        void Play  (SoundHandle handle);
        void Stop  (SoundHandle handle);
        void Pause (SoundHandle handle);
        void Resume(SoundHandle handle);

        bool IsPlaying(SoundHandle handle) const;
        bool IsPaused (SoundHandle handle) const;
        bool IsFinished(SoundHandle handle) const;

        // --- Sound properties ----------------------------------------------
        void SetLooping(SoundHandle handle, bool loop);
        void SetVolume (SoundHandle handle, float volume);   // [0,1]
        void SetPitch  (SoundHandle handle, float pitch);    // 1.0 = normal

        // --- Master / bus --------------------------------------------------
        void SetMasterVolume(float volume);
        float GetMasterVolume() const;

        // --- 3-D spatial audio ---------------------------------------------
        // Call once per frame from Scene::OnUpdate with the active camera transform.
        void SetListenerTransform(const glm::vec3& position,
                                  const glm::vec3& forward,
                                  const glm::vec3& up);

        // Per-sound 3D settings. Attenuation is linear between minDist/maxDist.
        void SetSpatial     (SoundHandle handle, bool spatial);
        void SetSoundPosition(SoundHandle handle, const glm::vec3& worldPos);
        void SetAttenuation (SoundHandle handle, float minDist, float maxDist);

        // --- Frame update --------------------------------------------------
        // Call once per engine frame; no-op when miniaudio is absent.
        void OnUpdate();

    private:
        AudioEngine() = default;
        ~AudioEngine() = default;
        AudioEngine(const AudioEngine&) = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        // PIMPL: opaque pointer to internal miniaudio state.
        // Sized generously; the actual ma_engine is ~256 bytes on most platforms.
        struct Impl;
        Impl* m_Impl = nullptr;
    };
}
