//! $LastChangedRevision: 353 $
//! $LastChangedDate: 2013-11-18 15:32:28 +0000 (Mon, 18 Nov 2013) $
//! $LastChangedBy: dgm $
//! $HeadURL: https://fussvn.fusion.culham.ukaea.org.uk/svnroot/IDAM/development/source/clientserver/freeDataBlock.c $

// Free Heap Memory
//
// Change History:
//
// 13Dec2006	dgm	errparams now fixed length array rather than heap
// 29Sep2008	dgm	free Opaque Data structures added
// 04Nov2008	dgm	reset opaque_count on free
// 11Feb2010	dgm	Free Generalised Structure Data
// 01Mar2010	dgm	created freeReducedDataBlock to manage heap for fatclients accessing generalised data structures
// 28Apr2010	dgm	OPAQUE_TYPE_XDRFILE added
// 18Nov2013	dgm	PUTDATA functionality included as standard rather than with a compiler option
// 10Oct2014	dgm	OPAQUE_TYPE_XDROBJECT added
//-----------------------------------------------------------------------------

#include "freeDataBlock.h"

#include <stdlib.h>
#include <logging/idamLog.h>
#include "initStructs.h"
#include <structures/struct.h>

#ifdef SERVERBUILD
#  include <include/idamserver.h>
#  include <include/idamgenstructprivate.h>
#  include <server/idamServerStartup.h>
#else
#  include <include/idamclient.h>
#endif

// It is assumed that data pointers within each putDataBlock are private to the client application
// and must be freed by the application.  

void freeIdamClientPutDataBlockList(PUTDATA_BLOCK_LIST* putDataBlockList)
{
    if (putDataBlockList->putDataBlock != NULL && putDataBlockList->blockListSize > 0)
        free(putDataBlockList->putDataBlock);
    initIdamPutDataBlockList(putDataBlockList);
}


void freeIdamDataBlock(DATA_BLOCK* data_block)
{
    freeDataBlock(data_block);
}

