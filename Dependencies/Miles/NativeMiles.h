// SPDX-License-Identifier: GPL-3.0-or-later
// Native (AMD64) implementation of the part of the Miles Sound System API the original engine uses.
//
// The retail mss32.dll only exists for 32 bit Windows. The engine's audio layer (MilesAudioManager) is written
// against the AIL_* API, which MilesLoader forwards to function pointers. On AMD64 those pointers are bound to
// this implementation instead of mss32.dll, so the original audio manager (event limits, priorities, fading,
// music transitions, 3D attenuation, completion callbacks) runs unchanged on a small software mixer:
//
//   * sample / 3D sample / stream voices mixed on a mixer thread into 44.1 kHz stereo 16 bit
//   * output through waveOut (winmm); when no output device opens the mixer still runs in real time so that
//     completion callbacks, which game flow waits on, keep firing
//   * WAV PCM (8/16 bit) and IMA ADPCM decoding; MP3 music through Media Foundation, decoded progressively
//   * positional audio: distance attenuation (min/max distance) and panning from the listener orientation
//
// Everything is deterministic without a device: SetDeviceless(true) + Render() mix a requested number of
// frames, which is what the offline tests use.
#pragma once

#if defined(_WIN64)
#include "mss/mss.h"
#include <cstdint>

namespace NativeMiles
{
// ---- test and diagnostics hooks -------------------------------------------------------------------------
void SetDevicelessForTests(bool deviceless);    // no waveOut / no mixer thread; Render() drives the mixer
unsigned Render(int16_t *interleavedStereo, unsigned frames);  // mixes and delivers EOS callbacks
unsigned ActiveVoiceCount();
unsigned OutputRate();
struct Stats {
    unsigned samplesStarted, streamsOpened, mp3Decoded, eosCallbacks, underruns, deviceOpened;
    unsigned handlesAllocated, handlesReleased;     // voice handles over the driver's lifetime
    unsigned active, peakActive;                    // voices currently / at most mixing
    unsigned callbacksQueued, callbacksDelivered;   // completion callbacks handed to the delivery thread / run
    unsigned starved;                               // output device ran dry (audible dropout)
};
Stats stats();
void resetStats();

// ---- WAV / ADPCM helpers (also used by the tests) --------------------------------------------------------
// Decodes a WAV image (PCM 8/16 bit or IMA ADPCM) to 16 bit PCM. Returns false when unsupported.
bool DecodeWav(const void *image, uint32_t size, int16_t **pcm, uint32_t *frames, uint32_t *rate, uint32_t *channels);
void FreePcm(int16_t *pcm);

// ---- the AIL API ----------------------------------------------------------------------------------------
F32 __stdcall AIL_3D_sample_volume(H3DSAMPLE sample);
void __stdcall AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume);
void __stdcall AIL_end_3D_sample(H3DSAMPLE sample);
void __stdcall AIL_resume_3D_sample(H3DSAMPLE sample);
void __stdcall AIL_stop_3D_sample(H3DSAMPLE sample);
void __stdcall AIL_start_3D_sample(H3DSAMPLE sample);
U32 __stdcall AIL_3D_sample_loop_count(H3DSAMPLE sample);
void __stdcall AIL_set_3D_sample_offset(H3DSAMPLE sample, U32 offset);
U32 __stdcall AIL_3D_sample_length(H3DSAMPLE sample);
U32 __stdcall AIL_3D_sample_offset(H3DSAMPLE sample);
S32 __stdcall AIL_3D_sample_playback_rate(H3DSAMPLE sample);
void __stdcall AIL_set_3D_sample_playback_rate(H3DSAMPLE sample, S32 playback_rate);
S32 __stdcall AIL_set_3D_sample_file(H3DSAMPLE sample, const void *file_image);
HPROVIDER __stdcall AIL_set_sample_processor(HSAMPLE sample, SAMPLESTAGE pipeline_stage, HPROVIDER provider);
void __stdcall AIL_set_filter_sample_preference(HSAMPLE sample, const char *name, const void *val);
void __stdcall AIL_release_sample_handle(HSAMPLE sample);
void __stdcall AIL_close_3D_provider(HPROVIDER lib);
S32 __stdcall AIL_set_preference(U32 number, S32 value);
void __stdcall AIL_set_3D_sample_loop_count(H3DSAMPLE sample, U32 count);
void __stdcall AIL_set_stream_playback_rate(HSTREAM stream, S32 rate);
S32 __stdcall AIL_stream_playback_rate(HSTREAM stream);
void __stdcall AIL_stream_ms_position(HSTREAM stream, S32 *total_milliseconds, S32 *current_milliseconds);
void __stdcall AIL_set_stream_ms_position(HSTREAM stream, S32 pos);
S32 __stdcall AIL_stream_loop_count(HSTREAM stream);
void __stdcall AIL_set_stream_loop_block(HSTREAM stream, S32 loop_start, S32 loop_end);
void __stdcall AIL_set_stream_loop_count(HSTREAM stream, S32 count);
void __stdcall AIL_close_stream(HSTREAM stream);
void __stdcall AIL_pause_stream(HSTREAM stream, S32 onoff);
AIL_stream_callback __stdcall AIL_register_stream_callback(HSTREAM stream, AIL_stream_callback callback);
AIL_3dsample_callback __stdcall AIL_register_3D_EOS_callback(H3DSAMPLE sample, AIL_3dsample_callback EOS);
AIL_sample_callback __stdcall AIL_register_EOS_callback(HSAMPLE sample, AIL_sample_callback EOS);
void __stdcall AIL_start_stream(HSTREAM stream);
void __stdcall AIL_set_sample_playback_rate(HSAMPLE sample, S32 playback_rate);
S32 __stdcall AIL_sample_playback_rate(HSAMPLE sample);
void __stdcall AIL_sample_ms_position(HSAMPLE sample, S32 *total_ms, S32 *current_ms);
void __stdcall AIL_set_sample_ms_position(HSAMPLE sample, S32 pos);
S32 __stdcall AIL_sample_loop_count(HSAMPLE sample);
void __stdcall AIL_set_sample_loop_count(HSAMPLE sample, S32 count);
void __stdcall AIL_end_sample(HSAMPLE sample);
void __stdcall AIL_resume_sample(HSAMPLE sample);
void __stdcall AIL_stop_sample(HSAMPLE sample);
void __stdcall AIL_start_sample(HSAMPLE sample);
void __stdcall AIL_init_sample(HSAMPLE sample);
S32 __stdcall AIL_set_named_sample_file(HSAMPLE sample, const char *file_name, const void *file_image, S32 file_size, S32 block);
void __stdcall AIL_set_3D_sample_effects_level(H3DSAMPLE sample, F32 effect_level);
void __stdcall AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist);
void __stdcall AIL_set_3D_velocity_vector(H3DPOBJECT obj, F32 x, F32 y, F32 z);
void __stdcall AIL_set_3D_position(H3DPOBJECT obj, F32 X, F32 Y, F32 Z);
void __stdcall AIL_set_3D_orientation(H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face, F32 X_up, F32 Y_up, F32 Z_up);
S32 __stdcall AIL_WAV_info(const void *data, AILSOUNDINFO *info);
void __stdcall AIL_stop_timer(HTIMER timer);
void __stdcall AIL_release_timer_handle(HTIMER timer);
void __stdcall AIL_shutdown(void);
S32 __stdcall AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name);
void __stdcall AIL_set_file_callbacks(AIL_file_open_callback opencb, AIL_file_close_callback closecb,
                                      AIL_file_seek_callback seekcb, AIL_file_read_callback readcb);
