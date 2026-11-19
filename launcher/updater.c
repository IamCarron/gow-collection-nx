// Auto-atualizador do launcher (GitHub Releases) via libcurl + extrator ZIP mínimo usando zlib.
// Nunca sobrescreve saves/opções (data/), dumps do jogo (game/), logs/ nem launcher.cfg.
#include "updater.h"

#include <switch.h>
#include <curl/curl.h>
#include <zlib.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef VERSAO
#define VERSAO "dev"
#endif

#define UPD_JSON_MAX   (256 * 1024)
#define UPD_CHUNK_SIZE (32 * 1024)
#define UPD_MAX_ENTRY  (256ULL * 1024ULL * 1024ULL)

static Mutex g_mtx;
static int g_mtx_init = 0;
static Thread g_thr;
static int g_thr_active = 0;
static volatile int g_cancel = 0;
static UpdStatus g_st;

static char g_cur_ver[64];
static char g_dest_dir[FS_MAX_PATH];
static char g_self_nro[FS_MAX_PATH];

static void mtx_ensure(void) {
    if (!g_mtx_init) {
        mutexInit(&g_mtx);
        g_mtx_init = 1;
    }
}

static void st_set_error(UpdError err, long http_code) {
    mutexLock(&g_mtx);
    g_st.state = UPD_ERROR;
    g_st.err = err;
    g_st.http_code = http_code;
    mutexUnlock(&g_mtx);
}

// ---------------------------------------------------------------- comparação de versões
static int parse_ver_nums(const char *s, int out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == 'v' || *s == 'V') s++;
    if (!isdigit((unsigned char)*s)) return 0;
    int idx = 0;
    while (*s && idx < 4) {
        if (!isdigit((unsigned char)*s)) break;
        int val = 0;
        while (isdigit((unsigned char)*s)) {
            if (val < 100000) val = val * 10 + (*s - '0');
            s++;
        }
        out[idx++] = val;
        if (*s == '.') s++;
        else break;
    }
    return idx;
}

int upd_version_cmp(const char *a, const char *b) {
    int va[4], vb[4];
    parse_ver_nums(a, va);
    parse_ver_nums(b, vb);
    for (int i = 0; i < 4; i++) {
        if (va[i] < vb[i]) return -1;
        if (va[i] > vb[i]) return 1;
    }
    return 0;
}

// ---------------------------------------------------------------- parser JSON mínimo (GitHub Releases)
static int ends_with_ci(const char *s, const char *sfx) {
    size_t ls = strlen(s), lf = strlen(sfx);
    if (ls < lf) return 0;
    s += ls - lf;
    for (size_t i = 0; i < lf; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)sfx[i])) return 0;
    return 1;
}

static int eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int url_permitida(const char *url) {
    return !strncmp(url, "https://github.com/", 19) ||
           !strncmp(url, "https://objects.githubusercontent.com/", 38) ||
           !strncmp(url, "https://release-assets.githubusercontent.com/", 45);
}

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static const char *read_json_str(const char *p, char *out, size_t out_sz) {
    if (*p != '"') return NULL;
    p++;
    size_t w = 0;
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            if (!*p) return NULL;
            char e = *p++;
            if (e == '"' || e == '\\' || e == '/') c = (unsigned char)e;
            else if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else if (e == 'u') {
                for (int k = 0; k < 4 && *p; k++) p++;
                c = '?';
            } else c = (unsigned char)e;
        }
        if (out && out_sz && w + 1 < out_sz) out[w++] = (char)c;
    }
    if (out && out_sz) out[w] = 0;
    return *p == '"' ? p + 1 : NULL;
}