void freeDataBlock(DATA_BLOCK* data_block)
{

    // Free Heap Memory & Zero all Integer values

    void* cptr;
    DIMS* ddims;
    unsigned int i;
    unsigned int rank;

    idamLog(LOG_DEBUG, "freeDataBlock: Enter\n");

    if (data_block != NULL) {

        idamLog(LOG_DEBUG, "freeDataBlock: Opaque Data\n");

        switch (data_block->opaque_type) {
            case (OPAQUE_TYPE_XML_DOCUMENT): {
                if (data_block->opaque_block != NULL) free(data_block->opaque_block);
                data_block->opaque_count = 0;
                data_block->opaque_block = NULL;
                break;
            }

            case (OPAQUE_TYPE_STRUCTURES): {
#ifdef GENERALSTRUCTS
                if (data_block->opaque_block != NULL) {

                    if (userdefinedtypelist != NULL) {
                        freeUserDefinedTypeList(userdefinedtypelist);
                        free(userdefinedtypelist);
                        userdefinedtypelist = NULL;
                    }

                    if (logmalloclist != NULL) {
                        freeMallocLogList(logmalloclist);
                        free(logmalloclist);
                        logmalloclist = NULL;
                    }

                    data_block->opaque_count = 0;
                    data_block->opaque_block = NULL;
                    data_block->data_type = TYPE_UNKNOWN;
                    data_block->opaque_type = OPAQUE_TYPE_UNKNOWN;

                    data_block->data = NULL;        // Added to Malloc Log List for freeing
                }
#endif
                break;
            }


            case (OPAQUE_TYPE_XDRFILE): {
#ifdef GENERALSTRUCTS
                if (data_block->opaque_block != NULL) {
                    free(data_block->opaque_block);
                }
                data_block->opaque_count = 0;
                data_block->opaque_block = NULL;
                data_block->data_type = TYPE_UNKNOWN;
                data_block->opaque_type = OPAQUE_TYPE_UNKNOWN;
                data_block->data = NULL;
#endif
                break;
            }
            case (OPAQUE_TYPE_XDROBJECT): {
#ifdef GENERALSTRUCTS
                if (data_block->opaque_block != NULL) {
                    free(data_block->opaque_block);
                }
                data_block->opaque_count = 0;
                data_block->opaque_block = NULL;
                data_block->data_type = TYPE_UNKNOWN;
                data_block->opaque_type = OPAQUE_TYPE_UNKNOWN;
                data_block->data = NULL;
#endif
                break;
            }

            default:
                break;
        }

        idamLog(LOG_DEBUG, "freeDataBlock: freeing Data\n");

        rank = data_block->rank;
        ddims = data_block->dims;

        if ((cptr = (void*) data_block->data) != NULL) free(cptr);
        if ((cptr = (void*) data_block->errhi) != NULL) free(cptr);
        if ((cptr = (void*) data_block->errlo) != NULL) free(cptr);

        data_block->data = NULL;
        data_block->errhi = NULL;
        data_block->errlo = NULL;

        idamLog(LOG_DEBUG, "freeDataBlock: freeing Dimensions - Rank = %d \n", rank);
        idamLog(LOG_DEBUG, "freeDataBlock: Dim Structure Location %p \n", ddims);

        if (ddims != NULL) {
            for (i = 0; i < rank; i++) {

                idamLog(LOG_DEBUG, "freeDataBlock: Dimension[%d] \n", i);
                idamLog(LOG_DEBUG, "freeDataBlock: Dimension Data \n");

                if ((cptr = (void*) ddims[i].dim) != NULL) free(cptr);

                idamLog(LOG_DEBUG, "freeDataBlock: Dimension Error Hi \n");

                if ((cptr = (void*) ddims[i].errhi) != NULL) free(cptr);

                idamLog(LOG_DEBUG, "freeDataBlock: Dimension Error Lo \n");

                if ((cptr = (void*) ddims[i].errlo) != NULL) free(cptr);

                idamLog(LOG_DEBUG, "freeDataBlock: Dimension Sams \n");

                if ((cptr = (void*) ddims[i].sams) != NULL) free(cptr);

                idamLog(LOG_DEBUG, "freeDataBlock: Dimension offs \n");

                if ((cptr = (void*) ddims[i].offs) != NULL) free(cptr);

                idamLog(LOG_DEBUG, "freeDataBlock: Dimension ints \n");

                if ((cptr = (void*) ddims[i].ints) != NULL) free(cptr);

                data_block->dims[i].dim = NULL;
                data_block->dims[i].errhi = NULL;
                data_block->dims[i].errlo = NULL;
                data_block->dims[i].sams = NULL;
                data_block->dims[i].offs = NULL;
                data_block->dims[i].ints = NULL;
            }

            idamLog(LOG_DEBUG, "freeDataBlock: Dimension Array \n");

            free(ddims);
            data_block->dims = NULL;
        }

        data_block->handle = 0;
        data_block->errcode = 0;
        data_block->rank = 0;
        data_block->order = 0;
        data_block->data_type = TYPE_UNKNOWN;
        data_block->error_type = TYPE_UNKNOWN;
        data_block->data_n = 0;
        data_block->error_param_n = 0;

    }

    idamLog(LOG_DEBUG, "freeDataBlock: Exit\n");
}

void freeReducedDataBlock(DATA_BLOCK* data_block)
{

// Free Heap Memory & Zero all Integer values

#if !defined(GENERALSTRUCTS) || !defined(FATCLIENT)
    if (data_block == NULL) return;
    return;
#else
    if(data_block == NULL) return;
    if(data_block->opaque_type != OPAQUE_TYPE_STRUCTURES) return;
    if(data_block->opaque_block == NULL) return;

    if(userdefinedtypelist != NULL) {
        freeUserDefinedTypeList(userdefinedtypelist);
        free((void *)userdefinedtypelist);
        userdefinedtypelist = NULL;
    }

    if(logmalloclist != NULL) {
        freeMallocLogList(logmalloclist);
        free((void *)logmalloclist);
        logmalloclist = NULL;
    }

    data_block->opaque_count = 0;
    data_block->opaque_block = NULL;
    data_block->data_type    = TYPE_UNKNOWN;
    data_block->opaque_type  = OPAQUE_TYPE_UNKNOWN;

    data_block->data         = NULL;		// Added to Malloc Log List for freeing
    return;
#endif
}

