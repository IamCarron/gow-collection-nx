// Incluído à força (-include) em todo o runtime do Switch: equivalentes das funções do CRT da Microsoft.
#pragma once
#ifdef __cplusplus
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <sys/types.h>

#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _fseeki64 fseeko
#define _ftelli64 ftello

inline int gmtime_s(struct tm *out, const time_t *t) { return gmtime_r(t, out) ? 0 : 1; }
inline int localtime_s(struct tm *out, const time_t *t) { return localtime_r(t, out) ? 0 : 1; }
inline time_t _mkgmtime(struct tm *tm) {
    // timegm sem depender do newlib: dias desde 1970 pelo algoritmo civil
    int y = tm->tm_year + 1900, m = tm->tm_mon + 1;
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + tm->tm_mday - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long days = era * 146097 + (long)doe - 719468;
    return (time_t)days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}
#define timegm _mkgmtime
#endif
