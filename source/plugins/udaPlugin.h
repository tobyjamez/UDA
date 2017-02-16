#ifndef IDAM_IDAMPLUGIN_H
#define IDAM_IDAMPLUGIN_H

#include <clientserver/udaStructs.h>
#include <logging/logging.h>
#include <clientserver/errorLog.h>
#include <server/pluginStructs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAXFUNCTIONNAME     256

// plugin State

#define PLUGINNOTEXTERNAL       0
#define PLUGINEXTERNAL          1           // The plugin resides in an external shared library
#define PLUGINNOTOPERATIONAL    0
#define PLUGINOPERATIONAL       1

// privacy

#define PLUGINPRIVATE       1           // Only internal users can use the service (access the data!)
#define PLUGINPUBLIC        0           // All users - internal and external - can use the service

// List of available data reader plugins

#define IDAM_NETRC          ".netrc"
#define IDAM_PROXYHOST      "proxypac"
#define IDAM_PROXYPORT      "8080"
#define IDAM_PROXYPAC       "fproxy.pac"
#define IDAM_PROXYUSER      "nobody"
#define IDAM_PROXYPROTOCOL  "http"

// SQL Connection Types

#define PLUGINSQLNOTKNOWN   	0
#define PLUGINSQLPOSTGRES   	1
#define PLUGINSQLMYSQL      	2
#define PLUGINSQLMONGODB	3

extern unsigned short pluginClass;

enum pluginClass {
    PLUGINUNKNOWN,
    PLUGINFILE,         // File format access
    PLUGINSERVER,       // Server protocol access
    PLUGINFUNCTION,     // Server-side function transformation
    PLUGINDEVICE,       // Server to Server chaining, i.e. Pass the request to an external server
    PLUGINOTHER
};

typedef void (* ADDIDAMERRORFUNP)(IDAMERRORSTACK*, int, char*, int, char*);   // Write to the Error Log

// Prototypes

unsigned short findStringValue(NAMEVALUELIST* namevaluelist, char** value, const char* name);

unsigned short findValue(NAMEVALUELIST* namevaluelist, const char* name);

unsigned short findIntValue(NAMEVALUELIST* namevaluelist, int* value, const char* name);

unsigned short findShortValue(NAMEVALUELIST* namevaluelist, short* value, const char* name);

unsigned short findFloatValue(NAMEVALUELIST* namevaluelist, float* values, const char* name);

unsigned short findIntArray(NAMEVALUELIST* namevaluelist, int** values, size_t* nvalues, const char* name);

unsigned short findFloatArray(NAMEVALUELIST* namevaluelist, float** values, size_t* nvalues, const char* name);

int callPlugin(PLUGINLIST* pluginlist, const char* request, const IDAM_PLUGIN_INTERFACE* old_plugin_interface);

#define QUOTE_(X) #X
#define QUOTE(X) QUOTE_(X)
#define CONCAT_(X, Y) X##Y
#define CONCAT(X, Y) CONCAT_(X, Y)
#define UNIQUE_VAR(NAME) __func__##NAME##__

#define RAISE_PLUGIN_ERROR(MSG) \
{ int UNIQUE_VAR(err) = 999; \
IDAM_LOGF(LOG_ERROR, "%s\n", MSG); \
addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, UNIQUE_VAR(err), MSG); \
return UNIQUE_VAR(err); }

#define RAISE_PLUGIN_ERROR_F(MSG, FMT, ...) \
{ int UNIQUE_VAR(err) = 999; \
IDAM_LOGF(LOG_ERROR, "%s\n", FMT, __VA_ARGS__); \
addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, UNIQUE_VAR(err), MSG); \
return UNIQUE_VAR(err); }

#define RAISE_PLUGIN_ERROR_EX(MSG, CODE) \
int UNIQUE_VAR(err) = 999; \
IDAM_LOGF(LOG_ERROR, "%s", MSG); \
addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, UNIQUE_VAR(err), MSG); \
CODE; \
return UNIQUE_VAR(err);

