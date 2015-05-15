/*
    things to do at the end of each source file to support logging
*/

/* this defines a 'maximum' value for the site ID for each scope, of the form gLogMax<scope> */
#define logDSMhelper(scope, counter) unsigned int gLogMax_##scope = counter
#define logDefineScopeMaximum( scope, counter)  logDSMhelper( scope, counter )

logDefineScopeMaximum( LOG_SCOPE , __COUNTER__ );
