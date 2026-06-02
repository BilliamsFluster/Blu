#include "Blupch.h"
#include "AudioEngine.h"
#include "Blu/Core/Log.h"

// ---------------------------------------------------------------------------
// miniaudio integration
// miniaudio is compiled once from ExternalDependencies/miniaudio/miniaudio.c.
// ---------------------------------------------------------------------------
#ifdef BLU_HAS_MINIAUDIO
#pragma warning(push)
#pragma warning(disable : 4244 4267 4996)
#include "../../ExternalDependencies/miniaudio/miniaudio.h"
#pragma warning(pop)
#endif // BLU_HAS_MINIAUDIO

#include <unordered_map>
#include <atomic>

namespace Blu
{

// ---------------------------------------------------------------------------
// Internal implementation struct (PIMPL)
// ---------------------------------------------------------------------------
struct AudioEngine::Impl
{
#ifdef BLU_HAS_MINIAUDIO
    ma_engine  Engine;
    bool       Initialized = false;

    struct SoundEntry
    {
        ma_sound  Sound;
        bool      Spatial  = false;
        bool      Valid    = false;
    };
    std::unordered_map<SoundHandle, SoundEntry> Sounds;
    std::atomic<SoundHandle>                    NextHandle{ 1 };

    SoundEntry* Get(SoundHandle h)
    {
        auto it = Sounds.find(h);
        return (it != Sounds.end() && it->second.Valid) ? &it->second : nullptr;
    }
#else
    // Stub placeholders so the rest of the .cpp compiles.
    bool Initialized = false;
#endif
};

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
AudioEngine& AudioEngine::Get()
{
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::IsBackendCompiled() const
{
#ifdef BLU_HAS_MINIAUDIO
    return true;
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------
void AudioEngine::Initialize()
{
    if (m_Impl) return;
    m_Impl = new Impl();

#ifdef BLU_HAS_MINIAUDIO
    ma_result result = ma_engine_init(nullptr, &m_Impl->Engine);
    if (result != MA_SUCCESS)
    {
        BLU_CORE_ERROR("AudioEngine: ma_engine_init failed (code {0})", (int)result);
        delete m_Impl;
        m_Impl = nullptr;
        return;
    }
    m_Impl->Initialized = true;
    BLU_CORE_INFO("AudioEngine: initialized (miniaudio {0})", ma_version_string());
#else
    BLU_CORE_WARN("AudioEngine: miniaudio not compiled in — audio is a no-op. "
                  "Add miniaudio.h to ExternalDependencies/miniaudio/ and define BLU_HAS_MINIAUDIO.");
#endif
}

void AudioEngine::Shutdown()
{
    if (!m_Impl) return;

#ifdef BLU_HAS_MINIAUDIO
    if (m_Impl->Initialized)
    {
        for (auto& [handle, entry] : m_Impl->Sounds)
        {
            if (entry.Valid)
                ma_sound_uninit(&entry.Sound);
        }
        m_Impl->Sounds.clear();
        ma_engine_uninit(&m_Impl->Engine);
    }
#endif
    delete m_Impl;
    m_Impl = nullptr;
}

// ---------------------------------------------------------------------------
// Resource management
// ---------------------------------------------------------------------------
SoundHandle AudioEngine::LoadSound(const std::string& filepath)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl || !m_Impl->Initialized) return kInvalidSound;

    SoundHandle h = m_Impl->NextHandle.fetch_add(1);
    auto& entry   = m_Impl->Sounds[h];

    ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
    ma_result res   = ma_sound_init_from_file(&m_Impl->Engine, filepath.c_str(),
                                               flags, nullptr, nullptr, &entry.Sound);
    if (res != MA_SUCCESS)
    {
        BLU_CORE_ERROR("AudioEngine: failed to load '{0}' (code {1})", filepath, (int)res);
        m_Impl->Sounds.erase(h);
        return kInvalidSound;
    }
    entry.Valid   = true;
    entry.Spatial = false;
    return h;
#else
    (void)filepath;
    return kInvalidSound;
#endif
}

void AudioEngine::UnloadSound(SoundHandle handle)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    auto* e = m_Impl->Get(handle);
    if (!e) return;
    ma_sound_uninit(&e->Sound);
    m_Impl->Sounds.erase(handle);
#else
    (void)handle;
#endif
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------
void AudioEngine::Play(SoundHandle handle)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
    {
        ma_sound_seek_to_pcm_frame(&e->Sound, 0);
        ma_sound_start(&e->Sound);
    }
#else
    (void)handle;
#endif
}

