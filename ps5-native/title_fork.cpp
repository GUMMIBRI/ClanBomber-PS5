#include <cerrno>
#include <sys/types.h>

// Native titles cannot spawn a helper for SDL's Unix URL launcher.
extern "C" pid_t __wrap_fork() {
    errno = ENOSYS;
    return -1;
}
