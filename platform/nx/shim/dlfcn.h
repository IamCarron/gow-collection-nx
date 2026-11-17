/* dlfcn.h vazio para o volk.c no Switch: sem loader; o volk recebe o vk_icdGetInstanceProcAddr do NVK. */
#pragma once
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define RTLD_LAZY 1
static inline void *dlopen(const char *f, int m) { (void)f; (void)m; return 0; }
static inline void *dlsym(void *h, const char *s) { (void)h; (void)s; return 0; }
static inline int dlclose(void *h) { (void)h; return 0; }
