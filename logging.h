/*
    logging macros, for my personal sanity...
*/

#ifndef LOGGING_H
#define LOGGING_H

#include    <syslog.h>

/* this is dynamically built by the Makefile */
#include "obj/logscopes.inc"

extern unsigned int     gLogLevel;
extern int              gFunctionTraceEnabled;

typedef struct {
    unsigned int    level;
    unsigned int    max;
    unsigned char  *site;
} gLogEntry;

extern gLogEntry gLog[kMaxLogScope];

#define kLogEmergency   LOG_EMERG
#define kLogAlert       LOG_ALERT
#define kLogCritical    LOG_CRIT
#define kLogError       LOG_ERR
#define kLogWarning     LOG_WARNING
#define kLogNotice      LOG_NOTICE
#define kLogInfo        LOG_INFO
#define kLogDebug       LOG_DEBUG

typedef enum { kLogToUndefined, kLogToSyslog, kLogToFile, kLogToStderr } eLogDestination;

/* set up the logging mechanisms. Call once, very early. */
void    initLogging( const char *name );

/* configure the logging mechanisms, may be called multiple times */
void    startLogging( unsigned int debugLevel, eLogDestination logDest, const char *logFile );

/* tidy up the current logging mechanism */
void    stopLogging( void );

/* start logging function entry & exit */
static inline void logFunctionTraceOn() { gFunctionTraceEnabled = 1; };

/* stop logging function entry & exit */
static inline void logFunctionTraceOff() { gFunctionTraceEnabled = 0; };

/* private helpers, used by preprocessor macros. Please don't use directly! */
void    _log( unsigned int priority, const char *format, ...)
            __attribute__((__format__ (__printf__, 2, 3))) __attribute__((no_instrument_function));
void    _logWithLocation( const char *inFile, unsigned int atLine, unsigned int priority, const char *format, ...)
            __attribute__((__format__ (__printf__, 4, 5))) __attribute__((no_instrument_function));

#define logEmergency(...)   logWithLocation(kLogEmergency,  __VA_ARGS__ )
#define logAlert(...)       logWithLocation(kLogAlert,      __VA_ARGS__ )
#define logCritical(...)    logWithLocation(kLogCritical,   __VA_ARGS__ )
#define logError(...)       logWithLocation(kLogError,      __VA_ARGS__ )
#define logWarning(...)     log(kLogWarning,    __VA_ARGS__ )
#define logNotice(...)      log(kLogNotice,     __VA_ARGS__ )
#define logInfo(...)        log(kLogInfo,       __VA_ARGS__ )

#ifndef RELEASE_BUILD
# define logDebug(...)      logWithLocation(kLogDebug,      __VA_ARGS__ )
# define logCheckpoint()    do { _log( __FILE__, __LINE__, kLogDebug, "reached" ); } while (0)
#else
# define logDebug(...)      do {} while (0)
# define logCheckpoint()    do {} while (0)
#endif

#define logCheck_expand_again(priority, scope, id)  ( gLogLevel >= priority && gLog[kLog_##scope].max > id && gLog[kLog_##scope].level >= priority && gLog[kLog_##scope].site[id] == 0 )
#define logCheck(priority, scope, id)       logCheck_expand_again(priority, scope, id)

#define log(priority, ...)              do { if (  logCheck( priority, LOG_SCOPE, __COUNTER__ ) ) _log( priority, __VA_ARGS__ ); } while (0)
#define logWithLocation(priority, ...)  do { if (  logCheck( priority, LOG_SCOPE, __COUNTER__ ) ) _logWithLocation( __FILE__, __LINE__, priority, __VA_ARGS__ ); } while (0)

#endif

