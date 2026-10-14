// SPDX-License-Identifier: GPL-3.0-or-later
// See NativeMiles.h. Software mixer behind the AIL_* API for the AMD64 build.
#if defined(_WIN64)

#include "NativeMiles.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace NativeMiles
{
namespace
{
constexpr unsigned kOutRate = 44100;
constexpr unsigned kBufferFrames = 1024;
constexpr unsigned kBufferCount = 6;

// ------------------------------------------------------------------------------------------------
// PCM storage
// ------------------------------------------------------------------------------------------------
struct PcmData
{
    std::vector<int16_t> samples;      // interleaved, `channels` per frame
    unsigned channels = 1;
    unsigned rate = 22050;
    std::atomic<size_t> framesReady{0};   // progressive decode publishes through this
    std::atomic<bool> finished{true};
    double totalMs = 0.0;
    size_t frames() const { return framesReady.load(); }
};

struct Listener
{
    float position[3] = {0, 0, 0};
    float face[3] = {0, 1, 0};
    float up[3] = {0, 0, 1};
};

enum class Kind : unsigned char { Free, Sample2D, Sample3D, Stream, Listener, Quick };
enum class State : unsigned char { Stopped, Playing, Paused, Done };

struct Voice
{
    Kind kind = Kind::Free;
    State state = State::Stopped;
    std::shared_ptr<PcmData> pcm;
    double position = 0.0;            // source frames
    int loopCount = 1;                // 0 = infinite, otherwise total plays
    int loopsDone = 0;
    float volume = 1.0f, pan = 0.5f;
    int playbackRate = 0;             // 0 = native
    // positional
    float pos[3] = {0, 0, 0};
    float minDist = 1.0f, maxDist = 1000.0f;
    float occlusion = 0.0f;
    S32 userData[8] = {};
    // callbacks (stored as raw pointers; invoked outside the lock)
    void *eos = nullptr;
    // stream decode support
    std::thread decoder;
    std::atomic<bool> cancel{false};
    Listener listener;                // used when kind == Listener
};

CRITICAL_SECTION g_lock;
bool g_lockInit = false;
std::vector<std::unique_ptr<Voice>> g_voices;
Voice *g_listenerVoice = nullptr;
DIG_DRIVER g_driver = {};
bool g_started = false;
bool g_deviceless = false;
Stats g_stats = {};
std::string g_lastError;

AIL_file_open_callback g_openCb = nullptr;
AIL_file_close_callback g_closeCb = nullptr;
AIL_file_seek_callback g_seekCb = nullptr;
AIL_file_read_callback g_readCb = nullptr;

struct Lock
{
    Lock() { if (g_lockInit) EnterCriticalSection(&g_lock); }
    ~Lock() { if (g_lockInit) LeaveCriticalSection(&g_lock); }
};

void EnsureLock()
{
    if (!g_lockInit) { InitializeCriticalSection(&g_lock); g_lockInit = true; }
}

Voice *Allocate(Kind kind)
{
    EnsureLock();
    Lock lock;
    ++g_stats.handlesAllocated;
    auto voice = std::make_unique<Voice>();
    voice->kind = kind;
    Voice *raw = voice.get();
    g_voices.push_back(std::move(voice));
    return raw;
}

Voice *Find(const void *handle)
{
    for (auto &v : g_voices)
        if (v.get() == handle && v->kind != Kind::Free) return v.get();
    return nullptr;
}

void StopDecoder(Voice &v)
{
    v.cancel = true;
    if (v.decoder.joinable()) v.decoder.join();
    v.cancel = false;
}

void ReleaseVoice(Voice *voice)
{
    if (!voice) return;
    StopDecoder(*voice);
    Lock lock;
    for (auto it = g_voices.begin(); it != g_voices.end(); ++it)
        if (it->get() == voice) {
            if (g_listenerVoice == voice) g_listenerVoice = nullptr;
            ++g_stats.handlesReleased;
            g_voices.erase(it);
            return;
        }
}

// ------------------------------------------------------------------------------------------------
// WAV / IMA ADPCM
// ------------------------------------------------------------------------------------------------
struct WavInfo
{
    uint16_t tag = 0, channels = 0, blockAlign = 0, bits = 0;
    uint32_t rate = 0, dataLen = 0, factSamples = 0;
    const uint8_t *data = nullptr;
    const uint8_t *initial = nullptr;
};

uint32_t Le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t Le16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

bool ParseWav(const void *image, uint32_t size, WavInfo &out)
{
    const uint8_t *p = static_cast<const uint8_t *>(image);
    if (!p || size < 12 || std::memcmp(p, "RIFF", 4) || std::memcmp(p + 8, "WAVE", 4)) return false;
    uint32_t at = 12;
    bool haveFmt = false, haveData = false;
    while (at + 8 <= size) {
        const uint32_t chunk = Le32(p + at + 4);
        const uint8_t *body = p + at + 8;
        const uint32_t avail = size - (at + 8);
        const uint32_t len = chunk > avail ? avail : chunk;
        if (!std::memcmp(p + at, "fmt ", 4) && len >= 16) {
            out.tag = Le16(body); out.channels = Le16(body + 2); out.rate = Le32(body + 4);
            out.blockAlign = Le16(body + 12); out.bits = Le16(body + 14);
            haveFmt = true;
        } else if (!std::memcmp(p + at, "fact", 4) && len >= 4) {
            out.factSamples = Le32(body);
        } else if (!std::memcmp(p + at, "data", 4)) {
            out.data = body; out.dataLen = len; out.initial = p; haveData = true;
            if (haveFmt) break;
        }
        at += 8 + chunk + (chunk & 1);
    }
    return haveFmt && haveData && out.channels >= 1 && out.channels <= 2 && out.rate > 0;
}

const int kStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
const int kIndexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

inline int16_t ImaStep(int &predictor, int &index, unsigned nibble)
{
    int step = kStepTable[index];
    int diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) predictor -= diff; else predictor += diff;
    predictor = predictor < -32768 ? -32768 : (predictor > 32767 ? 32767 : predictor);
    index += kIndexTable[nibble];
    index = index < 0 ? 0 : (index > 88 ? 88 : index);
    return static_cast<int16_t>(predictor);
}

// IMA ADPCM (WAVE_FORMAT 0x11): per block, per channel a 4 byte header (predictor, step index), then groups of
// 4 bytes (8 samples) per channel interleaved.
bool DecodeIma(const WavInfo &w, std::vector<int16_t> &out)
{
    const unsigned ch = w.channels;
    if (!w.blockAlign || w.blockAlign < 4u * ch) return false;
    const unsigned samplesPerBlock = (w.blockAlign - 4u * ch) * 8u / (4u * ch) + 1u;
    const size_t blocks = w.dataLen / w.blockAlign;
    out.reserve(blocks * samplesPerBlock * ch);
    std::vector<int16_t> block(static_cast<size_t>(samplesPerBlock) * ch);
    for (size_t b = 0; b < blocks; ++b) {
        const uint8_t *src = w.data + b * w.blockAlign;
        int predictor[2] = {0, 0}, index[2] = {0, 0};
        for (unsigned c = 0; c < ch; ++c) {
            predictor[c] = static_cast<int16_t>(Le16(src + c * 4));
            index[c] = src[c * 4 + 2] > 88 ? 88 : src[c * 4 + 2];
            block[c] = static_cast<int16_t>(predictor[c]);
        }
        const uint8_t *data = src + 4u * ch;
        unsigned frame = 1;
        const uint8_t *end = src + w.blockAlign;
        while (data + 4u * ch <= end && frame < samplesPerBlock) {
            for (unsigned c = 0; c < ch; ++c) {
                for (unsigned i = 0; i < 4; ++i) {
                    const uint8_t byte = data[c * 4 + i];
                    const unsigned f = frame + i * 2;
                    if (f < samplesPerBlock) block[static_cast<size_t>(f) * ch + c] = ImaStep(predictor[c], index[c], byte & 15);
                    if (f + 1 < samplesPerBlock) block[static_cast<size_t>(f + 1) * ch + c] = ImaStep(predictor[c], index[c], byte >> 4);
                }
            }
            data += 4u * ch;
            frame += 8;
        }
        out.insert(out.end(), block.begin(), block.end());
    }
    return true;
}

bool DecodeWavToPcm(const void *image, uint32_t size, PcmData &pcm)
{
    WavInfo w;
    if (!ParseWav(image, size, w)) return false;
    pcm.channels = w.channels;
    pcm.rate = w.rate;
    if (w.tag == 1 && w.bits == 16) {
        const size_t count = w.dataLen / 2;
        pcm.samples.resize(count - count % w.channels);
        std::memcpy(pcm.samples.data(), w.data, pcm.samples.size() * 2);
    } else if (w.tag == 1 && w.bits == 8) {
        pcm.samples.resize(w.dataLen - w.dataLen % w.channels);
        for (size_t i = 0; i < pcm.samples.size(); ++i) pcm.samples[i] = static_cast<int16_t>((static_cast<int>(w.data[i]) - 128) << 8);
    } else if (w.tag == 0x11 && w.bits == 4) {
        if (!DecodeIma(w, pcm.samples)) return false;
        if (w.factSamples && static_cast<size_t>(w.factSamples) * w.channels < pcm.samples.size())
            pcm.samples.resize(static_cast<size_t>(w.factSamples) * w.channels);
    } else {
        return false;
    }
    pcm.framesReady = pcm.samples.size() / pcm.channels;
    pcm.finished = true;
    pcm.totalMs = pcm.rate ? 1000.0 * static_cast<double>(pcm.framesReady.load()) / pcm.rate : 0.0;
    return true;
}

// Duration of a WAV image without decoding it (what AIL_open_stream + AIL_stream_ms_position reports).
double WavDurationMs(const void *image, uint32_t size)
{
    WavInfo w;
    if (!ParseWav(image, size, w)) return 0.0;
    double frames = 0.0;
    if (w.tag == 0x11 && w.blockAlign >= 4u * w.channels) {
        const unsigned spb = (w.blockAlign - 4u * w.channels) * 8u / (4u * w.channels) + 1u;
        frames = static_cast<double>(w.dataLen / w.blockAlign) * spb;
        if (w.factSamples && w.factSamples < frames) frames = w.factSamples;
    } else if (w.bits >= 8) {
        frames = static_cast<double>(w.dataLen) / (w.channels * (w.bits / 8));
    }
    return 1000.0 * frames / w.rate;
}

// ------------------------------------------------------------------------------------------------
// MP3 through Media Foundation (progressive)
// ------------------------------------------------------------------------------------------------
struct Mp3Source
{
    IMFSourceReader *reader = nullptr;
    IStream *stream = nullptr;
    IMFByteStream *bytes = nullptr;
    unsigned channels = 2, rate = 44100;
    double durationMs = 0.0;
    ~Mp3Source()
    {
        if (reader) reader->Release();
        if (bytes) bytes->Release();
        if (stream) stream->Release();
    }
};

bool g_mfStarted = false;

// Media Foundation is resolved at run time: Windows "N" editions ship without it, and the executable must
// keep starting there (music then plays silently).
struct MfApi
{
    HRESULT(WINAPI *Startup)(ULONG, DWORD) = nullptr;
    HRESULT(WINAPI *CreateMediaType)(IMFMediaType **) = nullptr;
    HRESULT(WINAPI *CreateByteStream)(IUnknown *, IMFByteStream **) = nullptr;
    HRESULT(WINAPI *CreateReader)(IMFByteStream *, IMFAttributes *, IMFSourceReader **) = nullptr;
    bool ok = false;
};
MfApi &Mf()
{
    static MfApi api;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE plat = LoadLibraryA("mfplat.dll");
        HMODULE rw = LoadLibraryA("mfreadwrite.dll");
        if (plat && rw) {
            api.Startup = reinterpret_cast<decltype(api.Startup)>(GetProcAddress(plat, "MFStartup"));
            api.CreateMediaType = reinterpret_cast<decltype(api.CreateMediaType)>(GetProcAddress(plat, "MFCreateMediaType"));
            api.CreateByteStream = reinterpret_cast<decltype(api.CreateByteStream)>(GetProcAddress(plat, "MFCreateMFByteStreamOnStream"));
            api.CreateReader = reinterpret_cast<decltype(api.CreateReader)>(GetProcAddress(rw, "MFCreateSourceReaderFromByteStream"));
            api.ok = api.Startup && api.CreateMediaType && api.CreateByteStream && api.CreateReader;
        }
    }
    return api;
}