static const char *skip_json_val(const char *p) {
    p = skip_ws(p);
    if (*p == '"') return read_json_str(p, NULL, 0);
    if (*p == '{' || *p == '[') {
        int d = 0;
        while (*p) {
            if (*p == '"') { p = read_json_str(p, NULL, 0); if (!p) return NULL; continue; }
            if (*p == '{' || *p == '[') d++;
            else if (*p == '}' || *p == ']') { if (--d == 0) return p + 1; }
            p++;
        }
        return NULL;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    return p;
}

int upd_parse_release_json(const char *json, char *tag_out, size_t tag_sz,
                           char *name_out, size_t name_sz,
                           char *url_out, size_t url_sz,
                           uint64_t *size_out, int *is_zip_out) {
    if (!json || !tag_out || !name_out || !url_out || !size_out || !is_zip_out) return 0;
    tag_out[0] = name_out[0] = url_out[0] = 0;
    *size_out = 0;
    *is_zip_out = 0;

    const char *p = skip_ws(json);
    if (*p != '{') return 0;
    p++;

    int best_score = 0;
    while (*p) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }
        if (*p != '"') break;
        char key[64];
        p = read_json_str(p, key, sizeof key);
        if (!p) return 0;
        p = skip_ws(p);
        if (*p != ':') return 0;
        p = skip_ws(p + 1);

        if (!strcmp(key, "tag_name") && *p == '"') {
            p = read_json_str(p, tag_out, tag_sz);
            if (!p) return 0;
        } else if (!strcmp(key, "assets") && *p == '[') {
            p++;
            while (*p) {
                p = skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                if (*p != '{') { p = skip_json_val(p); if (!p) return 0; continue; }
                p++;
                char aname[128] = {0}, aurl[512] = {0};
                uint64_t asize = 0;
                while (*p) {
                    p = skip_ws(p);
                    if (*p == '}') { p++; break; }
                    if (*p == ',') { p++; continue; }
                    if (*p != '"') return 0;
                    char akey[64];
                    p = read_json_str(p, akey, sizeof akey);
                    if (!p) return 0;
                    p = skip_ws(p);
                    if (*p != ':') return 0;
                    p = skip_ws(p + 1);
                    if (!strcmp(akey, "name") && *p == '"') {
                        p = read_json_str(p, aname, sizeof aname);
                    } else if (!strcmp(akey, "browser_download_url") && *p == '"') {
                        p = read_json_str(p, aurl, sizeof aurl);
                    } else if (!strcmp(akey, "size") && isdigit((unsigned char)*p)) {
                        asize = strtoull(p, (char **)&p, 10);
                    } else {
                        p = skip_json_val(p);
                    }
                    if (!p) return 0;
                }
                if (aname[0] && aurl[0] && url_permitida(aurl)) {
                    int score = 0, is_zip = 0;
                    if (eq_ci(aname, "gow-nx.zip")) { score = 3; is_zip = 1; }
                    else if (ends_with_ci(aname, ".zip")) { score = 2; is_zip = 1; }
                    else if (eq_ci(aname, "gow-nx.nro")) { score = 1; is_zip = 0; }
                    if (score > best_score) {
                        best_score = score;
                        snprintf(name_out, name_sz, "%s", aname);
                        snprintf(url_out, url_sz, "%s", aurl);
                        *size_out = asize;
                        *is_zip_out = is_zip;
                    }
                }
            }
        } else {
            p = skip_json_val(p);
            if (!p) return 0;
        }
    }
    return tag_out[0] != 0;
}

