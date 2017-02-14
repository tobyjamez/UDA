#ifndef UDA_SERVER_IDAMSERVER_H
#define UDA_SERVER_IDAMSERVER_H

#define MAXOPENFILEDESC 50  // Maximum number of Open File Descriptors

#define MAXMAPDEPTH     10  // Maximum number of chained signal name mappings (Recursive depth)
#define MAXREQDEPTH     4   // Maximum number of Device Name to Server Protocol and Host substitutions

#define XDEBUG          0   // Socket Streams

#include <server/idamPluginStructs.h>
#include <clientserver/socketStructs.h>
#include <structures/genStructs.h>

IDAMERRORSTACK* getIdamServerPluginErrorStack();
USERDEFINEDTYPELIST* getIdamServerUserDefinedTypeList();
extern void copyIdamServerEnvironment(ENVIRONMENT* environ);
void putIdamServerEnvironment(ENVIRONMENT environ);
LOGMALLOCLIST* getIdamServerLogMallocList();
USERDEFINEDTYPELIST* getIdamServerParsedUserDefinedTypeList();

#ifndef FATCLIENT
int idamServer(int argc, char** argv);
#else
int idamServer(CLIENT_BLOCK client_block, REQUEST_BLOCK * request_block0, SERVER_BLOCK * server_block0,
                          DATA_BLOCK * data_block0);
#endif

//--------------------------------------------------------------
// Static Global variables

extern SOCKETLIST server_socketlist;    // List of Data Server Sockets
extern PLUGINLIST pluginList;

extern unsigned int totalDataBlockSize;
extern int serverVersion;
extern int altRank;
extern unsigned int lastMallocIndex;
extern unsigned int* lastMallocIndexValue;

extern IDAMERRORSTACK idamerrorstack;

extern XDR* serverInput;
extern XDR* serverOutput;
extern int server_tot_block_time;
extern int server_timeout;

extern unsigned int XDRstdioFlag;

extern USERDEFINEDTYPELIST parseduserdefinedtypelist;

#endif // UDA_SERVER_IDAMSERVER_H