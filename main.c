/*
    daemon skeleton

    Sets up some well-known keys in a redis server, and listens for changes, reacting appropriately.

    Copyright (c) Paul Chambers <paul@chambers.name>
*/

#include <stdio.h>
#include <stdlib.h>     /* core functions */
#include <unistd.h>     /* POSIX API (fork/exec, etc) */
#include <signal.h>     /* signal handling */
#include <fcntl.h>      /* file handling */
#include <stdbool.h>    /* C99 boolean types */
#include <errno.h>      /* provides global variable errno */
#include <string.h>     /* basic string functions */
#include <sys/stat.h>   /* inode manipulation (needed for umask()) */
#include <sys/wait.h>   /* for waitpid() and friends on linux */

#include "common.h"     /* common stuff */
#include "config.h"     /* config file & command line configuration parsing */
#include "background.h"

#include "logging.h"    /* our logging support */

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

/*
 * global variables
 */

const char *    gExecName;      /* base name of the execuatable, derived from argv[0]. Same for all processes */
const char *    gProcessName;   /* Name of this process/instance - different in each process */

/*
 * FUNCTIONS
 */

int     trapSignals(bool on);
int     daemonize(bool inForeground);

/*
 * Main entry point.
 * parse command line options and launch background process.
 *
 */
int main(int argc, const char *argv[])
{
    int                   status;
    eLogDestination       logTo;
    kConfigurationOptions *options;

    /* extract the executable name */
    gExecName = strrchr(argv[0], '/');
    if (gExecName == NULL)
    {
        gExecName = argv[0]; // no slash, take as-is
    }
    else
    {
        ++gExecName; // skip past the last slash
    }
    /* set initial name for this process, forked processes will change their copy */
    gProcessName = gExecName;

    initLogging( gExecName );
    // enable pre-config logging with some sensible defaults
    startLogging( kLogDebug, kLogToStderr, NULL );

    options = parseConfiguration( argc, argv );

    if (options->logFile != NULL)
    {
        logTo = kLogToFile;
    }
    else if (options->foreground)
    {
        logTo = kLogToStderr;
    }
    else
    {
        logTo = kLogToSyslog;
    }

    // re-enable logging with user-supplied configuration
    startLogging( options->debugLevel, logTo, options->logFile );

    logInfo("%s started", gExecName);

    status = daemonize(options->foreground);

    stopLogging();

    return status;
}


/*
    Go through the proper incantations to make this a proper UNIX daemon.

    If running in the foreground, this is skipped, and background() is
    called directly.
*/
int daemonize(bool inForeground)
{
    pid_t   pid;

    if (!inForeground)
    {
        /*
         * Fork is a strange and unique system call. Along with exec(), it forms one of
         * the core features of Unix.
         *
         * At the moment fork() is called,
         *   - if the fork failed, it returns -1
         *   - if the fork succeeds, a duplicate of the parent process is formed,
         *     except that the caller is designated as the parent of new process
         *   - fork() returns the pid of the newly created process
         *   - BUT IN THE CHILD process, fork returns 0
         *
         * Because the child process will see fork() returning 0, and the parent will
         * see fork() returning a pid, we can carefully diverge the behavior of both to
         * do what we want!
         */
        pid = fork();

        if (pid < 0)
        {
            /* fork failed, probably due to resource limits */
            logError("fork failed (%s [%d])", strerror(errno), errno);
            return errno;
        }
        else if (pid > 0)
        {
            /* fork successful! Tell us the forked processes's pid and exit */
            logInfo("daemon process: %d\n", pid);
            return 0;
        }
        /* Forked process continues here */

        /* Give our forked process a different name */
        gProcessName = "background";

        /* Reset master file umask in case it has been altered.
         * Notice that this is a bitmask, and not a file mode!
         *
         * e.g. umask(022); open(f, O_CREAT, 0777) => -rwxr-xr-x
         *      umask(077); open(f, O_CREAT, 0777) => -rwx------
         *      umask(0);   open(f, O_CREAT, 0777) => -rwxrwxrwx
         *
         * Setting to zero essentially allows the program to fully manage its
         * file permissions.
         */
        umask(0);

        /* Create a new session.
         *
         * Processes are not only grouped hierarchically by parent pid, but also by
         * `process groups', which are typically composed of programs launched by
         * a common process, like your shell (which of course often correspond
         * exactly with the parent-child hierarchy).
         *
         * Process groups can be targets of various signals, so it's important to
         * break from the parent's group and form a new group (aka session). Calling
         * setsid() makes this process the session leader for a new process group.
         */
        if (setsid() < 0) {
            logError("setsid() failed (%s [%d])", strerror(errno), errno);
            return errno;
        }

        /* Change working directory to root.
         *
         * Every process has a working directory. Life is better when your working
         * directory is set to something that is unlikely to disappear.
         */
        if (chdir("/") < 0) {
            logError("chdir() failed (%s [%d])", strerror(errno), errno);
            return errno;
        }

        /*
         * Trap some signals
         *
         */
        if (!trapSignals(true)) {
            logError("unable to trap signals\n");
            return -1;
        }
    }

    return background(); /* all set up, so go do some actual work */
}


