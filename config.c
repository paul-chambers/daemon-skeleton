#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>
#include <popt.h>       /* popt library for parsing config files and command line options */

#include "common.h"
#include "config.h"

#include "logging.h"

/* static data */

static kConfigurationOptions  configurationOptions = {
    0,
    kLogDebug,
    NULL,
    NULL
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/* popt grammar for parsing command line options */
static struct poptOption commandLineVocab[] =
{
    /* longName, shortName, argInfo, arg, val, autohelp, autohelp arg */
    { "foreground", 'f',  POPT_ARG_VAL,    &configurationOptions.foreground, 1, "run in Foreground (don't daemonize)" },
    { "daemon",     '\0', POPT_ARG_VAL,    &configurationOptions.foreground, 0, "run as a Daemon (in the background)" },
    { "debug",      'd',  POPT_ARG_INT,    &configurationOptions.debugLevel, 0, "set the amount of logging (i.e. syslog priority)" },
    { "config",     'c',  POPT_ARG_STRING, &configurationOptions.configFile, 0, "read Configuration from <file>", "path to file" },
    { "logfile",    'l',  POPT_ARG_STRING, &configurationOptions.logFile,    0, "send logging to <file>", "path to file" },
    POPT_AUTOHELP
    POPT_TABLEEND
};

/* popt grammar used when parsing configuration files */
static struct poptOption configFileVocab[] =
{
    /* longName, shortName, argInfo, arg, val, autohelp, autohelp arg */
    { "foreground", '\0',  POPT_ARG_VAL,    &configurationOptions.foreground, 1, "run in Foreground (don't daemonize)" },
    { "daemon",     '\0',  POPT_ARG_VAL,    &configurationOptions.foreground, 0, "run as a Daemon (in the background)" },
    { "debug",      '\0',  POPT_ARG_INT,    &configurationOptions.debugLevel, 0, "set the amount of logging (i.e. syslog priority)" },
    { "config",     '\0',  POPT_ARG_STRING, &configurationOptions.configFile, 0, "read Configuration from <file>", "path to file" },
    { "logfile",    '\0',  POPT_ARG_STRING, &configurationOptions.logFile,    0, "send logging to <file>", "path to file" },
    POPT_TABLEEND
};

#pragma GCC diagnostic pop

int fileIsReadable( const char * file, int errIfMissing )
{
    int notReadable;

    notReadable = (access(file, R_OK) != 0);

    if ( notReadable )
    { /* don't have access - figure out why */
        switch (errno)
        {
        case ENOENT: /* nothing there - may not be an error */
            if (errIfMissing)
            {
                logError( "Cannot find config file \"%s\"", file );
            }
            break;

        default:
            logError( "Cannot read config file \"%s\" (%s [%d])", file, strerror(errno), errno );
            break;
        }
    }

    return !notReadable;
}

#define ARGV_SIZE   100

int parseConfigFile(const char * configFile)
{
    int             result;
    int             i;
    FILE           *confFD;
    char           *key, *value, *saved;
    char            line[1024];
    int             argc;
    char           *argv[ARGV_SIZE];
    poptContext     options;

    result = 0;

    for ( i = 1; i < ARGV_SIZE && argv[i] != NULL; ++i )
    {
        argv[i] = NULL;
    }
    argv[0] = (char *)gExecName;
    argc = 1;

    confFD = fopen( configFile, "r" );
    if ( confFD == NULL )
    {
        logError("unable to open config file \"%s\" (%s [%d])", configFile, strerror(errno), errno);
        result = errno;
    }
    else
    {
        while ( fgets(line, sizeof(line), confFD) != NULL && !feof(confFD) )
        {
            /* split each line */
            key   = strtok_r(line,"= \t\n\r", &saved);
            value = strtok_r(NULL,"= \t\n\r", &saved);

#ifdef TACHYON
            logDebug("key: %s, value: %s", key != NULL? key : "[none]", value != NULL? value : "[none]");
#endif

            /* add to argv */
            if (key != NULL)
            {
                argv[argc] = malloc(strlen(key + 2 + 1));
                if (argv[argc] != NULL)
                {
                    strcpy(argv[argc],"--");
                    strcat(argv[argc],key);
                    ++argc;
                }
            }
            if (value != NULL)
            {
                argv[argc] = strdup(value);
                if (argv[argc] != NULL)
                {
                    ++argc;
                }
            }
        }
        result = ferror(confFD);

#ifdef TACHYON
        /* debugging */
        for ( i = 0; i < argc; ++i )
        {
            logDebug("%d: \"%s\"", i, argv[i] != NULL? argv[i] : "<null>");
        }
#endif
    }

    if ( result == 0 )
    {
        options = poptGetContext( gExecName, argc, (const char **)argv, configFileVocab, 0 );

        if (options != NULL)
        {
            // enable any popt aliases
            poptReadDefaultConfig(options, 0);

            result = poptGetNextOpt(options);
            if (result < -1)
            {
                logError("problem in config file \"%s\" with option \"%s\" (%s)",
                    configFile, poptBadOption(options, POPT_BADOPTION_NOALIAS), poptStrerror(result));
            }

            poptFreeContext(options);
        }
    }

    /* free any strings allocated (don't free argv[0])*/
    for ( i = 1; i < argc && argv[i] != NULL; ++i)
    {
        free((void *)argv[i]);
    }

    return result;
}

int parseCmdLineOptions( int argc, const char *argv[] )
{
    poptContext options;
    int result;

    options = poptGetContext( argv[0], argc, argv, commandLineVocab, 0 );

    // enable any popt aliases
    poptReadDefaultConfig(options, 0);

    result = poptGetNextOpt(options);
    if (result < -1)
    {
        logError("problem with the command line option \"%s\" (%s)",
            poptBadOption(options, POPT_BADOPTION_NOALIAS), poptStrerror(result));
    }

    poptFreeContext(options);

    return 0;
}

kConfigurationOptions * parseConfiguration(int argc, const char *argv[])
{
    /* first time through, we really only care if there's a config file specified */
    parseCmdLineOptions( argc, argv );

    if (configurationOptions.configFile != NULL)
    { /* user explicitly provided a configuration file */
        if (fileIsReadable(configurationOptions.configFile, 1))
        {
            parseConfigFile(configurationOptions.configFile);
        }
    }
    else
    { /* user didn't provide a config file, so look in the standard place for a config file */
        if (fileIsReadable("/etc/toggled.conf", 0))
        {
            parseConfigFile("/etc/toggled.conf");
        }
    }

    /* command line options override config file, so parse them again */
    parseCmdLineOptions( argc, argv );

    return &configurationOptions;
}

#include "logging-epilogue.h"    /* our logging support */
