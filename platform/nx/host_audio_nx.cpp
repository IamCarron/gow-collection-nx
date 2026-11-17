// Saída de som no Switch por audout (cópia do host_audio.cpp do PC com o misturador trocado).
#include "host_audio.h"
#include "vitart.h"

#include <switch.h>
#include <cstdlib>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
#include <vector>



namespace hostaudio {
namespace {

struct Port {
    uint32_t freq = kRate;
    std::vector<float> ring; // estéreo intercalado, capacidade fixa
    size_t head = 0, size = 0; // em quadros
    bool playing = false;      // tocando: a placa consome; parada, espera encher kPrebuf antes de voltar
    double frac = 0.0;         // reamostragem
    float last[2] = {};
};

std::mutex g_mutex;
std::map<int, Port> g_ports;
int g_next = 1;
std::atomic<bool> g_started{false}, g_ok{false}, g_silenced{false};
Stats g_stats;
constexpr size_t kRingFrames = kRate / 2; // 500 ms de capacidade por porta
constexpr size_t kPrebuf = 1536;          // 32 ms antes de uma porta parada voltar a tocar

void push(Port &p, float l, float r) {
    const size_t cap = p.ring.size() / 2;
    if (p.size == cap) { // cheia: descarta o mais antigo (não deve acontecer com a cadência pela fila)
        p.head = (p.head + 1) % cap;
        p.size--;
    }
    const size_t at = (p.head + p.size) % cap;
    p.ring[2 * at] = l, p.ring[2 * at + 1] = r;
    p.size++;
}

void mix_into(int16_t *out, uint32_t n, std::vector<float> &mix) {
    mix.assign((size_t)n * 2, 0.f);
    {
        std::lock_guard<std::mutex> g(g_mutex);
        if (!g_silenced)
            for (auto &[id, p] : g_ports) {
                if (!p.playing) {
                    if (p.size < kPrebuf) continue; // pré-enchimento: evita micro-cortes no começo de cada som
                    p.playing = true;
                }
                const size_t cap = p.ring.size() / 2;
                const size_t take = std::min<size_t>(n, p.size);
                for (size_t i = 0; i < take; i++) {
                    const size_t at = (p.head + i) % cap;
                    mix[2 * i] += p.ring[2 * at], mix[2 * i + 1] += p.ring[2 * at + 1];
                }
                p.head = (p.head + take) % cap;
                p.size -= take;
                if (take < n) g_stats.underruns++, p.playing = false;
            }
        g_stats.mixed += n;
    }
    for (size_t i = 0; i < (size_t)n * 2; i++) {
        float x = mix[i];
        const float ax = std::fabs(x);
        if (ax > 0.9f) x = std::copysign(0.9f + 0.1f * std::tanh((ax - 0.9f) / 0.1f), x);
        out[i] = (int16_t)std::clamp(x * 32767.f, -32768.f, 32767.f);
    }
}

// Switch: audout a 48 kHz estéreo, 4 buffers de 10 ms em rodízio.
void mixer() {
    constexpr uint32_t kFrames = 480, kBufs = 4;
    constexpr size_t kBytes = (kFrames * 4 + 0xFFF) & ~(size_t)0xFFF;
    Result rc = audoutInitialize();
    if (R_SUCCEEDED(rc)) rc = audoutStartAudioOut();
    if (R_FAILED(rc)) {
        vt::logf("vitart: audout falhou (0x%x); sem saída de som", rc);
        g_ok = false;
        g_started = true;
        return;
    }
    vt::logf("vitart: som por audout: %u Hz, %u canais, %u buffers de %u quadros", audoutGetSampleRate(),
             audoutGetChannelCount(), kBufs, kFrames);
    g_stats.latency_ms = kBufs * kFrames * 1000 / kRate;
    static AudioOutBuffer bufs[kBufs];
    std::vector<float> mix;
    for (uint32_t i = 0; i < kBufs; i++) {
        void *mem = aligned_alloc(0x1000, kBytes);
        std::memset(mem, 0, kBytes);
        bufs[i].next = nullptr;
        bufs[i].buffer = mem;
        bufs[i].buffer_size = kBytes;
        bufs[i].data_size = kFrames * 4;
        bufs[i].data_offset = 0;
        audoutAppendAudioOutBuffer(&bufs[i]);
    }
    g_ok = true;
    g_started = true;
    for (;;) {
        AudioOutBuffer *done = nullptr;
        u32 count = 0;
        if (R_FAILED(audoutWaitPlayFinish(&done, &count, 100000000ull)) || !done) continue;
        mix_into(static_cast<int16_t *>(done->buffer), kFrames, mix);
        audoutAppendAudioOutBuffer(done);
    }
}
} // namespace

bool start() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (getenv("VITART_MUTE")) {
            g_started = true;
            return;
        }
        vt::spawn_detached(mixer, "audio");
    });
    while (!g_started) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return g_ok;
}

int open_port(uint32_t freq) {
    std::lock_guard<std::mutex> g(g_mutex);
    const int id = g_next++;
    Port &p = g_ports[id];
    p.freq = freq ? freq : kRate;
    p.ring.assign(kRingFrames * 2, 0.f);
    return id;
}

void close_port(int port) {
    std::lock_guard<std::mutex> g(g_mutex);
    g_ports.erase(port);
}

void write(int port, const int16_t *src, uint32_t frames, bool stereo, int vol_l, int vol_r) {
    std::lock_guard<std::mutex> g(g_mutex);
    auto it = g_ports.find(port);
    if (it == g_ports.end()) return;
    Port &p = it->second;
    const float gl = vol_l / (32768.f * 32768.f), gr = vol_r / (32768.f * 32768.f);
    auto at = [&](uint32_t i, int c) { return (float)(stereo ? src[2 * i + c] : src[i]); };
    if (p.freq == kRate) {
        for (uint32_t i = 0; i < frames; i++) push(p, at(i, 0) * gl, at(i, 1) * gr);
    } else { // linear: as portas do jogo usam 48 kHz; isto só cobre o caso raro
        const double step = (double)p.freq / kRate;
        while (p.frac < frames) {
            const uint32_t i = (uint32_t)p.frac;
            const float t = (float)(p.frac - i);
            const float l0 = i ? at(i - 1, 0) : p.last[0], r0 = i ? at(i - 1, 1) : p.last[1];
            push(p, (l0 + (at(i, 0) - l0) * t) * gl, (r0 + (at(i, 1) - r0) * t) * gr);
            p.frac += step;
        }
        p.frac -= frames;
        p.last[0] = at(frames - 1, 0), p.last[1] = at(frames - 1, 1);
    }
}

uint32_t queued(int port) {
    std::lock_guard<std::mutex> g(g_mutex);
    auto it = g_ports.find(port);
    return it == g_ports.end() ? 0 : (uint32_t)it->second.size;
}

void silence() {
    std::lock_guard<std::mutex> g(g_mutex);
    g_silenced = true;
}

Stats stats() {
    std::lock_guard<std::mutex> g(g_mutex);
    return g_stats;
}

} // namespace hostaudio
