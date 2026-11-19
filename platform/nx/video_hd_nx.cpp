// Cenas em vídeo remasterizadas no Switch: a mesma ideia do video_hd.cpp do PC (index.tsv em
// <dados>/videos/<jogo>/, instante tirado do contador de quadros do Bink do jogo), mas sem ffmpeg.exe:
//
//  - o arquivo (HEVC/H.264 em .mp4) é lido pelo FFmpeg do devkitPro (switch-ffmpeg) e decodificado no NVDEC do
//    Tegra (hwaccel nvtegra), numa thread "video hd" de prioridade comum (abaixo de main/Render/gravação vk/som);
//  - o quadro sai do NVDEC em NV12 (ou P010) por uma cópia do VIC para memória do processo (256 alinhado, sem
//    passar pela CPU) e vai para gxm_vk.cpp, que converte para RGBA num compute shader e põe no lugar do alvo.
//
// O jogo continua tocando o Bink dele (som, fim do vídeo e o pular com X ficam iguais); só a imagem é trocada.
// Sem a pasta videos, sem o NVDEC ou com um arquivo que não abre, fica tudo como antes (imagem do Bink).
#include "vitart.h"
#include "video_hd.h"

#include <switch.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
}

#ifndef GOW_GAME
#define GOW_GAME "gow1"
#endif

namespace vt {
namespace {

struct Entry {
    std::string file;
    double offset = 0; // segundos somados ao instante do jogo
};
std::map<std::string, Entry> g_index; // nome do .bk2 em minúsculas, sem extensão
bool g_index_loaded = false;

std::string lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

void load_dir(const std::filesystem::path &dir) {
    std::error_code ec;
    std::ifstream f(dir / "index.tsv");
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t a = line.find('\t');
        if (a == std::string::npos) continue;
        const size_t b = line.find('\t', a + 1);
        Entry e;
        e.file = (dir / line.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1)).string();
        if (b != std::string::npos) e.offset = std::atof(line.c_str() + b + 1);
        if (std::filesystem::exists(e.file, ec)) g_index.emplace(lower(line.substr(0, a)), e); // o primeiro vence
    }
}

// Pasta do jogo atual primeiro (videos/gow1 ou videos/gow2); as outras só completam nomes que faltarem.
void load_index() {
    if (g_index_loaded) return;
    g_index_loaded = true;
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::path(data_path()) / "videos";
    if (!std::filesystem::is_directory(root, ec)) return;
    load_dir(root / GOW_GAME);
    for (auto &dir : std::filesystem::directory_iterator(root, ec))
        if (dir.is_directory(ec) && dir.path().filename() != GOW_GAME) load_dir(dir.path());
    if (!g_index.empty()) logf("vitart: cenas remasterizadas: %zu vídeos em %s", g_index.size(), root.string().c_str());
}

void av_log_to_vitart(void *, int level, const char *fmt, va_list ap) {
    if (level > AV_LOG_ERROR) return;
    static int n = 0;
    if (n >= 40) return;
    n++;
    char msg[512];
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    size_t l = std::strlen(msg);
    while (l && (msg[l - 1] == '\n' || msg[l - 1] == '\r')) msg[--l] = 0;
    logf("vitart: ffmpeg: %s", msg);
}

// ------------------------------------------------------------------ quadros
// Memória dos quadros copiados do NVDEC: o VIC só copia para endereço e pitch alinhados a 256 (senão o FFmpeg cai
// numa cópia pela CPU); páginas inteiras para o nvmap mapear.
AVBufferRef *pool_alloc(size_t size) {
    void *p = aligned_alloc(0x1000, size);
    if (!p) return nullptr;
    AVBufferRef *b = av_buffer_create((uint8_t *)p, size, [](void *, uint8_t *d) { free(d); }, nullptr, 0);
    if (!b) free(p);
    return b;
}

struct Frame {
    int64_t index = 0; // quadro do arquivo (60 por segundo no remaster)
    AVFrame *f = nullptr;
    uint32_t w = 0, h = 0, pitch = 0, uv = 0, flags = 0;
    ~Frame() { av_frame_free(&f); }
};

// ------------------------------------------------------------------ decodificador (só a thread "video hd" usa)
AVBufferRef *g_hwdev = nullptr;
std::atomic<bool> g_hw_failed{false};

enum AVPixelFormat pick_format(AVCodecContext *, const enum AVPixelFormat *fmts) {
    for (const enum AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; p++)
        if (*p == AV_PIX_FMT_NVTEGRA) return *p;
    logf("vitart: cena remasterizada: o NVDEC não aceita este vídeo");
    return AV_PIX_FMT_NONE; // decodificar pela CPU não aguenta 1080p60: melhor ficar com a imagem do jogo
}

