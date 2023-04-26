/* Type of a signal handler.  */
typedef void (*__sighandler_t) (int);

#define _SIGSET_NWORDS (1024 / (8 * sizeof (unsigned long int)))
typedef struct
{
    unsigned long int __val[_SIGSET_NWORDS];
} __sigset_t;

int sigemptyset(__sigset_t* set);

struct sigaction
{
    /* Signal handler.  */
    __sighandler_t sa_handler;
    /* Additional set of signals to be blocked.  */
    __sigset_t sa_mask;
    /* Special flags.  */
    int sa_flags;
    /* Restore handler.  */
    void (*sa_restorer) (void);
};

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact);


#define        SIGSTKFLT        16        /* Stack fault (obsolete).  */
#define        SIGPWR                30        /* Power failure imminent.  */
#undef        SIGBUS
#define        SIGBUS                 7
#undef        SIGUSR1
#define        SIGUSR1                10
#undef        SIGUSR2
#define        SIGUSR2                12
#undef        SIGCHLD
#define        SIGCHLD                17
#undef        SIGCONT
#define        SIGCONT                18
#undef        SIGSTOP
#define        SIGSTOP                19
#undef        SIGTSTP
#define        SIGTSTP                20
#undef        SIGURG
#define        SIGURG                23
#undef        SIGPOLL
#define        SIGPOLL                29
#undef        SIGSYS
#define SIGSYS                31

#ifdef __cplusplus
extern "C"
#endif
    unsigned int sleep(unsigned int seconds);
