/*---------------------------------------------------------------
* IDAM Plugin data Reader to Access DATA from netCDF4 Files
*
* Input Arguments:	DATA_SOURCE data_source
*			SIGNAL_DESC signal_desc
*
* Returns:		readCDF		0 if read was successful
*					otherwise a Error Code is returned
*			DATA_BLOCK	Structure with Data from the IDA File
*
* Calls		freeDataBlock	to free Heap memory if an Error Occurs
*
* Notes: 	All memory required to hold data is allocated dynamically
*		in heap storage. Pointers to these areas of memory are held
*		by the passed DATA_BLOCK structure. Local memory allocations
*		are freed on exit. However, the blocks reserved for data are
*		not and MUST BE FREED by the calling routine.
*
* ToDo:
*
*		TRANSP data has coordinate dimensions that are of rank > 1: They are time dependent!
*		Ensure the netCDF3 plugin functionality is enabled.
*
*-----------------------------------------------------------------------------*/
#include "readCDFAtts.hpp"

#include <cstdlib>

#include <clientserver/errorLog.h>
#include <clientserver/stringUtils.h>
#include <clientserver/udaErrors.h>

#include "readCDF4.hpp"

// Read Standard Variable Attributes

int readCDF4Atts(int grpid, int varid, char* units, char* title, char* cls, char* comment)
{
    int err, i, rc, numatts;
    size_t attlength;
    char attname[MAX_NC_NAME];    // attribute name
    nc_type atttype;                    // attribute type

    char* txt = nullptr;

    units[0] = '\0';
    title[0] = '\0';
    cls[0] = '\0';
    comment[0] = '\0';

    //---------------------------------------------------------------------------------------------
    // Number of Attributes associated with this variable

    if ((rc = nc_inq_varnatts(grpid, varid, &numatts)) != NC_NOERR) {
        err = NETCDF_ERROR_INQUIRING_ATT_2;
        addIdamError(CODEERRORTYPE, "readCDFAtts", err, (char*)nc_strerror(rc));
        return err;
    }

    for (i = 0; i < numatts; i++) {
        if ((rc = nc_inq_attname(grpid, varid, i, attname)) != NC_NOERR) {
            err = NETCDF_ERROR_INQUIRING_ATT_7;
            addIdamError(CODEERRORTYPE, "readCDFAtts", err, (char*)nc_strerror(rc));
            return err;
        }

        if ((rc = nc_inq_atttype(grpid, varid, attname, &atttype)) != NC_NOERR) {
            err = NETCDF_ERROR_INQUIRING_ATT_8;
            addIdamError(CODEERRORTYPE, "readCDFAtts", err, (char*)nc_strerror(rc));
            return err;
        }

        if ((rc = nc_inq_attlen(grpid, varid, attname, &attlength)) != NC_NOERR) {
            err = NETCDF_ERROR_INQUIRING_ATT_9;
            addIdamError(CODEERRORTYPE, "readCDFAtts", err, (char*)nc_strerror(rc));
            return err;
        }

        if (atttype == NC_CHAR) {
            if ((txt = (char*)malloc((size_t)(attlength * sizeof(char) + 1))) == nullptr) {
                err = NETCDF_ERROR_ALLOCATING_HEAP_9;
                addIdamError(CODEERRORTYPE, "readCDFAtts", err, "Unable to Allocate Heap for Attribute Data");
                return err;
            }
            if ((rc = nc_get_att_text(grpid, varid, attname, txt)) != NC_NOERR) {
                err = NETCDF_ERROR_INQUIRING_ATT_10;
                addIdamError(CODEERRORTYPE, "readCDFAtts", err, (char*)nc_strerror(rc));
                free((void*)txt);
                return err;
            }
            txt[attlength * sizeof(char)] = '\0';        // Add the string Null terminator
            TrimString(txt);
            LeftTrimString(txt);

            if (STR_EQUALS(attname, "units")) {
                copyString(txt, units, STRING_LENGTH);
            } else if (STR_EQUALS(attname, "title") || STR_EQUALS(attname, "label") ||
                       STR_EQUALS(attname, "long_name")) {
                copyString(txt, title, STRING_LENGTH);
            } else if (STR_EQUALS(attname, "comment")) {
                copyString(txt, comment, STRING_LENGTH);
            } else if (STR_EQUALS(attname, "class")) {
                copyString(txt, cls, STRING_LENGTH);
            }
            free((void*)txt);
            txt = nullptr;
        } else if (atttype == NC_STRING) {
            char** sarr = (char**)malloc(attlength * sizeof(char*));
            if ((rc = nc_get_att_string(grpid, varid, attname, sarr)) != NC_NOERR) {
                err = 999;
                addIdamError(CODEERRORTYPE, "readCDF", err, (char*)nc_strerror(rc));
                free((void*)sarr);
                return err;
            }
            if (STR_EQUALS(attname, "units")) {
                copyString(sarr[0], units, STRING_LENGTH);
            } else if (STR_EQUALS(attname, "title") || STR_EQUALS(attname, "label") ||
                       STR_EQUALS(attname, "long_name")) {
                copyString(sarr[0], title, STRING_LENGTH);
            } else if (STR_EQUALS(attname, "comment")) {
                copyString(sarr[0], comment, STRING_LENGTH);
            } else if (STR_EQUALS(attname, "class")) {
                copyString(sarr[0], cls, STRING_LENGTH);
            }
            nc_free_string(attlength, sarr);
            free((void*)sarr);
        }
    }

    return 0;
}