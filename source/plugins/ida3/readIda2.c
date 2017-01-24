//! $LastChangedRevision: 312 $
//! $LastChangedDate: 2012-03-12 15:38:25 +0000 (Mon, 12 Mar 2012) $
//! $LastChangedBy: dgm $
//! $HeadURL: https://fussvn.fusion.culham.ukaea.org.uk/svnroot/IDAM/development/source/plugins/ida/readIda2.c $

/*---------------------------------------------------------------
* IDAM Plugin data Reader to Access DATA from IDA Files
*
* Input Arguments:	DATA_SOURCE data_source
*			SIGNAL_DESC signal_desc
*
* Returns:		readIDA		0 if read was successful
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
* 		A VALGRIND Test on an old IDA file generated some errors in low level IDA library components.
*		These may be due to legacy problems. The test was for shot 8323 and signals XNB_SS_BEAM_CURRENT
*		and XNB_SS_BEAM_VOLTAGE.	 
*
* ToDo:		                     
*
* Change History
*
* 1.0	01Dec2005 D.G.Muir	Simplified Version of H.M's Read_Data
* 1.1	27Mar2007 D.G.Muir	File Handle Management Added
* 1.2   09Jul2007 D.G.Muir	debugon, verbose enabled
* 29Oct2007 dgm	ERRORSTACK Components added
// 12Mar2012	dgm	Removed TESTCODE compiler option - legacy code deleted.
*-----------------------------------------------------------------------------*/

#include "readIda2.h"

#include <TrimString.h>
#include <ida3.h>
#include <idamclientserverpublic.h>
#include <idamLog.h>
#include <mastArchiveFilePath.h>
#include <idamErrorLog.h>
#include <printStructs.h>
#include <clientserver/idamErrors.h>

#include "nameIda.h"
#include "readIdaItem.h"