struct Decoder {
    std::string file;
    AVFormatContext *fmt = nullptr;
    AVCodecContext *cc = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *fr = nullptr;
    int si = -1;
    double tb = 0, fps = 60;
    int64_t t0 = 0;
    bool eof_sent = false;
    AVBufferPool *pool = nullptr;
    size_t pool_size = 0;

    void close() {
        avcodec_free_context(&cc);
        avformat_close_input(&fmt);
        av_packet_free(&pkt);
        av_frame_free(&fr);
        if (pool) av_buffer_pool_uninit(&pool); // os quadros ainda na fila soltam a memória quando saírem
        pool_size = 0;
        file.clear();
        si = -1;
    }
    ~Decoder() { close(); }

    bool open(const std::string &path) {
        close();
        if (!g_hwdev && !g_hw_failed) {
            const int r = av_hwdevice_ctx_create(&g_hwdev, AV_HWDEVICE_TYPE_NVTEGRA, nullptr, nullptr, 0);
            if (r < 0) {
                g_hw_failed = true;
                logf("vitart: cenas remasterizadas desligadas: NVDEC não abriu (%d)", r);
            } else
                logf("vitart: cenas remasterizadas: NVDEC pronto");
        }
        if (!g_hwdev) return false;
        // "sdmc:/..." sozinho o FFmpeg lê como protocolo "sdmc" (igual a "http:") e não abre: vai como file:
        const std::string url = path.compare(0, 5, "file:") == 0 ? path : "file:" + path;
        int r = avformat_open_input(&fmt, url.c_str(), nullptr, nullptr);
        if (r >= 0) r = avformat_find_stream_info(fmt, nullptr);
        if (r < 0) {
            char err[128];
            av_strerror(r, err, sizeof err);
            logf("vitart: cena remasterizada: não abriu %s (%s)", path.c_str(), err);
            close();
            return false;
        }
        const AVCodec *codec = nullptr;
        si = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (si < 0 || !codec) {
            logf("vitart: cena remasterizada: sem vídeo em %s", path.c_str());
            close();
            return false;
        }
        bool hw = false;
        for (int i = 0;; i++) {
            const AVCodecHWConfig *c = avcodec_get_hw_config(codec, i);
            if (!c) break;
            if (c->device_type == AV_HWDEVICE_TYPE_NVTEGRA && (c->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) hw = true;
        }
        if (!hw) {
            logf("vitart: cena remasterizada: %s (%s) sem decodificação no NVDEC", path.c_str(), codec->name);
            close();
            return false;
        }
        AVStream *st = fmt->streams[si];
        cc = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(cc, st->codecpar);
        cc->hw_device_ctx = av_buffer_ref(g_hwdev);
        cc->get_format = pick_format;
        cc->thread_count = 1;
        if (avcodec_open2(cc, codec, nullptr) < 0) {
            logf("vitart: cena remasterizada: %s não abriu no decodificador", path.c_str());
            close();
            return false;
        }
        tb = av_q2d(st->time_base);
        fps = st->avg_frame_rate.num && st->avg_frame_rate.den ? av_q2d(st->avg_frame_rate) : 60.0;
        if (fps < 1 || fps > 240) fps = 60.0;
        t0 = st->start_time != AV_NOPTS_VALUE ? st->start_time : 0;
        pkt = av_packet_alloc();
        fr = av_frame_alloc();
        eof_sent = false;
        file = path;
        logf("vitart: cena remasterizada: %s (%s %dx%d, %.2f quadros/s)", path.c_str(), codec->name, cc->width, cc->height, fps);
        return true;
    }

    void seek(int64_t index) {
        const int64_t ts = t0 + (int64_t)((index / fps) / tb);
        av_seek_frame(fmt, si, ts < t0 ? t0 : ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(cc);
        eof_sent = false;
    }

    // 1 = quadro em fr, 0 = fim do arquivo, -1 = erro
    int next() {
        for (;;) {
            int r = avcodec_receive_frame(cc, fr);
            if (r == 0) return 1;
            if (r == AVERROR_EOF) return 0;
            if (r != AVERROR(EAGAIN)) return -1;
            if (eof_sent) return 0;
            r = av_read_frame(fmt, pkt);
            if (r < 0) {
                avcodec_send_packet(cc, nullptr);
                eof_sent = true;
                continue;
            }
            if (pkt->stream_index == si) {
                r = avcodec_send_packet(cc, pkt);
                if (r < 0 && r != AVERROR(EAGAIN)) {
                    av_packet_unref(pkt);
                    return -1;
                }
            }
            av_packet_unref(pkt);
        }
    }

    int64_t index_of(const AVFrame *f) const {
        const int64_t pts = f->best_effort_timestamp != AV_NOPTS_VALUE ? f->best_effort_timestamp : f->pts;
        if (pts == AV_NOPTS_VALUE) return -1;
        return (int64_t)std::llround((pts - t0) * tb * fps);
    }

    // fr (no NVDEC) -> quadro em memória comum, pela cópia do VIC
    std::shared_ptr<Frame> fetch(int64_t index) {
        if (!fr->hw_frames_ctx) return nullptr;
        const auto *hfc = (const AVHWFramesContext *)fr->hw_frames_ctx->data;
        const AVPixelFormat sw = hfc->sw_format;
        if (sw != AV_PIX_FMT_NV12 && sw != AV_PIX_FMT_P010) {
            static bool told = false;
            if (!told) told = true, logf("vitart: cena remasterizada: formato %d do NVDEC não suportado", (int)sw);
            return nullptr;
        }
        const uint32_t w = (uint32_t)fr->width, h = (uint32_t)fr->height, bps = sw == AV_PIX_FMT_P010 ? 2 : 1;
        const uint32_t pitch = (w * bps + 255) & ~255u;
        const uint32_t uv = (pitch * h + 0xfff) & ~0xfffu;
        const size_t size = ((size_t)uv + (size_t)pitch * ((h + 1) / 2) + 0xfff) & ~(size_t)0xfff;
        if (!pool || pool_size != size) {
            if (pool) av_buffer_pool_uninit(&pool);
            pool = av_buffer_pool_init(size, pool_alloc);
            pool_size = size;
        }
        auto out = std::make_shared<Frame>();
        out->f = av_frame_alloc();
        out->f->buf[0] = av_buffer_pool_get(pool);
        if (!out->f->buf[0]) return nullptr;
        out->f->format = sw;
        out->f->width = (int)w, out->f->height = (int)h;
        out->f->data[0] = out->f->buf[0]->data;
        out->f->data[1] = out->f->buf[0]->data + uv;
        out->f->linesize[0] = out->f->linesize[1] = (int)pitch;
        if (av_hwframe_transfer_data(out->f, fr, 0) < 0) {
            static int told = 0;
            if (told++ < 3) logf("vitart: cena remasterizada: cópia do quadro do NVDEC falhou");
            return nullptr;
        }
        out->index = index;
        out->w = w, out->h = h, out->pitch = pitch, out->uv = uv;
        out->flags = (bps == 2 ? 1u : 0u) | (fr->color_range == AVCOL_RANGE_JPEG ? 2u : 0u) |
                     (fr->colorspace == AVCOL_SPC_BT470BG || fr->colorspace == AVCOL_SPC_SMPTE170M ? 4u : 0u);
        return out;
    }
};

// ------------------------------------------------------------------ estado compartilhado (g_mutex)
std::mutex g_mutex;
std::condition_variable g_cv;
std::atomic<uint64_t> g_gen{0}; // muda a cada abrir/fechar/salto: a thread larga o que estava fazendo
bool g_on = false;
std::string g_file;
int64_t g_start = 0;  // primeiro quadro pedido (abertura ou salto)
int64_t g_newest = -1; // último quadro posto na fila nesta geração
bool g_eof = false, g_failed = false;
std::deque<std::shared_ptr<Frame>> g_queue;
std::shared_ptr<Frame> g_hold;
Entry g_entry;
double g_file_fps = 60.0;
double g_game_fps = 29.97;
int64_t g_last_num = -1;
std::chrono::steady_clock::time_point g_num_at, g_open_at;
bool g_thread_started = false;
constexpr size_t kQueue = 4;

void worker() {
    // abaixo das três threads do quadro (0x2A/0x2B) e do som (0x28), igual às demais; o NVDEC faz o trabalho
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2C);
    av_log_set_callback(av_log_to_vitart);
    Decoder d;
    uint64_t served = ~0ull;
    for (;;) {
        std::string file;
        int64_t start;
        uint64_t gen;
        {
            std::unique_lock<std::mutex> l(g_mutex);
            if (!g_on && !d.file.empty()) {
                l.unlock();
                d.close(); // vídeo fechado: solta o NVDEC e os quadros
                l.lock();
            }
            g_cv.wait(l, [&] { return g_on && g_gen.load() != served; });
            gen = served = g_gen.load();
            file = g_file, start = g_start;
        }
        bool ok = d.file == file || d.open(file);
        if (ok) d.seek(start);
        {
            const std::lock_guard<std::mutex> l(g_mutex);
            if (g_gen.load() != gen) continue;
            if (ok) g_file_fps = d.fps;
            else g_failed = true;
        }
        if (!ok) continue;
        while (g_gen.load() == gen) {
            const int r = d.next();
            if (r <= 0) {
                if (r < 0) logf("vitart: cena remasterizada: erro ao decodificar %s", file.c_str());
                const std::lock_guard<std::mutex> l(g_mutex);
                if (g_gen.load() == gen) g_eof = true;
                break;
            }
            const int64_t idx = d.index_of(d.fr);
            if (idx < start - 1) { // depois de um salto o NVDEC recomeça do quadro-chave anterior
                av_frame_unref(d.fr);
                continue;
            }
            auto f = d.fetch(idx);
            av_frame_unref(d.fr);
            if (!f) continue;
            std::unique_lock<std::mutex> l(g_mutex);
            g_cv.wait(l, [&] { return g_gen.load() != gen || g_queue.size() < kQueue; });
            if (g_gen.load() != gen) break;
            g_queue.push_back(std::move(f));
            g_newest = idx;
        }
    }
}

// pede à thread que comece em index (g_mutex preso)
void restart_locked(int64_t index) {
    g_start = index < 0 ? 0 : index;
    g_queue.clear();
    g_newest = -1;
    g_eof = false;
    g_gen++;
    g_cv.notify_all();
}

} // namespace

