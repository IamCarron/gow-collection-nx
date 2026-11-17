// io.h do CRT da Microsoft sobre o POSIX do newlib (build do Switch).
#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define _open open
#define _close close
#define _read read
#define _write write
#define _lseeki64 lseek
#define _O_BINARY 0
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR O_RDWR
#define _O_APPEND O_APPEND
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_EXCL O_EXCL
#define _S_IREAD 0400
#define _S_IWRITE 0200