#define FIND_REQUIRED_VALUE(NAME_VALUE_LIST, VARIABLE, TYPE) \
if (!find##TYPE##Value(&NAME_VALUE_LIST, &VARIABLE, QUOTE(VARIABLE))) { \
    RAISE_PLUGIN_ERROR("Required argument '" QUOTE(VARIABLE) "' not given"); \
}

#define FIND_REQUIRED_ARRAY(NAME_VALUE_LIST, VARIABLE, TYPE) \
if (!find##TYPE##Array(&NAME_VALUE_LIST, &VARIABLE, CONCAT(&n, VARIABLE), QUOTE(VARIABLE))) { \
    RAISE_PLUGIN_ERROR("Required argument '" QUOTE(VARIABLE) "' not given"); \
}

#define FIND_REQUIRED_INT_VALUE(NAME_VALUE_LIST, VARIABLE)      FIND_REQUIRED_VALUE(NAME_VALUE_LIST, VARIABLE, Int)
#define FIND_REQUIRED_FLOAT_VALUE(NAME_VALUE_LIST, VARIABLE)    FIND_REQUIRED_VALUE(NAME_VALUE_LIST, VARIABLE, Float)
#define FIND_REQUIRED_STRING_VALUE(NAME_VALUE_LIST, VARIABLE)   FIND_REQUIRED_VALUE(NAME_VALUE_LIST, VARIABLE, String)

#define FIND_REQUIRED_INT_ARRAY(NAME_VALUE_LIST, VARIABLE)      FIND_REQUIRED_ARRAY(NAME_VALUE_LIST, VARIABLE, Int)
#define FIND_REQUIRED_FLOAT_ARRAY(NAME_VALUE_LIST, VARIABLE)    FIND_REQUIRED_ARRAY(NAME_VALUE_LIST, VARIABLE, Float)

#define FIND_INT_VALUE(NAME_VALUE_LIST, VARIABLE)       findIntValue(&NAME_VALUE_LIST, &VARIABLE, QUOTE(VARIABLE))
#define FIND_SHORT_VALUE(NAME_VALUE_LIST, VARIABLE)     findShortValue(&NAME_VALUE_LIST, &VARIABLE, QUOTE(VARIABLE))
#define FIND_FLOAT_VALUE(NAME_VALUE_LIST, VARIABLE)     findFloatValue(&NAME_VALUE_LIST, &VARIABLE, QUOTE(VARIABLE))
#define FIND_STRING_VALUE(NAME_VALUE_LIST, VARIABLE)    findStringValue(&NAME_VALUE_LIST, &VARIABLE, QUOTE(VARIABLE))

#define FIND_INT_ARRAY(NAME_VALUE_LIST, VARIABLE)       findIntArray(&NAME_VALUE_LIST, &VARIABLE, CONCAT(&n, VARIABLE), QUOTE(VARIABLE))
#define FIND_FLOAT_ARRAY(NAME_VALUE_LIST, VARIABLE)     findFloatArray(&NAME_VALUE_LIST, &VARIABLE, CONCAT(&n, VARIABLE), QUOTE(VARIABLE))

#define CALL_PLUGIN(PLUGIN_INTERFACE, FMT, ...) \
{ char UNIQUE_VAR(request)[1024]; \
snprintf(UNIQUE_VAR(request), FMT, __VA_ARGS__); \
UNIQUE_VAR(request)[1023] = '\0'; \
int UNIQUE_VAR(err) = callPlugin(PLUGIN_INTERFACE->pluginList, UNIQUE_VAR(request), PLUGIN_INTERFACE); \
if (UNIQUE_VAR(err)) { \
    RAISE_PLUGIN_ERROR("Plugin call failed"); \
} }

#ifdef __cplusplus
}
#endif

#endif // IDAM_IDAMPLUGIN_H