bool OpenMp3(const std::vector<uint8_t> &file, Mp3Source &out)
{
    MfApi &mf = Mf();
    if (!mf.ok) return false;
    if (!g_mfStarted) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(mf.Startup(MF_VERSION, MFSTARTUP_LITE))) return false;
        g_mfStarted = true;
    }
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, file.size());
    if (!mem) return false;
    if (void *p = GlobalLock(mem)) { std::memcpy(p, file.data(), file.size()); GlobalUnlock(mem); }
    if (FAILED(CreateStreamOnHGlobal(mem, TRUE, &out.stream))) { GlobalFree(mem); return false; }
    if (FAILED(mf.CreateByteStream(out.stream, &out.bytes))) return false;
    if (FAILED(mf.CreateReader(out.bytes, nullptr, &out.reader))) return false;
    out.reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    out.reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);
    IMFMediaType *native = nullptr;
    if (SUCCEEDED(out.reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &native))) {
        UINT32 v = 0;
        if (SUCCEEDED(native->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &v)) && v >= 1 && v <= 2) out.channels = v;
        if (SUCCEEDED(native->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &v)) && v) out.rate = v;
        native->Release();
    }
    IMFMediaType *wanted = nullptr;
    if (FAILED(mf.CreateMediaType(&wanted))) return false;
    wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    wanted->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    wanted->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, out.channels);
    wanted->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, out.rate);
    wanted->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, out.channels * 2);
    wanted->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, out.rate * out.channels * 2);
    const HRESULT hr = out.reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, wanted);
    wanted->Release();
    if (FAILED(hr)) return false;
    PROPVARIANT var;
    PropVariantInit(&var);
    if (SUCCEEDED(out.reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &var)) &&
        var.vt == VT_UI8)
        out.durationMs = static_cast<double>(var.uhVal.QuadPart) / 10000.0;
    PropVariantClear(&var);
    return true;
}