// ---------------------------------------------------------------- extrator ZIP seguro (zlib)
static uint16_t rd16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static int mkdir_p(const char *filepath) {
    char tmp[FS_MAX_PATH];
    snprintf(tmp, sizeof tmp, "%s", filepath);
    char *slash = strrchr(tmp, '/');
    if (!slash) return 1;
    *slash = 0;
    char *start = tmp;
    char *colon = strchr(tmp, ':');
    if (colon) start = colon + 1;
    for (char *p = start; *p; p++) {
        if (*p == '/' && p > start) {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
    return 1;
}

// Retorna: 1 = extrair (out preenchido), 0 = ignorar com segurança, -1 = caminho inválido/inseguro.
int upd_normalize_zip_path(const char *entry, char *out, size_t out_sz) {
    if (!entry || !out || out_sz < 2) return -1;
    char buf[512];
    size_t len = strlen(entry);
    if (len == 0 || len >= sizeof buf) return -1;
    for (size_t i = 0; i <= len; i++) buf[i] = (entry[i] == '\\') ? '/' : entry[i];

    // Rejeita drives/protocolos e segmentos ".."
    if (strchr(buf, ':')) return -1;
    const char *s = buf;
    while (*s == '/') s++;

    // Remove prefixos comuns de empacotamento (ex.: switch/gow-nx/ ou gow-nx/)
    int stripped = 1;
    while (stripped) {
        stripped = 0;
        if (!strncmp(s, "./", 2)) { s += 2; stripped = 1; }
        else if (!strncmp(s, "sdmc/switch/gow-nx/", 19)) { s += 19; stripped = 1; }
        else if (!strncmp(s, "switch/gow-nx/", 14)) { s += 14; stripped = 1; }
        else if (!strncmp(s, "gow-nx/", 7)) { s += 7; stripped = 1; }
    }
    if (!*s || s[strlen(s) - 1] == '/') return 0; // diretório

    // Valida cada segmento do caminho
    const char *seg = s;
    while (*seg) {
        const char *next = strchr(seg, '/');
        size_t slen = next ? (size_t)(next - seg) : strlen(seg);
        if (slen == 0) return -1;
        if ((slen == 2 && seg[0] == '.' && seg[1] == '.') || (slen == 1 && seg[0] == '.')) return -1;
        if (!strncmp(seg, "._", 2) || (slen == 9 && !strncmp(seg, ".DS_Store", 9))) return 0;
        if (!next) break;
        seg = next + 1;
    }

    // Protege dados do usuário (saves, opções, jogo, logs)
    if (!strncmp(s, "data/", 5) || !strcmp(s, "data") ||
        !strncmp(s, "game/", 5) || !strcmp(s, "game") ||
        !strncmp(s, "logs/", 5) || !strcmp(s, "logs") ||
        !strncmp(s, "__MACOSX/", 9) ||
        !strcmp(s, "launcher.cfg") || !strcmp(s, "nvk_debug.txt"))
        return 0;

    if (strlen(s) >= out_sz) return -1;
    strcpy(out, s);
    return 1;
}

static int find_eocd(FILE *f, uint64_t fsize, uint16_t *entries, uint32_t *cd_size, uint32_t *cd_off) {
    if (fsize < 22) return 0;
    size_t tail = (fsize > 65557ULL) ? 65557 : (size_t)fsize;
    uint8_t *buf = (uint8_t *)malloc(tail);
    if (!buf) return 0;
    if (fseek(f, (long)(fsize - tail), SEEK_SET) != 0 || fread(buf, 1, tail, f) != tail) {
        free(buf);
        return 0;
    }
    int found = 0;
    for (size_t i = tail - 22; ; i--) {
        if (rd32(buf + i) == 0x06054b50u) {
            uint16_t clen = rd16(buf + i + 20);
            if (i + 22 + clen == tail) {
                *entries = rd16(buf + i + 10);
                *cd_size = rd32(buf + i + 12);
                *cd_off = rd32(buf + i + 16);
                found = 1;
                break;
            }
        }
        if (i == 0) break;
    }
    free(buf);
    return found;
}

static int extract_entry_stream(FILE *zf, FILE *out, uint16_t method,
                                uint32_t comp_size, uint32_t uncomp_size,
                                uint32_t expected_crc, volatile int *cancel_flag) {
    if (uncomp_size > UPD_MAX_ENTRY || comp_size > UPD_MAX_ENTRY) return 0;
    uint8_t *inbuf = (uint8_t *)malloc(UPD_CHUNK_SIZE);
    uint8_t *outbuf = (method == 8) ? (uint8_t *)malloc(UPD_CHUNK_SIZE) : NULL;
    if (!inbuf || (method == 8 && !outbuf)) { free(inbuf); free(outbuf); return 0; }

    uLong crc = crc32(0L, Z_NULL, 0);
    uint64_t written = 0;
    int ok = 0;

    if (method == 0) {
        if (comp_size != uncomp_size) goto cleanup;
        uint32_t rem = comp_size;
        while (rem > 0) {
            if (cancel_flag && *cancel_flag) goto cleanup;
            size_t chunk = rem > UPD_CHUNK_SIZE ? UPD_CHUNK_SIZE : rem;
            if (fread(inbuf, 1, chunk, zf) != chunk) goto cleanup;
            if (fwrite(inbuf, 1, chunk, out) != chunk) goto cleanup;
            crc = crc32(crc, inbuf, (uInt)chunk);
            written += chunk;
            rem -= (uint32_t)chunk;
        }
        ok = (written == uncomp_size && (uint32_t)crc == expected_crc);
    } else if (method == 8) {
        z_stream zs;
        memset(&zs, 0, sizeof zs);
        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) goto cleanup;
        uint32_t rem = comp_size;
        int zret = Z_OK;
        while (zret != Z_STREAM_END) {
            if (cancel_flag && *cancel_flag) { inflateEnd(&zs); goto cleanup; }
            if (zs.avail_in == 0) {
                if (rem == 0) { inflateEnd(&zs); goto cleanup; }
                size_t chunk = rem > UPD_CHUNK_SIZE ? UPD_CHUNK_SIZE : rem;
                if (fread(inbuf, 1, chunk, zf) != chunk) { inflateEnd(&zs); goto cleanup; }
                zs.next_in = inbuf;
                zs.avail_in = (uInt)chunk;
                rem -= (uint32_t)chunk;
            }
            zs.next_out = outbuf;
            zs.avail_out = UPD_CHUNK_SIZE;
            zret = inflate(&zs, Z_NO_FLUSH);
            if (zret != Z_OK && zret != Z_STREAM_END) { inflateEnd(&zs); goto cleanup; }
            size_t have = UPD_CHUNK_SIZE - zs.avail_out;
            if (have > 0) {
                if (written + have > uncomp_size || fwrite(outbuf, 1, have, out) != have) {
                    inflateEnd(&zs);
                    goto cleanup;
                }
                crc = crc32(crc, outbuf, (uInt)have);
                written += have;
            }
        }
        inflateEnd(&zs);
        ok = (written == uncomp_size && (uint32_t)crc == expected_crc);
    }

cleanup:
    free(inbuf);
    free(outbuf);
    return ok;
}

int upd_extract_zip(const char *zip_path, const char *dest_dir, const char *self_nro,
                    volatile int *cancel_flag,
                    void (*progress_cb)(int done, int total, const char *file)) {
    FILE *zf = fopen(zip_path, "rb");
    if (!zf) return 0;
    if (fseek(zf, 0, SEEK_END) != 0) { fclose(zf); return 0; }
    long fsz_l = ftell(zf);
    if (fsz_l < 22) { fclose(zf); return 0; }
    uint64_t fsize = (uint64_t)fsz_l;

    uint16_t entries = 0;
    uint32_t cd_size = 0, cd_off = 0;
    if (!find_eocd(zf, fsize, &entries, &cd_size, &cd_off) ||
        (uint64_t)cd_off + (uint64_t)cd_size > fsize || entries == 0) {
        fclose(zf);
        return 0;
    }

    uint8_t *cd = (uint8_t *)malloc(cd_size);
    if (!cd) { fclose(zf); return 0; }
    if (fseek(zf, (long)cd_off, SEEK_SET) != 0 || fread(cd, 1, cd_size, zf) != cd_size) {
        free(cd); fclose(zf); return 0;
    }

    // 1ª passagem: valida entradas e conta quantos arquivos serão extraídos
    int total_files = 0;
    uint32_t pos = 0;
    for (uint16_t i = 0; i < entries; i++) {
        if (pos + 46 > cd_size || rd32(cd + pos) != 0x02014b50u) { free(cd); fclose(zf); return 0; }
        uint16_t nlen = rd16(cd + pos + 28), elen = rd16(cd + pos + 30), clen = rd16(cd + pos + 32);
        if (pos + 46 + nlen + elen + clen > cd_size || nlen >= 512) { free(cd); fclose(zf); return 0; }
        char raw[512], rel[512];
        memcpy(raw, cd + pos + 46, nlen); raw[nlen] = 0;
        int norm = upd_normalize_zip_path(raw, rel, sizeof rel);
        if (norm < 0) { free(cd); fclose(zf); return 0; }
        if (norm > 0) total_files++;
        pos += 46u + nlen + elen + clen;
    }
    if (total_files == 0) { free(cd); fclose(zf); return 0; }

    // 2ª passagem: extrai cada arquivo atomicamente (.upd_tmp -> rename)
    int done = 0;
    pos = 0;
    for (uint16_t i = 0; i < entries; i++) {
        if (cancel_flag && *cancel_flag) { free(cd); fclose(zf); return 0; }
        uint16_t method = rd16(cd + pos + 10);
        uint32_t crc = rd32(cd + pos + 16);
        uint32_t comp_sz = rd32(cd + pos + 20);
        uint32_t uncomp_sz = rd32(cd + pos + 24);
        uint16_t nlen = rd16(cd + pos + 28), elen = rd16(cd + pos + 30), clen = rd16(cd + pos + 32);
        uint32_t local_off = rd32(cd + pos + 42);
        char raw[512], rel[512];
        memcpy(raw, cd + pos + 46, nlen); raw[nlen] = 0;
        pos += 46u + nlen + elen + clen;

        if (upd_normalize_zip_path(raw, rel, sizeof rel) <= 0) continue;

        char dest[FS_MAX_PATH], tmp_dest[FS_MAX_PATH + 12];
        if (!strcmp(rel, "gow-nx.nro") && self_nro && self_nro[0])
            snprintf(dest, sizeof dest, "%s", self_nro);
        else
            snprintf(dest, sizeof dest, "%.500s%.250s", dest_dir, rel);

        // Não sobrescreve capa/música personalizadas que já estejam no SD
        if ((!strcmp(rel, "bg.bgra") || !strcmp(rel, "music.mp3")) && file_exists(dest)) {
            if (progress_cb) progress_cb(++done, total_files, rel);
            continue;
        }

        if (progress_cb) progress_cb(done, total_files, rel);
        mkdir_p(dest);

        if ((uint64_t)local_off + 30 > fsize || fseek(zf, (long)local_off, SEEK_SET) != 0) {
            free(cd); fclose(zf); return 0;
        }
        uint8_t lh[30];
        if (fread(lh, 1, 30, zf) != 30 || rd32(lh) != 0x04034b50u) {
            free(cd); fclose(zf); return 0;
        }
        uint64_t data_off = (uint64_t)local_off + 30u + rd16(lh + 26) + rd16(lh + 28);
        if (data_off + comp_sz > fsize || fseek(zf, (long)data_off, SEEK_SET) != 0) {
            free(cd); fclose(zf); return 0;
        }

        snprintf(tmp_dest, sizeof tmp_dest, "%s.upd_tmp", dest);
        FILE *out = fopen(tmp_dest, "wb");
        if (!out) { free(cd); fclose(zf); return 0; }
        int ok = extract_entry_stream(zf, out, method, comp_sz, uncomp_sz, crc, cancel_flag);
        if (ferror(out)) ok = 0;
        if (fclose(out) != 0) ok = 0;
        if (!ok) {
            remove(tmp_dest);
            free(cd); fclose(zf); return 0;
        }
        remove(dest);
        if (rename(tmp_dest, dest) != 0) {
            remove(tmp_dest);
            free(cd); fclose(zf); return 0;
        }
        if (progress_cb) progress_cb(++done, total_files, rel);
    }

    free(cd);
    fclose(zf);
    return done > 0;
}

// ---------------------------------------------------------------- rede (libcurl)
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} MemBuf;

