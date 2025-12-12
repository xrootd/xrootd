#undef NDEBUG

/* Clang's _FORTIFY_SOURCE breaks XrdPosixPreload */
#if defined(__clang__) && defined(_FORTIFY_SOURCE)
#undef _FORTIFY_SOURCE
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char *argv[]) {
#if defined(__linux__)

    struct statx stx;

    if (statx(AT_FDCWD, argv[1],
              AT_STATX_SYNC_AS_STAT,
              STATX_BTIME | STATX_BASIC_STATS,
              &stx) == -1) {
        perror("statx");
        return 1;
              }

    if (stx.stx_mask & STATX_BTIME) {
        printf("Birth: %lld.%09u\n",
               (long long)stx.stx_btime.tv_sec,
               stx.stx_btime.tv_nsec);
    } else {
        printf("Birth time not supported by filesystem\n");
    }

    return 0;
#else
    return 0;
#endif
}