void DecodeMp3Thread(std::shared_ptr<Mp3Source> source, std::shared_ptr<PcmData> pcm, std::atomic<bool> *cancel)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::vector<int16_t> chunk;
    for (;;) {
        if (cancel->load()) break;
        DWORD flags = 0;
        IMFSample *sample = nullptr;
        const HRESULT hr = source->reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags,
                                                       nullptr, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) break;
        if (sample) {
            IMFMediaBuffer *buffer = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
                BYTE *data = nullptr;
                DWORD length = 0;
                if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
                    const size_t count = length / 2;
                    chunk.assign(reinterpret_cast<int16_t *>(data), reinterpret_cast<int16_t *>(data) + count);
                    buffer->Unlock();
                    // Single writer: samples are only appended; framesReady publishes the new length. The vector is
                    // pre-reserved so a reader never sees a reallocation.
                    if (pcm->samples.capacity() - pcm->samples.size() >= chunk.size()) {
                        pcm->samples.insert(pcm->samples.end(), chunk.begin(), chunk.end());
                        pcm->framesReady = pcm->samples.size() / pcm->channels;
                    }
                }
                buffer->Release();
            }
            sample->Release();
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
    }
    pcm->totalMs = pcm->rate ? 1000.0 * static_cast<double>(pcm->framesReady.load()) / pcm->rate : pcm->totalMs;
    pcm->finished = true;
    CoUninitialize();
}

// ------------------------------------------------------------------------------------------------
// File reading through the engine's file callbacks
// ------------------------------------------------------------------------------------------------
bool ReadWholeFile(const char *name, std::vector<uint8_t> &out)
{
    if (g_openCb && g_readCb && g_closeCb) {
        void *handle = nullptr;
        if (!g_openCb(name, &handle) || !handle) return false;
        // The engine's streaming files report their size through seek-to-end.
        S32 size = g_seekCb ? g_seekCb(handle, 0, AIL_FILE_SEEK_END) : 0;
        if (g_seekCb) g_seekCb(handle, 0, AIL_FILE_SEEK_BEGIN);
        if (size <= 0) {
            // Unknown size: read in blocks.
            uint8_t block[16384];
            for (;;) {
                const U32 n = g_readCb(handle, block, sizeof(block));
                if (!n) break;
                out.insert(out.end(), block, block + n);
            }
        } else {
            out.resize(static_cast<size_t>(size));
            size_t done = 0;
            while (done < out.size()) {
                const U32 n = g_readCb(handle, out.data() + done, static_cast<U32>(out.size() - done));
                if (!n) break;
                done += n;
            }
            out.resize(done);
        }
        g_closeCb(handle);
        return !out.empty();
    }
    FILE *f = nullptr;
    if (fopen_s(&f, name, "rb") != 0 || !f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
        out.resize(static_cast<size_t>(size));
        out.resize(std::fread(out.data(), 1, out.size(), f));
    }
    std::fclose(f);
    return !out.empty();
}

// ------------------------------------------------------------------------------------------------
// Mixer
// ------------------------------------------------------------------------------------------------
struct PendingEos { void *callback; void *handle; Kind kind; };

void InvokeEos(const PendingEos &e)
{
    switch (e.kind) {
    case Kind::Sample2D: reinterpret_cast<AIL_sample_callback>(e.callback)(reinterpret_cast<HSAMPLE>(e.handle)); break;
    case Kind::Sample3D: reinterpret_cast<AIL_3dsample_callback>(e.callback)(reinterpret_cast<H3DPOBJECT>(e.handle)); break;
    case Kind::Stream: reinterpret_cast<AIL_stream_callback>(e.callback)(reinterpret_cast<HSTREAM>(e.handle)); break;
    default: break;
    }
    ++g_stats.callbacksDelivered;
}

// The engine's completion handler loads the next portion of a sound (file I/O, decode) synchronously. Running it on
// the mixer thread starved the output device (millisecond dropouts). A dedicated thread delivers completions
// instead; deviceless/test mode delivers inline so tests stay deterministic.
struct EosQueue
{
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<PendingEos> jobs;
    std::thread thread;
    bool running = false;
} g_eosQueue;

void EosWorker()
{
    for (;;) {
        PendingEos job;
        {
            std::unique_lock<std::mutex> lock(g_eosQueue.mutex);
            g_eosQueue.wake.wait(lock, [] { return !g_eosQueue.jobs.empty() || !g_eosQueue.running; });
            if (g_eosQueue.jobs.empty()) return;   // stopped and drained
            job = g_eosQueue.jobs.front();
            g_eosQueue.jobs.pop_front();
        }
        InvokeEos(job);
    }
}

void StartEosWorker()
{
    std::lock_guard<std::mutex> lock(g_eosQueue.mutex);
    if (g_eosQueue.running) return;
    g_eosQueue.running = true;
    g_eosQueue.thread = std::thread(EosWorker);
}

void StopEosWorker()
{
    {
        std::lock_guard<std::mutex> lock(g_eosQueue.mutex);
        if (!g_eosQueue.running) return;
        g_eosQueue.running = false;
        g_eosQueue.jobs.clear();   // handles die with the driver
    }
    g_eosQueue.wake.notify_all();
    if (g_eosQueue.thread.joinable()) g_eosQueue.thread.join();
}