static size_t write_mem_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    if (g_cancel) return 0;
    MemBuf *m = (MemBuf *)userdata;
    size_t n = size * nmemb;
    if (m->len + n + 1 > UPD_JSON_MAX) return 0;
    if (m->len + n + 1 > m->cap) {
        size_t ncap = m->cap ? m->cap * 2 : 8192;
        while (ncap < m->len + n + 1) ncap *= 2;
        if (ncap > UPD_JSON_MAX) ncap = UPD_JSON_MAX;
        char *nb = (char *)realloc(m->buf, ncap);
        if (!nb) return 0;
        m->buf = nb;
        m->cap = ncap;
    }
    memcpy(m->buf + m->len, ptr, n);
    m->len += n;
    m->buf[m->len] = 0;
    return n;
}

static size_t write_file_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    if (g_cancel) return 0;
    return fwrite(ptr, size, nmemb, (FILE *)userdata);
}

static int xfer_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;
    if (g_cancel) return 1;
    mutexLock(&g_mtx);
    g_st.dl_now = dlnow > 0 ? (uint64_t)dlnow : 0;
    if (dltotal > 0) g_st.dl_total = (uint64_t)dltotal;
    mutexUnlock(&g_mtx);
    return 0;
}

static void configure_curl_common(CURL *curl, const char *url, long timeout_sec) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "gow-collection-nx-launcher/" VERSAO);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    if (timeout_sec > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
}

