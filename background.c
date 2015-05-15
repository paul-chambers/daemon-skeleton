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

#include "background.h"

#include "logging.h"    /* our logging support */

/*
    This is the 'main' for the background processing
 */
int background(void)
{

    /* Block and wait for signals.
     *
     * An alternate, synchronous strategy to SIGCHLD could be deployed here,
     * which is to call waitpid(..., WNOHANG) in a loop
     */
    while (1)
    {
        logInfo("zzzz...");
        logError(":: yawn ::");
        sleep(2);
    };

    return 0;
}

#include "logging-epilogue.h"