void DeliverEos(const PendingEos &e)
{
    ++g_stats.eosCallbacks;
    bool async = false;
    {
        std::lock_guard<std::mutex> lock(g_eosQueue.mutex);
        if (g_eosQueue.running) { g_eosQueue.jobs.push_back(e); async = true; }
    }
    if (async) { ++g_stats.callbacksQueued; g_eosQueue.wake.notify_one(); }
    else InvokeEos(e);
}

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void Gains(const Voice &v, float &left, float &right)
{
    float volume = v.volume;
    float pan = v.pan;
    if (v.kind == Kind::Sample3D && g_listenerVoice) {
        const Listener &l = g_listenerVoice->listener;
        float d[3] = {v.pos[0] - l.position[0], v.pos[1] - l.position[1], v.pos[2] - l.position[2]};
        const float dist = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        // Miles: full volume inside the minimum distance, inverse-distance roll-off beyond it, and no further
        // attenuation past the maximum distance (the application culls what should be inaudible; the engine's
        // own audibility rules do that). Treating max distance as a hard cutoff silenced off-screen combat.
        float attenuation = 1.0f;
        const float effective = (dist > v.maxDist && v.maxDist > v.minDist) ? v.maxDist : dist;
        if (effective > v.minDist) attenuation = v.minDist > 0.0f ? v.minDist / effective : 0.0f;
        volume *= attenuation * (1.0f - Clamp(v.occlusion, 0.0f, 1.0f));
        // Pan from the listener's right vector (face x up).
        const float f[3] = {l.face[0], l.face[1], l.face[2]};
        const float u[3] = {l.up[0], l.up[1], l.up[2]};
        float r[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]};
        const float rl = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        if (dist > 1e-4f && rl > 1e-6f) {
            const float side = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / (dist * rl);
            pan = 0.5f + 0.5f * Clamp(side, -1.0f, 1.0f) * 0.85f;   // never fully hard-panned
        } else {
            pan = 0.5f;
        }
    }
    left = volume * std::min(1.0f, 2.0f * (1.0f - pan));
    right = volume * std::min(1.0f, 2.0f * pan);
}

void MixVoice(Voice &v, float *accum, unsigned frames, std::vector<PendingEos> &eos)
{
    if (v.state != State::Playing || !v.pcm) return;
    PcmData &pcm = *v.pcm;
    float gl, gr;
    Gains(v, gl, gr);
    const double rate = v.playbackRate > 0 ? static_cast<double>(v.playbackRate) : static_cast<double>(pcm.rate);
    const double step = rate / kOutRate;
    const unsigned ch = pcm.channels;
    for (unsigned i = 0; i < frames; ++i) {
        const size_t total = pcm.frames();
        size_t p = static_cast<size_t>(v.position);
        if (p >= total) {
            if (!pcm.finished.load()) { ++g_stats.underruns; return; }   // waiting for the decoder
            ++v.loopsDone;
            if (v.loopCount == 0 || v.loopsDone < v.loopCount) {
                v.position = 0.0;
                p = 0;
                if (total == 0) { v.state = State::Done; break; }
            } else {
                v.state = State::Done;
                break;
            }
        }
        const double frac = v.position - static_cast<double>(p);
        const size_t next = p + 1 < total ? p + 1 : p;
        float s0, s1;
        if (ch == 1) {
            const float a = pcm.samples[p], b = pcm.samples[next];
            s0 = s1 = (a + (b - a) * static_cast<float>(frac)) * (1.0f / 32768.0f);
            accum[i * 2] += s0 * gl;
            accum[i * 2 + 1] += s1 * gr;
        } else {
            const float la = pcm.samples[p * 2], lb = pcm.samples[next * 2];
            const float ra = pcm.samples[p * 2 + 1], rb = pcm.samples[next * 2 + 1];
            accum[i * 2] += (la + (lb - la) * static_cast<float>(frac)) * (1.0f / 32768.0f) * gl;
            accum[i * 2 + 1] += (ra + (rb - ra) * static_cast<float>(frac)) * (1.0f / 32768.0f) * gr;
        }
        v.position += step;
    }
    if (v.state == State::Done && v.eos) eos.push_back({v.eos, &v, v.kind});
}

unsigned RenderInternal(int16_t *out, unsigned frames)
{
    std::vector<float> accum(static_cast<size_t>(frames) * 2, 0.0f);
    std::vector<PendingEos> eos;
    {
        Lock lock;
        unsigned active = 0;
        for (auto &v : g_voices)
            if (v->kind == Kind::Sample2D || v->kind == Kind::Sample3D || v->kind == Kind::Stream || v->kind == Kind::Quick) {
                if (v->state == State::Playing) ++active;
                MixVoice(*v, accum.data(), frames, eos);
            }
        g_stats.active = active;
        if (active > g_stats.peakActive) g_stats.peakActive = active;
    }
    for (size_t i = 0; i < accum.size(); ++i) {
        const float s = Clamp(accum[i], -1.0f, 1.0f);
        out[i] = static_cast<int16_t>(s * 32767.0f);
    }
    for (const PendingEos &e : eos) DeliverEos(e);
    return frames;
}

// --- output device thread -----------------------------------------------------------------------
struct Device
{
    HWAVEOUT out = nullptr;
    HANDLE event = nullptr;
    std::thread thread;
    std::atomic<bool> running{false};
    WAVEHDR headers[kBufferCount] = {};
    std::vector<int16_t> buffers[kBufferCount];
};
Device *g_device = nullptr;

void DeviceThread(Device *d)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    double owed = 0.0;
    std::vector<int16_t> scratch(kBufferFrames * 2);
    while (d->running.load()) {
        if (d->out) {
            WaitForSingleObject(d->event, 50);
            unsigned drained = 0;
            for (unsigned i = 0; i < kBufferCount; ++i)
                if ((d->headers[i].dwFlags & WHDR_PREPARED) && (d->headers[i].dwFlags & WHDR_DONE)) ++drained;
            if (drained == kBufferCount) ++g_stats.starved;   // the device ran out of queued audio: an audible dropout
            for (unsigned i = 0; i < kBufferCount; ++i) {
                WAVEHDR &h = d->headers[i];
                if (h.dwFlags & WHDR_DONE || !(h.dwFlags & WHDR_PREPARED)) {
                    if (h.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(d->out, &h, sizeof(h));
                    RenderInternal(d->buffers[i].data(), kBufferFrames);
                    h.lpData = reinterpret_cast<LPSTR>(d->buffers[i].data());
                    h.dwBufferLength = kBufferFrames * 4;
                    h.dwFlags = 0;
                    waveOutPrepareHeader(d->out, &h, sizeof(h));
                    waveOutWrite(d->out, &h, sizeof(h));
                }
            }
        } else {
            // No output device: keep real-time pacing so completion callbacks arrive when they would have.
            Sleep(8);
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            owed += static_cast<double>(now.QuadPart - last.QuadPart) * kOutRate / static_cast<double>(freq.QuadPart);
            last = now;
            while (owed >= kBufferFrames) {
                RenderInternal(scratch.data(), kBufferFrames);
                owed -= kBufferFrames;
            }
        }
    }
}