static void check_thread(void *arg) {
    (void)arg;
    if (R_FAILED(socketInitializeDefault())) {
        st_set_error(UPD_ERR_NET, 0);
        return;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        socketExit();
        st_set_error(UPD_ERR_NET, 0);
        return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        socketExit();
        st_set_error(UPD_ERR_NET, 0);
        return;
    }

    char api_url[256];
    snprintf(api_url, sizeof api_url, "https://api.github.com/repos/%s/releases/latest", UPDATE_REPO);
    MemBuf mb = {0};
    struct curl_slist *hdrs = curl_slist_append(NULL, "Accept: application/vnd.github+json");
    configure_curl_common(curl, api_url, 20L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mb);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    socketExit();

    if (g_cancel) {
        free(mb.buf);
        st_set_error(UPD_ERR_CANCELLED, 0);
        return;
    }
    if (res != CURLE_OK) {
        free(mb.buf);
        st_set_error(UPD_ERR_NET, code);
        return;
    }
    if (code != 200 || !mb.buf) {
        free(mb.buf);
        st_set_error(UPD_ERR_HTTP, code);
        return;
    }

    char tag[64], name[128], url[512];
    uint64_t sz = 0;
    int is_zip = 0;
    int parsed = upd_parse_release_json(mb.buf, tag, sizeof tag, name, sizeof name, url, sizeof url, &sz, &is_zip);
    free(mb.buf);

    if (!parsed || !url[0]) {
        st_set_error(UPD_ERR_NO_ASSET, code);
        return;
    }

    int cmp = upd_version_cmp(g_cur_ver, tag);
    mutexLock(&g_mtx);
    snprintf(g_st.latest_tag, sizeof g_st.latest_tag, "%s", tag);
    snprintf(g_st.asset_name, sizeof g_st.asset_name, "%s", name);
    snprintf(g_st.asset_url, sizeof g_st.asset_url, "%s", url);
    g_st.asset_size = sz;
    g_st.asset_is_zip = is_zip;
    g_st.dl_total = sz;
    g_st.http_code = code;
    g_st.err = UPD_ERR_NONE;
    g_st.state = (cmp < 0) ? UPD_AVAILABLE : UPD_UP_TO_DATE;
    mutexUnlock(&g_mtx);
}

