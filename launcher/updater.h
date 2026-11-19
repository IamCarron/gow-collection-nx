#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef UPDATE_REPO
#define UPDATE_REPO "gerisson1996/gow-collection-nx"
#endif

typedef enum {
    UPD_IDLE = 0,
    UPD_CHECKING,
    UPD_UP_TO_DATE,
    UPD_AVAILABLE,
    UPD_DOWNLOADING,
    UPD_EXTRACTING,
    UPD_DONE,
    UPD_ERROR,
} UpdState;

typedef enum {
    UPD_ERR_NONE = 0,
    UPD_ERR_NET,
    UPD_ERR_HTTP,
    UPD_ERR_NO_ASSET,
    UPD_ERR_DISK,
    UPD_ERR_ZIP,
    UPD_ERR_CANCELLED,
} UpdError;

typedef struct {
    UpdState state;
    UpdError err;
    long http_code;
    char latest_tag[64];
    char asset_name[128];
    char asset_url[512];
    uint64_t asset_size;
    int asset_is_zip;
    uint64_t dl_now;
    uint64_t dl_total;
    int files_done;
    int files_total;
    char current_file[128];
} UpdStatus;

void upd_start_check(const char *current_ver);
void upd_start_install(const char *dir, const char *self_nro);
void upd_cancel(int wait);
void upd_get_status(UpdStatus *out);
void upd_shutdown(void);

// Auxiliares puros (também usados em testes unitários)
int upd_version_cmp(const char *a, const char *b);
int upd_parse_release_json(const char *json, char *tag_out, size_t tag_sz,
                           char *name_out, size_t name_sz,
                           char *url_out, size_t url_sz,
                           uint64_t *size_out, int *is_zip_out);
int upd_normalize_zip_path(const char *entry, char *out, size_t out_sz);
int upd_extract_zip(const char *zip_path, const char *dest_dir, const char *self_nro,
                    volatile int *cancel_flag,
                    void (*progress_cb)(int done, int total, const char *file));