void AudioEngine::Stop(SoundHandle handle)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
    {
        ma_sound_stop(&e->Sound);
        ma_sound_seek_to_pcm_frame(&e->Sound, 0);
    }
#else
    (void)handle;
#endif
}

void AudioEngine::Pause(SoundHandle handle)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
        ma_sound_stop(&e->Sound);
#else
    (void)handle;
#endif
}

void AudioEngine::Resume(SoundHandle handle)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
        ma_sound_start(&e->Sound);
#else
    (void)handle;
#endif
}

bool AudioEngine::IsPlaying(SoundHandle handle) const
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return false;
    if (auto* e = m_Impl->Get(handle))
        return ma_sound_is_playing(&e->Sound) == MA_TRUE;
#else
    (void)handle;
#endif
    return false;
}

bool AudioEngine::IsPaused(SoundHandle handle) const
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return false;
    if (auto* e = m_Impl->Get(handle))
        return ma_sound_is_playing(&e->Sound) == MA_FALSE &&
               ma_sound_at_end(&e->Sound) == MA_FALSE;
#else
    (void)handle;
#endif
    return false;
}

bool AudioEngine::IsFinished(SoundHandle handle) const
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return true;
    if (auto* e = m_Impl->Get(handle))
        return ma_sound_at_end(&e->Sound) == MA_TRUE;
#else
    (void)handle;
#endif
    return true;
}

// ---------------------------------------------------------------------------
// Sound properties
// ---------------------------------------------------------------------------
void AudioEngine::SetLooping(SoundHandle handle, bool loop)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
        ma_sound_set_looping(&e->Sound, loop ? MA_TRUE : MA_FALSE);
#else
    (void)handle; (void)loop;
#endif
}

void AudioEngine::SetVolume(SoundHandle handle, float volume)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
        ma_sound_set_volume(&e->Sound, volume);
#else
    (void)handle; (void)volume;
#endif
}

void AudioEngine::SetPitch(SoundHandle handle, float pitch)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
        ma_sound_set_pitch(&e->Sound, pitch);
#else
    (void)handle; (void)pitch;
#endif
}

// ---------------------------------------------------------------------------
// Master volume
// ---------------------------------------------------------------------------
void AudioEngine::SetMasterVolume(float volume)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl || !m_Impl->Initialized) return;
    ma_engine_set_volume(&m_Impl->Engine, volume);
#else
    (void)volume;
#endif
}

float AudioEngine::GetMasterVolume() const
{
#ifdef BLU_HAS_MINIAUDIO
    if (m_Impl && m_Impl->Initialized)
        return ma_engine_get_volume(&m_Impl->Engine);
#endif
    return 1.0f;
}

// ---------------------------------------------------------------------------
// 3-D spatial
// ---------------------------------------------------------------------------
void AudioEngine::SetListenerTransform(const glm::vec3& position,
                                       const glm::vec3& forward,
                                       const glm::vec3& up)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl || !m_Impl->Initialized) return;
    ma_engine_listener_set_position (&m_Impl->Engine, 0,
        position.x, position.y, position.z);
    ma_engine_listener_set_direction(&m_Impl->Engine, 0,
        forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up (&m_Impl->Engine, 0,
        up.x, up.y, up.z);
#else
    (void)position; (void)forward; (void)up;
#endif
}

void AudioEngine::SetSpatial(SoundHandle handle, bool spatial)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
    {
        e->Spatial = spatial;
        ma_sound_set_spatialization_enabled(&e->Sound, spatial ? MA_TRUE : MA_FALSE);
    }
#else
    (void)handle; (void)spatial;
#endif
}

void AudioEngine::SetSoundPosition(SoundHandle handle, const glm::vec3& worldPos)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
        ma_sound_set_position(&e->Sound, worldPos.x, worldPos.y, worldPos.z);
#else
    (void)handle; (void)worldPos;
#endif
}

void AudioEngine::SetAttenuation(SoundHandle handle, float minDist, float maxDist)
{
#ifdef BLU_HAS_MINIAUDIO
    if (!m_Impl) return;
    if (auto* e = m_Impl->Get(handle))
    {
        ma_sound_set_min_distance(&e->Sound, minDist);
        ma_sound_set_max_distance(&e->Sound, maxDist);
    }
#else
    (void)handle; (void)minDist; (void)maxDist;
#endif
}

// ---------------------------------------------------------------------------
// Frame update
// ---------------------------------------------------------------------------
void AudioEngine::OnUpdate()
{
    // miniaudio processes audio on a dedicated thread; nothing to pump here.
    // Reserved for future per-frame callbacks (e.g. streaming, 3D updates).
}

} // namespace Blu
