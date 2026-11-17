/* Funções POSIX que o NVK (libnvk_support) e o vitart pedem e o newlib do Switch não traz. Nenhuma delas é
 * caminho de desempenho: cache de disco do Mesa desligado (MESA_SHADER_CACHE_DISABLE), sem usuários, sem sinais. */
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <switch.h>

int posix_memalign(void **out, size_t align, size_t size) {
    if (align < sizeof(void *)) align = sizeof(void *);
    void *p = memalign(align, size ? size : 1);
    if (!p) return ENOMEM;
    *out = p;
    return 0;
}

long sysconf(int name) {
    switch (name) {
    case _SC_PAGESIZE: return 0x1000;
#ifdef _SC_NPROCESSORS_ONLN
    case _SC_NPROCESSORS_ONLN: return 3; /* núcleos para o aplicativo */
#endif
#ifdef _SC_NPROCESSORS_CONF
    case _SC_NPROCESSORS_CONF: return 3;
#endif
#ifdef _SC_PHYS_PAGES
    case _SC_PHYS_PAGES: {
        u64 total = 0;
        svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        return (long)(total / 0x1000);
    }
#endif
#ifdef _SC_AVPHYS_PAGES
    case _SC_AVPHYS_PAGES: {
        u64 total = 0, used = 0;
        svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
        return (long)((total - used) / 0x1000);
    }
#endif
    default: errno = EINVAL; return -1;
    }
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }
struct passwd;
int getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t n, struct passwd **res) {
    (void)uid, (void)pw, (void)buf, (void)n;
    *res = 0;
    return ENOENT;
}

int flock(int fd, int op) { (void)fd, (void)op; return 0; }
int pthread_sigmask(int how, const sigset_t *set, sigset_t *old) {
    (void)how, (void)set;
    if (old) memset(old, 0, sizeof *old);
    return 0;
}

int dirfd(DIR *d) { (void)d; errno = ENOTSUP; return -1; }
int fstatat(int dirfd_, const char *path, struct stat *st, int flags) {
    (void)flags;
    if (path[0] != '/' && !strchr(path, ':') && dirfd_ >= 0) { errno = ENOTSUP; return -1; }
    return stat(path, st);
}

/* pread/pwrite: o newlib do Switch não tem; com uma trava global ficam atômicos entre si. */
static pthread_mutex_t g_pio = PTHREAD_MUTEX_INITIALIZER;
static ssize_t pio(int fd, void *buf, size_t n, off_t off, int wr) {
    pthread_mutex_lock(&g_pio);
    const off_t cur = lseek(fd, 0, SEEK_CUR);
    ssize_t r = -1;
    if (lseek(fd, off, SEEK_SET) == off) r = wr ? write(fd, buf, n) : read(fd, buf, n);
    if (cur >= 0) lseek(fd, cur, SEEK_SET);
    pthread_mutex_unlock(&g_pio);
    return r;
}
ssize_t pread(int fd, void *buf, size_t n, off_t off) { return pio(fd, buf, n, off, 0); }
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off) { return pio(fd, (void *)buf, n, off, 1); }

/* regex: o Mesa só usa para filtros de depuração por variável de ambiente; nunca casa. */
typedef struct { size_t re_nsub; void *p; } regex_t_nx;
int regcomp(void *re, const char *pat, int flags) { (void)re, (void)pat, (void)flags; return 1; }
int regexec(const void *re, const char *s, size_t n, void *m, int flags) { (void)re, (void)s, (void)n, (void)m, (void)flags; return 1; }
void regfree(void *re) { (void)re; }