static void extract_progress(int done, int total, const char *file) {
    mutexLock(&g_mtx);
    g_st.files_done = done;
    g_st.files_total = total;
    snprintf(g_st.current_file, sizeof g_st.current_file, "%s", file ? file : "");
    mutexUnlock(&g_mtx);
}

static int verify_nro_magic(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t hdr[0x20];
    size_t n = fread(hdr, 1, sizeof hdr, f);
    fclose(f);
    return n == sizeof hdr && memcmp(hdr + 0x10, "NRO0", 4) == 0;
}

static void install_thread(void *arg) {
    (void)arg;
    char url[512];
    int is_zip = 0;
    uint64_t expected_sz = 0;

    mutexLock(&g_mtx);
    snprintf(url, sizeof url, "%s", g_st.asset_url);
    is_zip = g_st.asset_is_zip;
    expected_sz = g_st.asset_size;
    g_st.state = UPD_DOWNLOADING;
    g_st.dl_now = 0;
    g_st.dl_total = expected_sz;
    g_st.err = UPD_ERR_NONE;
    mutexUnlock(&g_mtx);

    char dl_path[FS_MAX_PATH + 16];
    if (is_zip) snprintf(dl_path, sizeof dl_path, "%.700supdate.zip.tmp", g_dest_dir);
    else snprintf(dl_path, sizeof dl_path, "%.700s.upd_tmp", g_self_nro);

    FILE *out = fopen(dl_path, "wb");
    if (!out) {
        st_set_error(UPD_ERR_DISK, 0);
        return;
    }

    if (R_FAILED(socketInitializeDefault())) {
        fclose(out); remove(dl_path);
        st_set_error(UPD_ERR_NET, 0);
        return;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        socketExit(); fclose(out); remove(dl_path);
        st_set_error(UPD_ERR_NET, 0);
        return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup(); socketExit(); fclose(out); remove(dl_path);
        st_set_error(UPD_ERR_NET, 0);
        return;
    }

    configure_curl_common(curl, url, 0L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_cb);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    socketExit();

    int write_ok = !ferror(out);
    if (fclose(out) != 0) write_ok = 0;

    if (g_cancel) {
        remove(dl_path);
        st_set_error(UPD_ERR_CANCELLED, 0);
        return;
    }
    if (!write_ok) {
        remove(dl_path);
        st_set_error(UPD_ERR_DISK, code);
        return;
    }
    if (res != CURLE_OK) {
        remove(dl_path);
        st_set_error(UPD_ERR_NET, code);
        return;
    }
    if (code != 200) {
        remove(dl_path);
        st_set_error(UPD_ERR_HTTP, code);
        return;
    }

    mutexLock(&g_mtx);
    g_st.state = UPD_EXTRACTING;
    g_st.files_done = 0;
    g_st.files_total = is_zip ? 0 : 1;
    g_st.current_file[0] = 0;
    mutexUnlock(&g_mtx);

    if (is_zip) {
        int ok = upd_extract_zip(dl_path, g_dest_dir, g_self_nro, &g_cancel, extract_progress);
        remove(dl_path);
        if (g_cancel) { st_set_error(UPD_ERR_CANCELLED, 0); return; }
        if (!ok) { st_set_error(UPD_ERR_ZIP, 0); return; }
    } else {
        if (!verify_nro_magic(dl_path)) {
            remove(dl_path);
            st_set_error(UPD_ERR_ZIP, 0);
            return;
        }
        remove(g_self_nro);
        if (rename(dl_path, g_self_nro) != 0) {
            remove(dl_path);
            st_set_error(UPD_ERR_DISK, 0);
            return;
        }
        extract_progress(1, 1, "gow-nx.nro");
    }

    mutexLock(&g_mtx);
    g_st.state = UPD_DONE;
    g_st.err = UPD_ERR_NONE;
    mutexUnlock(&g_mtx);
}

