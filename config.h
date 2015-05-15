
#ifndef config_h
#define config_h

typedef struct {
    int     foreground;     /* if non-zero, don't daemonize (for debug or systemd purposes) */
    int     debugLevel;     /* controls the amount of logging (syslog priority) */
    char *  configFile;     /* config file path, or NULL for default search */
    char *  logFile;        /* file destination for logs, or NULL if the user didn't supply one */

} kConfigurationOptions;

kConfigurationOptions * parseConfiguration(int argc, const char *argv[] );

#endif
