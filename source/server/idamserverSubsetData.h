//--------------------------------------------------------------------------------------------------------------------
// Serverside Data Subsetting Operations Data
//
// Return Codes:	0 => OK, otherwise Error
//
//--------------------------------------------------------------------------------------------------------------------

#ifndef IDAM_SERVER_IDAMSERVERSUBSETDATA_H
#define IDAM_SERVER_IDAMSERVERSUBSETDATA_H

#include <clientserver/parseXML.h>
#include <clientserver/idamStructs.h>

int idamserverSubsetData(DATA_BLOCK *data_block, ACTION action);
int idamserversubsetindices(char *operation, DIMS *dim, double value, unsigned int *subsetindices);
int idamserverParseServerSide(REQUEST_BLOCK *request_block, ACTIONS *actions_serverside);
int idamserverNewDataArray2(DIMS *dims, int rank, int dimid,
                            char *data, int ndata, int data_type, int notoperation, int reverse,
                            int start, int end, int start1, int end1, int *n, void **newdata);

#endif // IDAM_SERVER_IDAMSERVERSUBSETDATA_H