static void join_prev_thread(void) {
    if (!g_thr_active) return;
    threadWaitForExit(&g_thr);
    threadClose(&g_thr);
    g_thr_active = 0;
}

void upd_start_check(const char *current_ver) {
    mtx_ensure();
    g_cancel = 1;
    join_prev_thread();
    g_cancel = 0;

    snprintf(g_cur_ver, sizeof g_cur_ver, "%s", current_ver ? current_ver : "0");
    mutexLock(&g_mtx);
    memset(&g_st, 0, sizeof g_st);
    g_st.state = UPD_CHECKING;
    mutexUnlock(&g_mtx);

    if (R_FAILED(threadCreate(&g_thr, check_thread, NULL, NULL, 0x40000, 0x2C, -2))) {
        st_set_error(UPD_ERR_NET, 0);
        return;
    }
    threadStart(&g_thr);
    g_thr_active = 1;
}

void upd_start_install(const char *dir, const char *self_nro) {
    mtx_ensure();
    join_prev_thread();
    g_cancel = 0;

    snprintf(g_dest_dir, sizeof g_dest_dir, "%s", dir ? dir : "sdmc:/switch/gow-nx/");
    if (self_nro && self_nro[0]) snprintf(g_self_nro, sizeof g_self_nro, "%s", self_nro);
    else snprintf(g_self_nro, sizeof g_self_nro, "%.700sgow-nx.nro", g_dest_dir);

    if (R_FAILED(threadCreate(&g_thr, install_thread, NULL, NULL, 0x40000, 0x2C, -2))) {
        st_set_error(UPD_ERR_NET, 0);
        return;
    }
    threadStart(&g_thr);
    g_thr_active = 1;
}

void upd_cancel(int wait) {
    g_cancel = 1;
    if (wait) join_prev_thread();
}

void upd_get_status(UpdStatus *out) {
    if (!out) return;
    mtx_ensure();
    mutexLock(&g_mtx);
    *out = g_st;
    mutexUnlock(&g_mtx);
}

void upd_shutdown(void) {
    g_cancel = 1;
    join_prev_thread();
}