void StartDevice()
{
    if (g_device || g_deviceless) return;
    g_device = new Device;
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = kOutRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = kOutRate * 4;
    g_device->event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (waveOutOpen(&g_device->out, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(g_device->event), 0, CALLBACK_EVENT) == MMSYSERR_NOERROR) {
        g_stats.deviceOpened = 1;
        for (unsigned i = 0; i < kBufferCount; ++i) g_device->buffers[i].assign(kBufferFrames * 2, 0);
    } else {
        g_device->out = nullptr;
    }
    StartEosWorker();
    g_device->running = true;
    g_device->thread = std::thread(DeviceThread, g_device);
}

void StopDevice()
{
    if (!g_device) return;
    g_device->running = false;
    SetEvent(g_device->event);
    if (g_device->thread.joinable()) g_device->thread.join();
    StopEosWorker();
    if (g_device->out) {
        waveOutReset(g_device->out);
        for (unsigned i = 0; i < kBufferCount; ++i)
            if (g_device->headers[i].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(g_device->out, &g_device->headers[i], sizeof(WAVEHDR));
        waveOutClose(g_device->out);
    }
    CloseHandle(g_device->event);
    delete g_device;
    g_device = nullptr;
}

bool LoadSampleData(const void *handle, const void *image)
{
    if (!handle || !image) return false;
    // The image is a complete WAV file; its size is not passed, so measure it from the RIFF header.
    const uint8_t *p = static_cast<const uint8_t *>(image);
    if (std::memcmp(p, "RIFF", 4)) return false;
    const uint32_t size = Le32(p + 4) + 8;
    auto pcm = std::make_shared<PcmData>();
    if (!DecodeWavToPcm(image, size, *pcm)) return false;   // decoded before taking the mixer lock
    Lock lock;
    Voice *v = Find(handle);
    if (!v) return false;
    v->pcm = pcm;
    v->position = 0.0;
    v->loopsDone = 0;
    // Miles: a newly set file plays at its own native rate. A pooled handle must not keep the pitch-shifted
    // rate (in Hz, of the previous clip) - that made every following clip play too slow or too fast.
    v->playbackRate = 0;
    return true;
}
} // namespace

// ================================================================================================
// public helpers
// ================================================================================================
void SetDevicelessForTests(bool deviceless) { g_deviceless = deviceless; }
unsigned Render(int16_t *out, unsigned frames) { EnsureLock(); return RenderInternal(out, frames); }
unsigned ActiveVoiceCount()
{
    EnsureLock();
    Lock lock;
    unsigned n = 0;
    for (auto &v : g_voices)
        if (v->state == State::Playing) ++n;
    return n;
}
unsigned OutputRate() { return kOutRate; }
Stats stats() { return g_stats; }
void resetStats() { g_stats = Stats{}; }

bool DecodeWav(const void *image, uint32_t size, int16_t **pcmOut, uint32_t *frames, uint32_t *rate, uint32_t *channels)
{
    PcmData pcm;
    if (!DecodeWavToPcm(image, size, pcm)) return false;
    *frames = static_cast<uint32_t>(pcm.frames());
    *rate = pcm.rate;
    *channels = pcm.channels;
    *pcmOut = new int16_t[pcm.samples.size() ? pcm.samples.size() : 1];
    std::memcpy(*pcmOut, pcm.samples.data(), pcm.samples.size() * 2);
    return true;
}
void FreePcm(int16_t *pcm) { delete[] pcm; }

// ================================================================================================
// AIL API
// ================================================================================================
S32 __stdcall AIL_startup(void)
{
    EnsureLock();
    if (!g_started) {
        g_started = true;
        std::memset(&g_driver, 0, sizeof(g_driver));
        g_driver.emulated_ds = 1;   // software mixing, like the "fast 2D" provider on hardware without DS3D
        if (!g_listenerVoice) {
            g_listenerVoice = Allocate(Kind::Listener);
        }
        StartDevice();
    }
    return 1;
}

void __stdcall AIL_shutdown(void)
{
    if (!g_started) return;
    StopDevice();
    {
        EnsureLock();
        std::vector<std::unique_ptr<Voice>> voices;
        {
            Lock lock;
            voices.swap(g_voices);
            g_listenerVoice = nullptr;
        }
        for (auto &v : voices) StopDecoder(*v);
    }
    g_started = false;
}

S32 __stdcall AIL_quick_startup(S32, S32, U32, S32, S32) { return NativeMiles::AIL_startup(); }
void __stdcall AIL_quick_handles(HDIGDRIVER *pdig, HMDIDRIVER *pmdi, HDLSDEVICE *pdls)
{
    if (pdig) *pdig = &g_driver;
    if (pmdi) *pmdi = nullptr;
    if (pdls) *pdls = nullptr;
}
char *__stdcall AIL_set_redist_directory(const char *) { static char empty[1] = {0}; return empty; }
S32 __stdcall AIL_set_preference(U32, S32) { return 0; }
char *__stdcall AIL_last_error(void) { return const_cast<char *>(g_lastError.c_str()); }
U32 __stdcall AIL_get_timer_highest_delay(void) { return 0; }
void __stdcall AIL_stop_timer(HTIMER) {}
void __stdcall AIL_release_timer_handle(HTIMER) {}
void __stdcall AIL_lock(void) { EnsureLock(); EnterCriticalSection(&g_lock); }
void __stdcall AIL_unlock(void) { if (g_lockInit) LeaveCriticalSection(&g_lock); }
void __stdcall AIL_lock_mutex(void) { NativeMiles::AIL_lock(); }
void __stdcall AIL_unlock_mutex(void) { NativeMiles::AIL_unlock(); }
void __stdcall AIL_set_file_callbacks(AIL_file_open_callback o, AIL_file_close_callback c, AIL_file_seek_callback s, AIL_file_read_callback r)
{
    g_openCb = o; g_closeCb = c; g_seekCb = s; g_readCb = r;
}
void __stdcall AIL_mem_free_lock(void *ptr) { delete[] static_cast<uint8_t *>(ptr); }
void __stdcall AIL_get_DirectSound_info(HSAMPLE, AILLPDIRECTSOUND *ds, AILLPDIRECTSOUNDBUFFER *dsb)
{
    if (ds) *ds = nullptr;
    if (dsb) *dsb = nullptr;
}

