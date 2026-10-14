// Offline coverage for the native AMD64 Miles (AIL) implementation: decoding, mixing, callbacks, 3D gain.
#define NOMINMAX
#include <windows.h>
#include <gtest/gtest.h>
#include "NativeMiles.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
std::vector<uint8_t> MakeWav(const std::vector<int16_t> &pcm, unsigned rate, unsigned channels = 1)
{
    std::vector<uint8_t> w(44 + pcm.size() * 2);
    auto p32 = [&](size_t at, uint32_t v) { std::memcpy(&w[at], &v, 4); };
    auto p16 = [&](size_t at, uint16_t v) { std::memcpy(&w[at], &v, 2); };
    std::memcpy(&w[0], "RIFF", 4); p32(4, static_cast<uint32_t>(36 + pcm.size() * 2));
    std::memcpy(&w[8], "WAVEfmt ", 8); p32(16, 16); p16(20, 1); p16(22, static_cast<uint16_t>(channels));
    p32(24, rate); p32(28, rate * channels * 2); p16(32, static_cast<uint16_t>(channels * 2)); p16(34, 16);
    std::memcpy(&w[36], "data", 4); p32(40, static_cast<uint32_t>(pcm.size() * 2));
    std::memcpy(&w[44], pcm.data(), pcm.size() * 2);
    return w;
}

std::vector<int16_t> Square(unsigned frames, int16_t amp)
{
    return std::vector<int16_t>(frames, amp);
}

struct Fixture : ::testing::Test
{
    void SetUp() override
    {
        NativeMiles::SetDevicelessForTests(true);
        NativeMiles::AIL_startup();
        NativeMiles::resetStats();
    }
    void TearDown() override { NativeMiles::AIL_shutdown(); }
};

int g_eos = 0;
void __stdcall OnEos(HSAMPLE) { ++g_eos; }
void __stdcall OnEos3D(H3DPOBJECT) { ++g_eos; }
}

TEST_F(Fixture, WavInfoAndPcmDecode)
{
    auto wav = MakeWav(Square(100, 1000), 22050);
    AILSOUNDINFO info;
    ASSERT_EQ(NativeMiles::AIL_WAV_info(wav.data(), &info), 1);
    EXPECT_EQ(info.rate, 22050u);
    EXPECT_EQ(info.channels, 1u);
    EXPECT_EQ(info.bits, 16u);
    int16_t *pcm = nullptr;
    uint32_t frames = 0, rate = 0, ch = 0;
    ASSERT_TRUE(NativeMiles::DecodeWav(wav.data(), static_cast<uint32_t>(wav.size()), &pcm, &frames, &rate, &ch));
    EXPECT_EQ(frames, 100u);
    EXPECT_EQ(pcm[50], 1000);
    NativeMiles::FreePcm(pcm);
}

TEST_F(Fixture, ImaAdpcmSilenceAndHeader)
{
    // One mono block: predictor 1000, index 0, then 4 bytes of zero nibbles -> predictor follows tiny steps.
    std::vector<uint8_t> w(44 + 8, 0);
    auto p32 = [&](size_t at, uint32_t v) { std::memcpy(&w[at], &v, 4); };
    auto p16 = [&](size_t at, uint16_t v) { std::memcpy(&w[at], &v, 2); };
    std::memcpy(&w[0], "RIFF", 4); p32(4, 36 + 8);
    std::memcpy(&w[8], "WAVEfmt ", 8); p32(16, 16); p16(20, 0x11); p16(22, 1); p32(24, 22050); p32(28, 11000);
    p16(32, 8); p16(34, 4);
    std::memcpy(&w[36], "data", 4); p32(40, 8);
    int16_t pred = 1000;
    std::memcpy(&w[44], &pred, 2);
    int16_t *pcm = nullptr;
    uint32_t frames = 0, rate = 0, ch = 0;
    ASSERT_TRUE(NativeMiles::DecodeWav(w.data(), static_cast<uint32_t>(w.size()), &pcm, &frames, &rate, &ch));
    EXPECT_EQ(frames, 9u);          // header sample + 8 nibbles
    EXPECT_EQ(pcm[0], 1000);
    EXPECT_NEAR(pcm[8], 1000, 40);
    NativeMiles::FreePcm(pcm);
}

TEST_F(Fixture, SampleMixesAndCompletes)
{
    HSAMPLE s = NativeMiles::AIL_allocate_sample_handle(nullptr);
    ASSERT_NE(s, nullptr);
    NativeMiles::AIL_init_sample(s);
    auto wav = MakeWav(Square(4410, 16384), 44100);   // 100 ms
    ASSERT_EQ(NativeMiles::AIL_set_sample_file(s, wav.data(), 0), 1);
    NativeMiles::AIL_set_sample_volume_pan(s, 1.0f, 0.5f);
    g_eos = 0;
    NativeMiles::AIL_register_EOS_callback(s, OnEos);
    NativeMiles::AIL_start_sample(s);
    EXPECT_EQ(NativeMiles::ActiveVoiceCount(), 1u);
    std::vector<int16_t> out(1024 * 2);
    NativeMiles::Render(out.data(), 1024);
    EXPECT_NEAR(out[10], 16384, 200);
    EXPECT_NEAR(out[11], 16384, 200);
    for (int i = 0; i < 6; ++i) NativeMiles::Render(out.data(), 1024);
    EXPECT_EQ(g_eos, 1);
    EXPECT_EQ(NativeMiles::ActiveVoiceCount(), 0u);
    NativeMiles::AIL_release_sample_handle(s);
}