void video_hd_open(const std::string &path, uint32_t frames, uint32_t fps_num, uint32_t fps_den, uint32_t, uint32_t) {
    load_index();
    std::string name = std::filesystem::path(path).stem().string();
    auto it = g_index.find(lower(name));
    const std::lock_guard<std::mutex> lock(g_mutex);
    g_on = false;
    g_hold.reset();
    restart_locked(0);
    if (it == g_index.end() || g_hw_failed) return;
    (void)frames;
    g_entry = it->second;
    g_file = g_entry.file;
    g_failed = false;
    g_game_fps = fps_den ? (double)fps_num / fps_den : 29.97;
    g_last_num = -1;
    g_open_at = std::chrono::steady_clock::now();
    g_on = true;
    restart_locked((int64_t)(g_entry.offset * g_file_fps));
    if (!g_thread_started) {
        g_thread_started = true;
        spawn_detached(worker, "video hd");
    }
    logf("vitart: cena remasterizada: %s -> %s", name.c_str(), g_entry.file.c_str());
}

void video_hd_close() {
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_on) return;
    g_on = false;
    g_hold.reset();
    restart_locked(0);
}

// a interface RGBA do PC não é usada no Switch (video_hd_frame_nx abaixo)
bool video_hd_frame(int64_t, const uint8_t *&, uint32_t &, uint32_t &) { return false; }