// ---- providers / filters / listener ----
S32 __stdcall AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
    static char providerName[] = "Miles Fast 2D Positional Audio";
    if (!next || *next != HPROENUM_FIRST) return 0;
    if (dest) *dest = reinterpret_cast<HPROVIDER>(static_cast<uintptr_t>(1));
    if (name) *name = providerName;
    *next = 1;
    return 1;
}
M3DRESULT __stdcall AIL_open_3D_provider(HPROVIDER) { return M3D_NOERR; }
void __stdcall AIL_close_3D_provider(HPROVIDER) {}
void __stdcall AIL_set_3D_speaker_type(HPROVIDER, S32) {}
S32 __stdcall AIL_enumerate_filters(HPROENUM *, HPROVIDER *, char **) { return 0; }
HPROVIDER __stdcall AIL_set_sample_processor(HSAMPLE, SAMPLESTAGE, HPROVIDER) { return nullptr; }
void __stdcall AIL_set_filter_sample_preference(HSAMPLE, const char *, const void *) {}
H3DPOBJECT __stdcall AIL_open_3D_listener(HPROVIDER)
{
    EnsureLock();
    Lock lock;
    if (!g_listenerVoice) g_listenerVoice = Allocate(Kind::Listener);
    return reinterpret_cast<H3DPOBJECT>(g_listenerVoice);
}
void __stdcall AIL_close_3D_listener(H3DPOBJECT) {}

// ---- 3D object positioning (listener or sample) ----
void __stdcall AIL_set_3D_position(H3DPOBJECT obj, F32 x, F32 y, F32 z)
{
    Lock lock;
    Voice *v = Find(obj);
    if (!v) return;
    if (v->kind == Kind::Listener) { v->listener.position[0] = x; v->listener.position[1] = y; v->listener.position[2] = z; }
    else { v->pos[0] = x; v->pos[1] = y; v->pos[2] = z; }
}
void __stdcall AIL_set_3D_orientation(H3DPOBJECT obj, F32 xf, F32 yf, F32 zf, F32 xu, F32 yu, F32 zu)
{
    Lock lock;
    Voice *v = Find(obj);
    if (v && v->kind == Kind::Listener) {
        v->listener.face[0] = xf; v->listener.face[1] = yf; v->listener.face[2] = zf;
        v->listener.up[0] = xu; v->listener.up[1] = yu; v->listener.up[2] = zu;
    }
}
void __stdcall AIL_set_3D_velocity_vector(H3DPOBJECT, F32, F32, F32) {}
void __stdcall AIL_set_3D_user_data(H3DPOBJECT obj, U32 index, S32 value)
{
    Lock lock;
    if (Voice *v = Find(obj)) if (index < 8) v->userData[index] = value;
}
S32 __stdcall AIL_3D_user_data(H3DPOBJECT obj, U32 index)
{
    Lock lock;
    Voice *v = Find(obj);
    return v && index < 8 ? v->userData[index] : 0;
}

// ---- 2D samples ----
HSAMPLE __stdcall AIL_allocate_sample_handle(HDIGDRIVER) { return reinterpret_cast<HSAMPLE>(Allocate(Kind::Sample2D)); }
void __stdcall AIL_release_sample_handle(HSAMPLE s) { ReleaseVoice(Find(s)); }
void __stdcall AIL_init_sample(HSAMPLE s)
{
    Lock lock;
    Voice *v = Find(s);
    if (!v) return;
    v->state = State::Stopped;
    v->pcm.reset();
    v->position = 0.0;
    v->loopCount = 1;
    v->loopsDone = 0;
    v->volume = 1.0f;
    v->pan = 0.5f;
    v->playbackRate = 0;
    v->eos = nullptr;
}
void __stdcall AIL_set_sample_user_data(HSAMPLE s, U32 index, S32 value) { NativeMiles::AIL_set_3D_user_data(reinterpret_cast<H3DPOBJECT>(s), index, value); }
S32 __stdcall AIL_sample_user_data(HSAMPLE s, U32 index) { return NativeMiles::AIL_3D_user_data(reinterpret_cast<H3DPOBJECT>(s), index); }
AIL_sample_callback __stdcall AIL_register_EOS_callback(HSAMPLE s, AIL_sample_callback cb)
{
    Lock lock;
    Voice *v = Find(s);
    if (!v) return nullptr;
    auto previous = reinterpret_cast<AIL_sample_callback>(v->eos);
    v->eos = reinterpret_cast<void *>(cb);
    return previous;
}
S32 __stdcall AIL_set_sample_file(HSAMPLE s, const void *image, S32)
{
    return LoadSampleData(s, image) ? 1 : 0;
}
S32 __stdcall AIL_set_named_sample_file(HSAMPLE s, const char *, const void *image, S32, S32 block) { return NativeMiles::AIL_set_sample_file(s, image, block); }
void __stdcall AIL_start_sample(HSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) if (v->pcm) { v->state = State::Playing; ++g_stats.samplesStarted; }
}
void __stdcall AIL_stop_sample(HSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) if (v->state == State::Playing) v->state = State::Paused;
}
void __stdcall AIL_resume_sample(HSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) if (v->state == State::Paused) v->state = State::Playing;
}
void __stdcall AIL_end_sample(HSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) { v->state = State::Stopped; v->position = 0.0; }
}
void __stdcall AIL_set_sample_volume_pan(HSAMPLE s, F32 volume, F32 pan)
{
    Lock lock;
    if (Voice *v = Find(s)) { v->volume = Clamp(volume, 0.0f, 4.0f); v->pan = Clamp(pan, 0.0f, 1.0f); }
}
void __stdcall AIL_sample_volume_pan(HSAMPLE s, F32 *volume, F32 *pan)
{
    Lock lock;
    Voice *v = Find(s);
    if (volume) *volume = v ? v->volume : 0.0f;
    if (pan) *pan = v ? v->pan : 0.5f;
}
void __stdcall AIL_set_sample_playback_rate(HSAMPLE s, S32 rate)
{
    Lock lock;
    if (Voice *v = Find(s)) v->playbackRate = rate;
}
S32 __stdcall AIL_sample_playback_rate(HSAMPLE s)
{
    Lock lock;
    Voice *v = Find(s);
    return v ? (v->playbackRate > 0 ? v->playbackRate : (v->pcm ? static_cast<S32>(v->pcm->rate) : 0)) : 0;
}
S32 __stdcall AIL_sample_loop_count(HSAMPLE s)
{
    Lock lock;
    Voice *v = Find(s);
    return v ? v->loopCount : 1;
}
void __stdcall AIL_set_sample_loop_count(HSAMPLE s, S32 count)
{
    Lock lock;
    if (Voice *v = Find(s)) v->loopCount = count;
}
void __stdcall AIL_sample_ms_position(HSAMPLE s, S32 *total, S32 *current)
{
    Lock lock;
    Voice *v = Find(s);
    if (total) *total = v && v->pcm ? static_cast<S32>(v->pcm->totalMs) : 0;
    if (current) *current = v && v->pcm && v->pcm->rate ? static_cast<S32>(1000.0 * v->position / v->pcm->rate) : 0;
}
void __stdcall AIL_set_sample_ms_position(HSAMPLE s, S32 pos)
{
    Lock lock;
    Voice *v = Find(s);
    if (v && v->pcm) v->position = static_cast<double>(pos) * v->pcm->rate / 1000.0;
}