/* Master's SIGCHLD handler.
 *
 * When a process is fork()ed by a process, the new process is an exact copy
 * of the old process, except for a few values, one of which is that the parent
 * pid of the child is that of the process that forked it.
 *
 * When this child exits, the signal SIGCHLD is sent to the parent process to
 * alert it. By default, the signal is ignored, but we can take this opportunity
 * to restart any children that have died.
 *
 * There are many ways to determine which children have died, but the most
 * portable method is to use the wait() family of system calls.
 *
 * A dead child process releases its memory, but sticks around so that any
 * interested parties can determine how they died (exit status). Calling wait()
 * in the master collects the status of the first available dead process, and
 * removes it from the process table.
 *
 * If wait() is never called by the parent, the dead child sticks around as a
 * "zombie" process, marked with status `Z' in ps output. If the parent process
 * exits without ever calling wait, the zombie process does not disappear, but
 * is inherited by the root process (its parent pid is set to 1).
 *
 * Because SIGCHLD is an asynchronous signal, it is possible that if many
 * children die simultaneously, the parent may only notice one SIGCHLD when many
 * have been sent. In order to beat this edge case, we can simply loop through
 * all the known children and call waitpid() in non-blocking mode to see if they
 * have died, and spawn a new one in their place.
 */
void restartChildren(int UNUSED(signal))
{
}

/* Master's kill switch
 *
 * It's important to ensure that all children have exited before the master
 * exits so no root zombies are created. The default handler for SIGINT sends
 * SIGINT to all children, but this is not true with SIGTERM.
 */
void terminateChildren(int UNUSED(signal))
{
}

/* suppress an (apparently) spurious warning */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/*
    static table used by trapSignals for registering signal handlers
 */
static struct {
    int                 signal;         /* What SIGNAL to trap */
    struct sigaction    action;         /* handler, passed to sigaction() */
} sigpairs[] = {
    { SIGCHLD,
        {
            &restartChildren,
            {},
            SA_NOCLDSTOP
        }
    },     /* Don't send SIGCHLD when a process has been frozen (e.g. Ctrl-Z) */
    { SIGINT,  { &terminateChildren } },
    { SIGTERM, { &terminateChildren } },
    { 0 } /* end of list */
};
#pragma GCC diagnostic pop

/*
 * When passed true, traps signals and assigns handlers as defined in sigpairs[]
 * When passed false, resets all trapped signals back to their default behavior.
 */
int trapSignals(bool on)
{
    int i;
    struct sigaction dfl;       /* the handler object */

    dfl.sa_handler = SIG_DFL;   /* for resetting to default behavior */

    /* Loop through all registered signals and either set to
     * the new handler or reset them back to the default */
    i = 0;
    while (sigpairs[i].signal != 0)
    {
        /* notice that the second parameter takes the address of the handler */
        if ( sigaction(sigpairs[i].signal, (on ? &sigpairs[i].action : &dfl), NULL) < 0 )
        {
            return false;
        }
        ++i;
    }

    return true;
}

#include "logging-epilogue.h"    /* our logging support */
