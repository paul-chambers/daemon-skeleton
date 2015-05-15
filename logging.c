#define  _GNU_SOURCE  /* dladdr is a gcc extension */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <dlfcn.h>

#include "logging.h"

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

gLogEntry gLog[kMaxLogScope];


/* use a function pointer to handle the logging destination */
typedef void (*fpLogTo)(unsigned int priority, const char *msg);

fpLogTo  gLogString;

unsigned int    gLogDestination = kLogToUndefined;
unsigned int    gLogLevel = 0;
const char *    gLogName = "";
FILE *          gLogFile;

void *          gDLhandle = NULL;
int             gFunctionTraceEnabled = 0;
int             gCallDepth = 1;

static char *leader = "..........................................................................................";

/* dynamically built by the Makefile */
#include "obj/logscopedefs.inc"


/********** DO NOT INSTRUMENT THE INSTRUMENTATION! **********/

void initLogging( const char *name )
                            __attribute__((no_instrument_function));

void _log(unsigned int priority, const char *format, ...)
                            __attribute__((no_instrument_function));

void _logWithLocation(const char *inFile, unsigned int atLine, unsigned int priority, const char *format, ...)
                            __attribute__((no_instrument_function));

void _logToTheVoid( unsigned int priority, const char *msg )
                            __attribute__((no_instrument_function));
void _logToSyslog(  unsigned int priority, const char *msg )
                            __attribute__((no_instrument_function));
void _logToFile(    unsigned int priority, const char *msg )
                            __attribute__((no_instrument_function));
void _logToStderr(  unsigned int priority, const char *msg )
                            __attribute__((no_instrument_function));

void _logString(unsigned int priority, const char *format)
                            __attribute__((no_instrument_function));

void _profileHelper(void *left, const char *middle, void *right)
                            __attribute__((no_instrument_function));

const char *addrToString(void *addr, char *scratch)
                            __attribute__((no_instrument_function));

void __cyg_profile_func_enter(void *this_fn, void *call_site)
                            __attribute__((no_instrument_function));

void __cyg_profile_func_exit(void *this_fn, void *call_site)
                            __attribute__((no_instrument_function));

/********** DO NOT INSTRUMENT THE INSTRUMENTATION! **********/


void initLogging( const char *name )
{
    int i;

    gLogName = name;

    // initialize globals to something safe until startLogging has been invoked
    gLogDestination = kLogToUndefined;
    gLogLevel       = kLogDebug;
    gLogFile        = stderr;
    gLogString      = &_logToStderr;

    gDLhandle       = dlopen(NULL, RTLD_LAZY);

    // dynamically defined in logscopedefs.inc by Makefile
    logLogInit();

   	for ( i = 0; i < kMaxLogScope; ++i )
   	{
        gLog[i].level = kLogDebug;
        if ( gLog[i].site == NULL )
        {
            logCritical("### Failed to allocate memory for logging - exiting\n");
            exit(ENOMEM); // fatal
        }

        logDebug("%s scope has %u log statements", logScopeNames[i], gLog[i].max);
    }

    if (gDLhandle != NULL)
    {
        logFunctionTraceOn();
    }
}

void startLogging( unsigned int debugLevel, eLogDestination logDest, const char * logFile )
{
    gLogLevel = debugLevel;

    if (logDest != gLogDestination)
    {
        stopLogging();

        switch (logDest)
        {
        case kLogToSyslog:
            setlogmask( LOG_UPTO (LOG_DEBUG) );
            openlog( gLogName, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

            gLogString = &_logToSyslog;
            break;

        case kLogToFile:
            gLogString = &_logToStderr;

            if (logFile != NULL)
            {
                gLogFile = fopen( logFile, "a" );

                if (gLogFile != NULL)
                {
                    gLogString = &_logToFile;
                }
                else
                {
                    logDest = kLogToStderr;
                    logError("Unable to log to \"%s\" (%s [%d]), redirecting to stderr", logFile, strerror(errno), errno);
                }
            }
            break;

        case kLogToStderr:
            gLogString = &_logToStderr;
            break;

        default:
            gLogString = &_logToTheVoid;
            break;
        }
        gLogDestination = logDest;
    }
}

void stopLogging( void )
{
    switch (gLogDestination)
    {
    case kLogToSyslog:
        closelog();
        break;

    case kLogToFile:
        fclose( gLogFile );
        gLogFile = stderr;
        break;

        // don't do anything for the other cases
    default:
        break;
    }
    //gLogString = &_logToTheVoid;
    gLogDestination = kLogToUndefined; // just in case startLogging is called again
}

void _logToTheVoid(unsigned int UNUSED(priority), const char * UNUSED(msg))  { /* just return */ }

void _logToSyslog(unsigned int priority, const char *msg)   { syslog( priority, msg ); }

void _logToFile(unsigned int UNUSED(priority), const char *msg)     { fprintf(gLogFile, "%s\n", msg); }

void _logToStderr(unsigned int UNUSED(priority), const char *msg)   { fprintf(stderr, "%s\n", msg); }


void _log(unsigned int priority, const char *format, ...)
{
    va_list vaptr;
    char    msg[256];

    va_start(vaptr, format);

    vsnprintf( msg, sizeof(msg), format, vaptr );

    gLogString(priority, msg);

    va_end(vaptr);
}

void _logWithLocation(const char *inFile, unsigned int atLine, unsigned int priority, const char *format, ...)
{
    va_list vaptr;
    char    msg[256];
    int     prefixLen, remaining;

    va_start(vaptr, format);

    prefixLen = vsnprintf( msg, sizeof(msg), format, vaptr );

    remaining = sizeof(msg) - prefixLen - 1;

    if ( remaining > 10 )
    {
        snprintf( &msg[prefixLen], remaining, " (%s:%d)", inFile, atLine );
    }

    gLogString(priority, msg);

    va_end(vaptr);
}

const char *addrToString(void *addr, char *scratch)
{
    const char   *str;
    Dl_info info;

    str = NULL;
    if (gDLhandle != NULL )
    {
        dladdr(addr, &info);
        str = info.dli_sname;
    }
    if (str == NULL)
    {
        sprintf( scratch, "0x%08lx", (unsigned long)addr);
        str = scratch;
    }
    return str;
}

/*
    gcc inseerts calls to these functions at the beginning and end of every
    compiled function when the -finstrument-functions option is used.

    If just left on, this generates so much logging that it's rarely useful.
    Please use logFunctionTraceOn and logFunctionTraceOff around the
    code/situation you care about.
*/

void _profileHelper(void *left, const char *middle, void *right)
{
    // some scratch space, in case addrToString needs it. avoids needless malloc churn
    char leftScratch[16];
    char rightScratch[16];
    char msg[256];

    if (gFunctionTraceEnabled && gLogDestination != kLogToUndefined)
    {
        snprintf(msg, sizeof(msg), "%.*s %s() %s %s()", gCallDepth, leader, addrToString(left, leftScratch), middle, addrToString(right, rightScratch));
        gLogString(kLogDebug, msg);
    }
}

/* just landed in a function */
void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
    _profileHelper( call_site, "called", this_fn );
    ++gCallDepth;
}

/* about to leave a function */
void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
    --gCallDepth;
    if (gCallDepth < 1) gCallDepth = 1;
    _profileHelper( this_fn, "returned to", call_site );
}


#include "logging-epilogue.h"