bool video_hd_active() {
    const std::lock_guard<std::mutex> lock(g_mutex);
    return g_on && !g_failed;
}

bool video_hd_frame_nx(int64_t bink_frame, VideoFrameNX &out) {
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_on || g_failed) return false;
    // o Bink ainda não começou: o jogo abre o vídeo e carrega a fase antes de tocá-lo; fica a imagem do jogo
    if (bink_frame < 0) return false;
    const auto now = std::chrono::steady_clock::now();
    if (bink_frame != g_last_num) g_last_num = bink_frame, g_num_at = now;
    const double since = std::chrono::duration<double>(now - g_num_at).count();
    const double t = (bink_frame + std::min(since * g_game_fps, 1.0)) / g_game_fps;
    static int64_t logged_at = -1;
    if (logged_at < 0 || bink_frame - logged_at >= 300 || bink_frame < logged_at) {
        logged_at = bink_frame;
        logf("vitart: cena remasterizada: quadro do Bink %lld -> %.2f s (%.1f s desde a abertura)", (long long)bink_frame, t,
             std::chrono::duration<double>(now - g_open_at).count());
    }
    const int64_t want = (int64_t)((t + g_entry.offset) * g_file_fps);
    // salto grande (para frente ou para trás): recomeça a decodificação no ponto pedido
    // (logo depois de um recomeço a fila está vazia e g_start = want: não dispara de novo)
    const bool behind = !g_queue.empty() && want < g_queue.front()->index - 2;
    const bool ahead = !g_eof && want > std::max(g_newest, g_start) + 90;
    if (behind || ahead) restart_locked(want);
    while (g_queue.size() > 1 && g_queue[1]->index <= want) g_queue.pop_front();
    if (!g_queue.empty() && g_queue.front()->index <= want + 1) {
        g_hold = std::move(g_queue.front());
        g_queue.pop_front();
        g_cv.notify_all();
    }
    if (!g_hold) return false;
    // remaster mais curto que o vídeo do jogo: no fim do arquivo volta para a imagem do jogo
    if (g_eof && g_queue.empty() && want > g_hold->index + 6) return false;
    const Frame &f = *g_hold;
    out.data = f.f->data[0];
    out.w = f.w, out.h = f.h, out.pitch = f.pitch, out.uv_offset = f.uv, out.flags = f.flags;
    out.keep = g_hold;
    return true;
}

} // namespace vt