// ---- 3D samples ----
H3DSAMPLE __stdcall AIL_allocate_3D_sample_handle(HPROVIDER) { return reinterpret_cast<H3DSAMPLE>(Allocate(Kind::Sample3D)); }
void __stdcall AIL_release_3D_sample_handle(H3DSAMPLE s) { ReleaseVoice(Find(s)); }
AIL_3dsample_callback __stdcall AIL_register_3D_EOS_callback(H3DSAMPLE s, AIL_3dsample_callback cb)
{
    Lock lock;
    Voice *v = Find(s);
    if (!v) return nullptr;
    auto previous = reinterpret_cast<AIL_3dsample_callback>(v->eos);
    v->eos = reinterpret_cast<void *>(cb);
    return previous;
}
S32 __stdcall AIL_set_3D_sample_file(H3DSAMPLE s, const void *image)
{
    if (!LoadSampleData(s, image)) return 0;
    Lock lock;
    if (Voice *v = Find(s)) v->state = State::Stopped;
    return 1;
}
void __stdcall AIL_start_3D_sample(H3DSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) if (v->pcm) { v->state = State::Playing; ++g_stats.samplesStarted; }
}
void __stdcall AIL_stop_3D_sample(H3DSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) if (v->state == State::Playing) v->state = State::Paused;
}
void __stdcall AIL_resume_3D_sample(H3DSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) if (v->state == State::Paused) v->state = State::Playing;
}
void __stdcall AIL_end_3D_sample(H3DSAMPLE s)
{
    Lock lock;
    if (Voice *v = Find(s)) { v->state = State::Stopped; v->position = 0.0; }
}
void __stdcall AIL_set_3D_sample_volume(H3DSAMPLE s, F32 volume)
{
    Lock lock;
    if (Voice *v = Find(s)) v->volume = Clamp(volume, 0.0f, 4.0f);
}
F32 __stdcall AIL_3D_sample_volume(H3DSAMPLE s)
{
    Lock lock;
    Voice *v = Find(s);
    return v ? v->volume : 0.0f;
}
void __stdcall AIL_set_3D_sample_distances(H3DSAMPLE s, F32 maxDist, F32 minDist)
{
    Lock lock;
    if (Voice *v = Find(s)) { v->maxDist = maxDist; v->minDist = minDist; }
}
void __stdcall AIL_set_3D_sample_occlusion(H3DSAMPLE s, F32 occlusion)
{
    Lock lock;
    if (Voice *v = Find(s)) v->occlusion = occlusion;
}
void __stdcall AIL_set_3D_sample_effects_level(H3DSAMPLE, F32) {}
U32 __stdcall AIL_3D_sample_loop_count(H3DSAMPLE s) { return static_cast<U32>(NativeMiles::AIL_sample_loop_count(reinterpret_cast<HSAMPLE>(s))); }
void __stdcall AIL_set_3D_sample_loop_count(H3DSAMPLE s, U32 count) { NativeMiles::AIL_set_sample_loop_count(reinterpret_cast<HSAMPLE>(s), static_cast<S32>(count)); }
void __stdcall AIL_set_3D_sample_offset(H3DSAMPLE s, U32 offset)
{
    Lock lock;
    Voice *v = Find(s);
    if (v && v->pcm) v->position = static_cast<double>(offset) / (v->pcm->channels * 2);
}
U32 __stdcall AIL_3D_sample_length(H3DSAMPLE s)
{
    Lock lock;
    Voice *v = Find(s);
    return v && v->pcm ? static_cast<U32>(v->pcm->frames() * v->pcm->channels * 2) : 0;
}
U32 __stdcall AIL_3D_sample_offset(H3DSAMPLE s)
{
    Lock lock;
    Voice *v = Find(s);
    return v && v->pcm ? static_cast<U32>(v->position * v->pcm->channels * 2) : 0;
}
S32 __stdcall AIL_3D_sample_playback_rate(H3DSAMPLE s) { return NativeMiles::AIL_sample_playback_rate(reinterpret_cast<HSAMPLE>(s)); }
void __stdcall AIL_set_3D_sample_playback_rate(H3DSAMPLE s, S32 rate) { NativeMiles::AIL_set_sample_playback_rate(reinterpret_cast<HSAMPLE>(s), rate); }

// ---- WAV info / ADPCM ----
S32 __stdcall AIL_WAV_info(const void *data, AILSOUNDINFO *info)
{
    if (!info) return 0;
    std::memset(info, 0, sizeof(*info));
    const uint8_t *p = static_cast<const uint8_t *>(data);
    if (!p || std::memcmp(p, "RIFF", 4)) return 0;
    WavInfo w;
    if (!ParseWav(data, Le32(p + 4) + 8, w)) return 0;
    info->format = w.tag;
    info->data_ptr = w.data;
    info->data_len = w.dataLen;
    info->rate = w.rate;
    info->bits = w.bits;
    info->channels = w.channels;
    info->block_size = w.blockAlign;
    info->initial_ptr = p;
    info->samples = w.bits >= 8 && w.tag == 1 ? w.dataLen / (w.channels * (w.bits / 8)) : 0;
    if (w.tag == 0x11 && w.blockAlign >= 4u * w.channels) {
        const unsigned spb = (w.blockAlign - 4u * w.channels) * 8u / (4u * w.channels) + 1u;
        info->samples = (w.dataLen / w.blockAlign) * spb;
    }
    return 1;
}

S32 __stdcall AIL_decompress_ADPCM(const AILSOUNDINFO *info, void **outdata, U32 *outsize)
{
    if (!info || !outdata || !outsize || !info->initial_ptr) return 0;
    const uint8_t *p = static_cast<const uint8_t *>(info->initial_ptr);
    PcmData pcm;
    if (!DecodeWavToPcm(p, Le32(p + 4) + 8, pcm)) return 0;
    // 16 bit PCM WAV image: 44 byte header + data.
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.samples.size() * 2);
    uint8_t *image = new uint8_t[44 + dataBytes];
    auto w32 = [&](size_t at, uint32_t v) { std::memcpy(image + at, &v, 4); };
    auto w16 = [&](size_t at, uint16_t v) { std::memcpy(image + at, &v, 2); };
    std::memcpy(image, "RIFF", 4); w32(4, 36 + dataBytes);
    std::memcpy(image + 8, "WAVEfmt ", 8); w32(16, 16);
    w16(20, 1); w16(22, static_cast<uint16_t>(pcm.channels)); w32(24, pcm.rate);
    w32(28, pcm.rate * pcm.channels * 2); w16(32, static_cast<uint16_t>(pcm.channels * 2)); w16(34, 16);
    std::memcpy(image + 36, "data", 4); w32(40, dataBytes);
    std::memcpy(image + 44, pcm.samples.data(), dataBytes);
    *outdata = image;
    *outsize = 44 + dataBytes;
    return 1;
}