TEST_F(Fixture, LoopCountAndPan)
{
    HSAMPLE s = NativeMiles::AIL_allocate_sample_handle(nullptr);
    auto wav = MakeWav(Square(441, 10000), 44100);   // 10 ms
    ASSERT_EQ(NativeMiles::AIL_set_sample_file(s, wav.data(), 0), 1);
    NativeMiles::AIL_set_sample_loop_count(s, 0);   // infinite
    NativeMiles::AIL_set_sample_volume_pan(s, 1.0f, 0.0f);   // hard left
    NativeMiles::AIL_start_sample(s);
    std::vector<int16_t> out(4096 * 2);
    NativeMiles::Render(out.data(), 4096);
    EXPECT_GT(out[2000], 9000);
    EXPECT_EQ(out[2001], 0);
    EXPECT_EQ(NativeMiles::ActiveVoiceCount(), 1u);
    NativeMiles::AIL_release_sample_handle(s);
}

TEST_F(Fixture, StopResumeHoldsPosition)
{
    HSAMPLE s = NativeMiles::AIL_allocate_sample_handle(nullptr);
    auto wav = MakeWav(Square(44100, 8000), 44100);
    ASSERT_EQ(NativeMiles::AIL_set_sample_file(s, wav.data(), 0), 1);
    NativeMiles::AIL_start_sample(s);
    std::vector<int16_t> out(1000 * 2);
    NativeMiles::Render(out.data(), 1000);
    NativeMiles::AIL_stop_sample(s);
    NativeMiles::Render(out.data(), 1000);
    EXPECT_EQ(out[0], 0);
    S32 total = 0, cur = 0;
    NativeMiles::AIL_sample_ms_position(s, &total, &cur);
    EXPECT_EQ(total, 1000);
    EXPECT_NEAR(cur, 22, 2);
    NativeMiles::AIL_resume_sample(s);
    NativeMiles::Render(out.data(), 1000);
    EXPECT_GT(out[0], 7000);
    NativeMiles::AIL_release_sample_handle(s);
}

TEST_F(Fixture, PositionalAttenuationAndPan)
{
    H3DPOBJECT listener = NativeMiles::AIL_open_3D_listener(nullptr);
    NativeMiles::AIL_set_3D_position(listener, 0, 0, 0);
    NativeMiles::AIL_set_3D_orientation(listener, 0, 1, 0, 0, 0, 1);
    H3DSAMPLE s = NativeMiles::AIL_allocate_3D_sample_handle(nullptr);
    auto wav = MakeWav(Square(8820, 20000), 44100);
    ASSERT_EQ(NativeMiles::AIL_set_3D_sample_file(s, wav.data()), 1);
    NativeMiles::AIL_set_3D_sample_distances(s, 1000.0f, 10.0f);
    NativeMiles::AIL_set_3D_sample_volume(s, 1.0f);
    g_eos = 0;
    NativeMiles::AIL_register_3D_EOS_callback(s, OnEos3D);
    NativeMiles::AIL_set_3D_position(reinterpret_cast<H3DPOBJECT>(s), 40.0f, 0, 0);   // to the right, 4x min distance
    NativeMiles::AIL_start_3D_sample(s);
    std::vector<int16_t> out(256 * 2);
    NativeMiles::Render(out.data(), 256);
    EXPECT_GT(out[101], out[100]);     // source to the listener's right: right channel louder
    const int l = out[100], r = out[101];
    EXPECT_NE(l, r);
    EXPECT_LT(((l > r) ? l : r), 20000 / 3);   // 10/40 distance roll-off
    NativeMiles::AIL_end_3D_sample(s);
    NativeMiles::AIL_set_3D_position(reinterpret_cast<H3DPOBJECT>(s), 5000.0f, 0, 0);   // beyond max distance
    NativeMiles::AIL_start_3D_sample(s);
    NativeMiles::Render(out.data(), 256);
    EXPECT_EQ(out[100], 0);
    NativeMiles::AIL_release_3D_sample_handle(s);
}

TEST_F(Fixture, StreamLengthWithoutPlaying)
{
    // AIL_open_stream goes through the file callbacks / filesystem; use a temp file.
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    std::strcat(path, "nativemiles_test.wav");
    auto wav = MakeWav(Square(22050, 100), 22050);   // exactly 1000 ms
    FILE *f = nullptr;
    fopen_s(&f, path, "wb");
    ASSERT_NE(f, nullptr);
    fwrite(wav.data(), 1, wav.size(), f);
    fclose(f);
    HSTREAM st = NativeMiles::AIL_open_stream(nullptr, path, 0);
    ASSERT_NE(st, nullptr);
    S32 total = 0, cur = 0;
    NativeMiles::AIL_stream_ms_position(st, &total, &cur);
    EXPECT_EQ(total, 1000);
    EXPECT_EQ(cur, 0);
    NativeMiles::AIL_start_stream(st);
    std::vector<int16_t> out(2048 * 2);
    NativeMiles::Render(out.data(), 2048);
    EXPECT_GT(out[10], 0);
    NativeMiles::AIL_close_stream(st);
    DeleteFileA(path);
}

TEST_F(Fixture, DecompressAdpcmProducesPcmWav)
{
    auto wav = MakeWav(Square(32, 5), 22050);
    AILSOUNDINFO info;
    ASSERT_EQ(NativeMiles::AIL_WAV_info(wav.data(), &info), 1);
    void *out = nullptr;
    U32 size = 0;
    ASSERT_EQ(NativeMiles::AIL_decompress_ADPCM(&info, &out, &size), 1);
    EXPECT_EQ(size, 44u + 64u);
    NativeMiles::AIL_mem_free_lock(out);
}
