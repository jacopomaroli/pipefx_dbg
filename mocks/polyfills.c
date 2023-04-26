#include <errno.h>
#include <string.h>
#include <signal.h>
#include <windows.h>

#include "polyfills.h"

int sigemptyset(__sigset_t* set) {
    if (set == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(set, 0, sizeof(__sigset_t));
    return 0;
}

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact)
{
    signal(signum, act->sa_handler);
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    Sleep(seconds * 1000);
    return 0;
}