// ---- streams ----
HSTREAM __stdcall AIL_open_stream(HDIGDRIVER, const char *filename, S32)
{
    if (!filename || !*filename) return nullptr;
    std::vector<uint8_t> file;
    if (!ReadWholeFile(filename, file)) return nullptr;
    auto pcm = std::make_shared<PcmData>();
    std::shared_ptr<Mp3Source> mp3;
    if (file.size() >= 12 && !std::memcmp(file.data(), "RIFF", 4)) {
        if (!DecodeWavToPcm(file.data(), static_cast<uint32_t>(file.size()), *pcm)) return nullptr;
        pcm->totalMs = WavDurationMs(file.data(), static_cast<uint32_t>(file.size()));
    } else {
        mp3 = std::make_shared<Mp3Source>();
        if (!OpenMp3(file, *mp3)) return nullptr;
        pcm->channels = mp3->channels;
        pcm->rate = mp3->rate;
        pcm->totalMs = mp3->durationMs;
        pcm->finished = false;
        // Reserve for the whole decode so concurrent readers never see a reallocation.
        const double seconds = mp3->durationMs > 0.0 ? mp3->durationMs / 1000.0 + 2.0 : 600.0;
        pcm->samples.reserve(static_cast<size_t>(seconds * mp3->rate * mp3->channels) + 65536);
        ++g_stats.mp3Decoded;
    }
    EnsureLock();
    Voice *v = Allocate(Kind::Stream);
    {
        Lock lock;
        v->pcm = pcm;
        v->state = State::Stopped;
        v->loopCount = 1;
    }
    if (mp3) v->decoder = std::thread(DecodeMp3Thread, mp3, pcm, &v->cancel);
    ++g_stats.streamsOpened;
    return reinterpret_cast<HSTREAM>(v);
}
void __stdcall AIL_close_stream(HSTREAM s)
{
    EnsureLock();
    Voice *v = nullptr;
    {
        Lock lock;
        v = Find(s);
        if (v) v->state = State::Stopped;
    }
    ReleaseVoice(v);
}
void __stdcall AIL_start_stream(HSTREAM s)
{
    Lock lock;
    if (Voice *v = Find(s)) { v->state = State::Playing; v->position = 0.0; v->loopsDone = 0; }
}
void __stdcall AIL_pause_stream(HSTREAM s, S32 onoff)
{
    Lock lock;
    Voice *v = Find(s);
    if (!v) return;
    if (onoff && v->state == State::Playing) v->state = State::Paused;
    else if (!onoff && v->state == State::Paused) v->state = State::Playing;
}
AIL_stream_callback __stdcall AIL_register_stream_callback(HSTREAM s, AIL_stream_callback cb)
{
    Lock lock;
    Voice *v = Find(s);
    if (!v) return nullptr;
    auto previous = reinterpret_cast<AIL_stream_callback>(v->eos);
    v->eos = reinterpret_cast<void *>(cb);
    return previous;
}
void __stdcall AIL_set_stream_volume_pan(HSTREAM s, F32 volume, F32 pan) { NativeMiles::AIL_set_sample_volume_pan(reinterpret_cast<HSAMPLE>(s), volume, pan); }
void __stdcall AIL_stream_volume_pan(HSTREAM s, F32 *volume, F32 *pan) { NativeMiles::AIL_sample_volume_pan(reinterpret_cast<HSAMPLE>(s), volume, pan); }
void __stdcall AIL_set_stream_playback_rate(HSTREAM s, S32 rate) { NativeMiles::AIL_set_sample_playback_rate(reinterpret_cast<HSAMPLE>(s), rate); }
S32 __stdcall AIL_stream_playback_rate(HSTREAM s) { return NativeMiles::AIL_sample_playback_rate(reinterpret_cast<HSAMPLE>(s)); }
void __stdcall AIL_stream_ms_position(HSTREAM s, S32 *total, S32 *current)
{
    Lock lock;
    Voice *v = Find(s);
    if (total) *total = v && v->pcm ? static_cast<S32>(v->pcm->totalMs + 0.5) : 0;
    if (current) *current = v && v->pcm && v->pcm->rate ? static_cast<S32>(1000.0 * v->position / v->pcm->rate) : 0;
}
void __stdcall AIL_set_stream_ms_position(HSTREAM s, S32 pos) { NativeMiles::AIL_set_sample_ms_position(reinterpret_cast<HSAMPLE>(s), pos); }
S32 __stdcall AIL_stream_loop_count(HSTREAM s) { return NativeMiles::AIL_sample_loop_count(reinterpret_cast<HSAMPLE>(s)); }
void __stdcall AIL_set_stream_loop_count(HSTREAM s, S32 count) { NativeMiles::AIL_set_sample_loop_count(reinterpret_cast<HSAMPLE>(s), count); }
void __stdcall AIL_set_stream_loop_block(HSTREAM, S32, S32) {}

// ---- quick API (mission briefing audio) ----
HAUDIO __stdcall AIL_quick_load_and_play(const char *filename, U32 loopCount, S32)
{
    std::vector<uint8_t> file;
    if (!filename || !ReadWholeFile(filename, file)) return nullptr;
    auto pcm = std::make_shared<PcmData>();
    if (!DecodeWavToPcm(file.data(), static_cast<uint32_t>(file.size()), *pcm)) return nullptr;
    EnsureLock();
    Voice *v = Allocate(Kind::Quick);
    Lock lock;
    v->pcm = pcm;
    v->loopCount = static_cast<int>(loopCount);
    v->state = State::Playing;
    return reinterpret_cast<HAUDIO>(v);
}
void __stdcall AIL_quick_set_volume(HAUDIO audio, F32 volume, F32 extravol)
{
    Lock lock;
    if (Voice *v = Find(audio)) { v->volume = Clamp(volume, 0.0f, 4.0f); v->pan = Clamp(extravol, 0.0f, 1.0f); }
}
void __stdcall AIL_quick_unload(HAUDIO audio) { ReleaseVoice(Find(audio)); }

} // namespace NativeMiles
#endif