void __stdcall AIL_release_3D_sample_handle(H3DSAMPLE sample);
H3DSAMPLE __stdcall AIL_allocate_3D_sample_handle(HPROVIDER lib);
void __stdcall AIL_set_3D_user_data(H3DPOBJECT obj, U32 index, S32 value);
void __stdcall AIL_unlock(void);
void __stdcall AIL_unlock_mutex(void);
void __stdcall AIL_lock(void);
void __stdcall AIL_lock_mutex(void);
void __stdcall AIL_set_3D_speaker_type(HPROVIDER lib, S32 speaker_type);
void __stdcall AIL_close_3D_listener(H3DPOBJECT listener);
S32 __stdcall AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name);
M3DRESULT __stdcall AIL_open_3D_provider(HPROVIDER lib);
char *__stdcall AIL_last_error(void);
H3DPOBJECT __stdcall AIL_open_3D_listener(HPROVIDER lib);
S32 __stdcall AIL_3D_user_data(H3DPOBJECT obj, U32 index);
S32 __stdcall AIL_sample_user_data(HSAMPLE sample, U32 index);
HSAMPLE __stdcall AIL_allocate_sample_handle(HDIGDRIVER dig);
void __stdcall AIL_set_sample_user_data(HSAMPLE sample, U32 index, S32 value);
S32 __stdcall AIL_decompress_ADPCM(const AILSOUNDINFO *info, void **outdata, U32 *outsize);
void __stdcall AIL_get_DirectSound_info(HSAMPLE sample, AILLPDIRECTSOUND *lplpDS, AILLPDIRECTSOUNDBUFFER *lplpDSB);
void __stdcall AIL_mem_free_lock(void *ptr);
HSTREAM __stdcall AIL_open_stream(HDIGDRIVER dig, const char *filename, S32 stream_mem);
S32 __stdcall AIL_startup(void);
void __stdcall AIL_quick_unload(HAUDIO audio);
HAUDIO __stdcall AIL_quick_load_and_play(const char *filename, U32 loop_count, S32 wait_request);
void __stdcall AIL_quick_set_volume(HAUDIO audio, F32 volume, F32 extravol);
S32 __stdcall AIL_quick_startup(S32 use_digital, S32 use_MIDI, U32 output_rate, S32 output_bits, S32 output_channels);
void __stdcall AIL_quick_handles(HDIGDRIVER *pdig, HMDIDRIVER *pmdi, HDLSDEVICE *pdls);
void __stdcall AIL_sample_volume_pan(HSAMPLE sample, F32 *volume, F32 *pan);
void __stdcall AIL_set_3D_sample_occlusion(H3DSAMPLE sample, F32 occlusion);
char *__stdcall AIL_set_redist_directory(const char *dir);
S32 __stdcall AIL_set_sample_file(HSAMPLE sample, const void *file_image, S32 block);
void __stdcall AIL_set_sample_volume_pan(HSAMPLE sample, F32 volume, F32 pan);
void __stdcall AIL_set_stream_volume_pan(HSTREAM stream, F32 volume, F32 pan);
void __stdcall AIL_stream_volume_pan(HSTREAM stream, F32 *volume, F32 *pan);
U32 __stdcall AIL_get_timer_highest_delay(void);
}
#endif