int readIda3Plugin(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{

    char ida_path[STRING_LENGTH] = "";
    char ida_file[IDA_FSIZE + 1] = "";
    char ida_signal[IDA_LSIZE + 1] = "";
    char ida_errmsg[256] = "";

    long pulno, pass;

    ida_file_ptr* ida_file_id = NULL;

    short context;

    int err = 0, serrno, rc;

//----------------------------------------------------------------------
// Data Source Details

    err = 0;

    DATA_SOURCE data_source = *idam_plugin_interface->data_source;
    SIGNAL_DESC signal_desc = *idam_plugin_interface->signal_desc;

    pulno = (long) data_source.exp_number;
    pass = (long) data_source.pass;

    if (pulno > 0) {

        if (strlen(data_source.source_alias) == 0) {
            strncpy(data_source.source_alias, signal_desc.signal_name, 3);
            data_source.source_alias[3] = '\0';
        }

        TrimString(data_source.source_alias);
        TrimString(data_source.filename);
        strlwr(data_source.source_alias);
        strlwr(data_source.filename);

        idamLog(LOG_DEBUG, "readIDA: alias          : %s \n", data_source.source_alias);
        idamLog(LOG_DEBUG, "readIDA: filename       : %s \n", data_source.filename);
        idamLog(LOG_DEBUG, "readIDA: length         : %d \n", strlen(data_source.source_alias));
        idamLog(LOG_DEBUG, "readIDA: alias == file? : %d \n",
                strcasecmp(data_source.filename, data_source.source_alias));

// Check whether or not the filename is the alias name   
// If is it then form the correct filename

        if (strcasecmp(data_source.filename, data_source.source_alias) == 0) {
            nameIDA(data_source.source_alias, (int) pulno, ida_file);
        } else {
            strcpy(ida_file, data_source.filename);
        }

// Check whether or not a Path has been specified

        if (strlen(data_source.path) == 0) {
            if (data_source.type == 'R') {
                mastArchiveFilePath(pulno, -1, ida_file, ida_path);    // Always Latest
            } else {
                mastArchiveFilePath(pulno, pass, ida_file, ida_path);
            }
        } else {                        // User Specified
            strcpy(ida_path, data_source.path);
        }

        idamLog(LOG_DEBUG, "readIDA: Signal Name  : %s \n", signal_desc.signal_name);
        idamLog(LOG_DEBUG, "readIDA: File Alias   : %s \n", data_source.source_alias);
        idamLog(LOG_DEBUG, "readIDA: File Name    : %s \n", ida_file);
        idamLog(LOG_DEBUG, "readIDA: File Path    : %s \n", ida_path);
        idamLog(LOG_DEBUG, "readIDA: Pulse Number : %d \n", (int) pulno);
        idamLog(LOG_DEBUG, "readIDA: Pass Number  : %d \n", (int) pass);

    } else {
        strcpy(ida_path, data_source.path);        //Fully Specified

        idamLog(LOG_DEBUG, "readIDA: Signal Name  : %s \n", signal_desc.signal_name);
        idamLog(LOG_DEBUG, "readIDA: File Name    : %s \n", ida_path);
    }

    idamLog(LOG_DEBUG, "readIDA: Signal Name  : %s \n", signal_desc.signal_name);
    idamLog(LOG_DEBUG, "readIDA: File Alias   : %s \n", data_source.source_alias);
    idamLog(LOG_DEBUG, "readIDA: File Name    : %s \n", ida_file);
    idamLog(LOG_DEBUG, "readIDA: File Path    : %s \n", ida_path);
    idamLog(LOG_DEBUG, "readIDA: Pulse Number : %d \n", (int) pulno);
    idamLog(LOG_DEBUG, "readIDA: Pass Number  : %d \n", (int) pass);

    DATA_BLOCK* data_block = idam_plugin_interface->data_block;

//---------------------------------------------------------------------- 
// Error Trap Loop

    do {

//---------------------------------------------------------------------- 
// Test String lengths are Compliant  

        if (strlen(data_source.filename) <= IDA_FSIZE + 1 || pulno < 0) {
            strcpy(ida_file, data_source.filename);
        } else {
            err = IDA_CLIENT_FILE_NAME_TOO_LONG;
            addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", err, "IDA Filename Length is too Long");
            break;
        }

        if (strlen(signal_desc.signal_name) <= IDA_LSIZE + 1) {
            strcpy(ida_signal, signal_desc.signal_name);
        } else {
            err = IDA_CLIENT_SIGNAL_NAME_TOO_LONG;
            addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", err, "IDA Signalname Length is too Long");
            break;
        }

//----------------------------------------------------------------------
// Is the IDA File Already open for Reading? If Not then Open  

        idamLog(LOG_DEBUG, "readIDA: IDA file: (%s)\n", ida_path);

        errno = 0;

#ifdef FILELISTTEST
        if((ida_file_id=(ida_file_ptr *)getOpenIdamFile(&idamfilelist, REQUEST_READ_IDA, ida_path)) == NULL){

           if ((ida_file_id = ida_open(ida_path, IDA_READ, NULL)) == NULL || errno != 0) {
          serrno = errno;
          err = IDA_ERROR_OPENING_FILE;
          if(serrno != 0) addIdamError(&idamerrorstack, SYSTEMERRORTYPE, "readIDA2", serrno, "");
              ida_error_mess(ida_error(ida_file_id), ida_errmsg);
          addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", err, ida_errmsg);
              //int sys_err = ida_system_error(ida_file_id);
              break;
           }
       addIdamFile(&idamfilelist, REQUEST_READ_IDA, ida_path, (void *)ida_file_id);		// Register the File Handle
        }
#else
        ida_file_id = ida_open(ida_path, IDA_READ, NULL);
        serrno = errno;
        if (ida_file_id == NULL || errno != 0) {
            err = IDA_ERROR_OPENING_FILE;
            if (serrno != 0) addIdamError(&idamerrorstack, SYSTEMERRORTYPE, "readIDA2", serrno, "");
            ida_error_mess(ida_error(ida_file_id), ida_errmsg);
            addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", err, ida_errmsg);
            break;
        }
#endif

//----------------------------------------------------------------------
// Fetch the Data

        idamLog(LOG_DEBUG, "Calling readIdaItem\n");

        context = (short) 0;

        if ((err = readIdaItem(ida_signal, ida_file_id, &context, data_block)) != 0) {
            err = IDA_ERROR_READING_DATA;
            addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", err, "Unable to Read IDA Data Item");
            break;
        }

        idamLog(LOG_DEBUG, "Returned from readIdaItem\n");

//----------------------------------------------------------------------
// End of Error Trap Loop

    } while (0);

    idamLog(LOG_DEBUG, "readIDA: Final Error Status = %d\n", err);
    printDataBlock(*data_block);

//---------------------------------------------------------------------- 
// Housekeeping

// Close IDA File

#ifndef FILELISTTEST
    rc = ida_close(ida_file_id);

    if (rc != 0) {
        ida_error_mess(ida_error(ida_file_id), ida_errmsg);
        addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", 1, "Problem Closing IDA File");
        addIdamError(&idamerrorstack, CODEERRORTYPE, "readIDA2", 1, ida_errmsg);
    }
#endif

    return err;
}

