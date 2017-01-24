
#ifndef IDAM_MAKESERVERREQUESTBLOCK_H
#define IDAM_MAKESERVERREQUESTBLOCK_H

#include "idamclientserverpublic.h"
#include "idamclientserverprivate.h"
#include "idamserver.h"
#include "idamplugin.h"

#if defined(SERVERBUILD) || defined(FATCLIENT)

void extractFunctionName(char *str, REQUEST_BLOCK *request_block);
void initServerRequestBlock(REQUEST_BLOCK *str);
int makeServerRequestBlock(REQUEST_BLOCK *request_block, PLUGINLIST pluginList);
int sourceFileFormatTest(const char *source, REQUEST_BLOCK *request_block, PLUGINLIST pluginList);
int genericRequestTest(const char *source, REQUEST_BLOCK *request_block, PLUGINLIST pluginList);
int extractArchive(REQUEST_BLOCK *request_block, int reduceSignal);

#endif

void expandEnvironmentVariables(REQUEST_BLOCK *request_block);
int extractSubset(REQUEST_BLOCK *request_block);
void freeNameValueList(NAMEVALUELIST *nameValueList);
void parseNameValue(char *pair, NAMEVALUE *nameValue,unsigned short strip);
int nameValuePairs(char *pairList, NAMEVALUELIST *nameValueList, unsigned short strip);

#endif // IDAM_MAKESERVERREQUESTBLOCK_H
