/*---------------------------------------------------------------
* IDAM Plugin data Reader to Access DATA mapped to a common time invarient grid

* Input Arguments:	IDAM_PLUGIN_INTERFACE *idam_plugin_interface
*
* Returns:		equimap		0 if read was successful
*					otherwise a Error Code is returned 
*			DATA_BLOCK	Structure with Data from the File 
*
* Calls		freeDataBlock	to free Heap memory if an Error Occurs
*
* Notes: 	All memory required to hold data is allocated dynamically
*		in heap storage. Pointers to these areas of memory are held
*		by the passed DATA_BLOCK structure. Local memory allocations
*		are freed on exit. However, the blocks reserved for data are
*		not and MUST BE FREED by the calling routine.
*
*---------------------------------------------------------------------------------------------------------------*/
#include "equimap.h"

#include <stdlib.h>
#include <strings.h>

#include <clientserver/initStructs.h>
#include <client/accAPI.h>
#include <client/udaClient.h>
#include <clientserver/udaTypes.h>
#include <clientserver/stringUtils.h>

#include "importdata.h"
#include "smoothpsi.h"

static int handleCount = 0;
static int handles[MAXHANDLES];

static EQUIMAPDATA equimapdata;

static int do_ping(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static int do_help(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);

extern int equiMap(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    unsigned short housekeeping = 0;

    if (idam_plugin_interface->interfaceVersion == 1) {
        idam_plugin_interface->pluginVersion = 1;
        housekeeping = idam_plugin_interface->housekeeping;
    } else {
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown");
    }

    REQUEST_BLOCK* request_block = idam_plugin_interface->request_block;

    if (STR_IEQUALS(request_block->function, "help")) {
        return do_help(idam_plugin_interface);
    }

    if (STR_IEQUALS(request_block->function, "ping")) {
        return do_ping(idam_plugin_interface);
    }

    //----------------------------------------------------------------------------------------
    // Heap Housekeeping

    static int init = 0;
    static int prior_exp_number = -1;
    static char prior_file[MAX_STRING_LENGTH];
    static int smoothedPsi = 0;

    if (housekeeping || STR_IEQUALS(request_block->function, "reset")) {

        if (!init) return 0;        // Not previously initialised: Nothing to do!

        if (prior_exp_number == -1) {
            initEquiMapData();
        }

        // Free Heap & reset counters

        freeEquiMapData();

        init = 0;
        prior_exp_number = 0;
        prior_file[0] = '\0';
        smoothedPsi = 0;

        if (equimapdata.efitdata != NULL) free((void*)equimapdata.efitdata);

        return 0;
    }

    if (request_block->exp_number != prior_exp_number || strcmp(request_block->file, prior_file) != 0) {

        // Free Heap & reset counters

        freeEquiMapData();

        init = 0;
        smoothedPsi = 0;
    }

    //----------------------------------------------------------------------------------------
    // Initialise: Define the fixed grid, read the raw data, and set the time vector
    //             Read additional data relevant to the ITM

    // Set the number of flux surfaces using the name value pair: fluxSurfaceCount = int

    // The user has a choice of flux surface label: One must be selected

    if (!init || STR_IEQUALS(request_block->function, "init")
        || STR_IEQUALS(request_block->function, "initialise")) {

        initEquiMapData();    // initialise the data structure

        // Read the ITM Data set ?

        int i;
        for (i = 0; i < request_block->nameValueList.pairCount; i++) {
            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "readITMData")) {
                equimapdata.readITMData = 1;
                equimapdata.rhoType = NORMALISEDITMFLUXRADIUS;    // ITM Default type
                break;
            }
        }

        // Number of Flux Surfaces

        for (i = 0; i < request_block->nameValueList.pairCount; i++) {
            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "fluxSurfaceCount")) {
                equimapdata.rhoBCount = atoi(request_block->nameValueList.nameValue[i].value);
                equimapdata.rhoCount = equimapdata.rhoBCount - 1;
                break;
            }
        }

        // Identify Flux Surface label type: Mandatory requirement

        for (i = 0; i < request_block->nameValueList.pairCount; i++) {
            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "fluxSurfaceLabel")) {
                if (STR_IEQUALS(request_block->nameValueList.nameValue[i].value, "SQRTNORMALISEDTOROIDALFLUX")) {
                    equimapdata.rhoType = SQRTNORMALISEDTOROIDALFLUX;
                    break;
                } else if (STR_IEQUALS(request_block->nameValueList.nameValue[i].value, "NORMALISEDPOLOIDALFLUX")) {
                    equimapdata.rhoType = NORMALISEDPOLOIDALFLUX;
                    break;
                } else if (STR_IEQUALS(request_block->nameValueList.nameValue[i].value, "NORMALISEDITMFLUXRADIUS")) {
                    equimapdata.rhoType = NORMALISEDITMFLUXRADIUS;
                    break;
                }
                break;
            }
        }

        // Test a Flux Surface Label has been selected

        if (equimapdata.rhoType == UNKNOWNCOORDINATETYPE) {
            RAISE_PLUGIN_ERROR("No Flux Surface label type has been selected. "
                               "Use the fluxSurfaceLabel name-value pair argument to set it.");
        }

        // Preserve Shot Number// Number of Flux Surfaces

        if (request_block->exp_number == 0) {
            for (i = 0; i < request_block->nameValueList.pairCount; i++) {
                if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "shot")) {
                    request_block->exp_number = atoi(request_block->nameValueList.nameValue[i].value);
                    break;
                }
            }
        }

        equimapdata.exp_number = request_block->exp_number;

        // Create a normalised flux surface grid - no particular definition assumed - rhoType is used for Mapping

        equimapdata.rhoB = (float*)malloc(equimapdata.rhoBCount * sizeof(float));
        for (i = 0; i < equimapdata.rhoBCount; i++)
            equimapdata.rhoB[i] = (float)i / ((float)equimapdata.rhoBCount - 1);

        equimapdata.rho = (float*)malloc(equimapdata.rhoCount * sizeof(float));
        for (i = 0; i < equimapdata.rhoCount; i++)
            equimapdata.rho[i] = 0.5 * (equimapdata.rhoB[i] + equimapdata.rhoB[i + 1]);

        if (request_block->exp_number == 0 && request_block->file[0] == '\0') {
            RAISE_PLUGIN_ERROR("No Shot Number or Private File!");
        }

        if ((importData(request_block, &equimapdata)) != 0) {
            RAISE_PLUGIN_ERROR("Problem importing data");
        }

        // Universal/Default set of times
        int err;
        if ((err = selectTimes(&equimapdata)) != 0) {
            return err;
        }

        equimapdata.efitdata = (EFITDATA*)malloc(equimapdata.timeCount * sizeof(EFITDATA));
        for (i = 0; i < equimapdata.timeCount; i++) {
            initEfitData(&equimapdata.efitdata[i]);
            if ((err = extractData(equimapdata.times[i], &equimapdata.efitdata[i], &equimapdata)) != 0) return err;
        }

        init = 1;
        prior_exp_number = request_block->exp_number;        // Retain previous analysis shot to automatically re-initialise when different
        strcpy(prior_file, request_block->file);

        if (STR_IEQUALS(request_block->function, "init") || STR_IEQUALS(request_block->function, "initialise")) {
            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            const char* str = "Initialisation Completed";
            data_block->rank = 0;
            data_block->data_n = strlen(str) + 1;
            data_block->data_type = UDA_TYPE_CHAR;
            data_block->data = strdup(str);
            return 0;
        }
    }

    //----------------------------------------------------------------------------------------
    // Processing over the Time domain
    //----------------------------------------------------------------------------------------

    static float priorLimitRMaj = -1.0;

    {
        int i;
        for (i = 0; i < request_block->nameValueList.pairCount; i++) {

            // Reduce the size of the psi grid to the minimum size enclosing the boundary
            // Fixed grid so need to test all time points to establish the spatial range
            // Data is processed once only - smoothing is not reversible!

            if (STR_IEQUALS(request_block->nameValueList.nameValue[i].name, "smoothPsi")) {

                UDA_LOG(UDA_LOG_DEBUG, "EQUIMAP: processing time domain option 'smoothPsi'\n");

                int invert = 0;
                int limitPsi = 0;
                float limitRMaj = -1.0;
                int j;
                for (j = 0; j < request_block->nameValueList.pairCount; j++) {
                    if (STR_IEQUALS(request_block->nameValueList.nameValue[j].name, "invert")) invert = 1;
                    if (STR_IEQUALS(request_block->nameValueList.nameValue[j].name, "limitPsi")) limitPsi = 1;
                    if (STR_IEQUALS(request_block->nameValueList.nameValue[j].name, "limitRMaj")) {
                        limitRMaj = (float)atof(request_block->nameValueList.nameValue[j].value);
                    }
                }

                UDA_LOG(UDA_LOG_DEBUG, "EQUIMAP: smoothPsi(invert=%d, limitPsi=%d, limitRMaj=%f)\n", invert, limitPsi,
                        limitRMaj);

                if (!smoothedPsi) {
                    smoothPsi(&equimapdata, invert, limitPsi, -1.0);        // Constrain by LCFS
                    smoothedPsi = 1;
                }
                if (limitRMaj != -1.0 && limitRMaj != priorLimitRMaj) {
                    smoothPsi(&equimapdata, invert, limitPsi, limitRMaj);        // Constrain by upper RMajor
                    priorLimitRMaj = limitRMaj;
                }
                UDA_LOG(UDA_LOG_DEBUG, "EQUIMAP: psiRZBox nr=%d, nz=%d)\n", equimapdata.efitdata[0].psiCountRZBox[0],
                        equimapdata.efitdata[0].psiCountRZBox[1]);
            }
        }
    }

    //----------------------------------------------------------------------------------------
    // Functions
    //----------------------------------------------------------------------------------------

    int err = 0;

    do {

// Set the required Times via an ASCII Name value pair or subset

        if (STR_IEQUALS(request_block->function, "setTimes")) {    // User specifies a set of times
            if ((err = subsetTimes(request_block)) != 0) break;    // Subset
        } else

// Return the list of available times

        if (STR_IEQUALS(request_block->function, "listTimes")) {
            break;
        } else

// Limiter Coordinates (Not time dependent)

        if (STR_IEQUALS(request_block->function, "Rlim") ||
            STR_IEQUALS(request_block->function, "Zlim")) {

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 1;
            data_block->order = -1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));
            initDimBlock(&data_block->dims[0]);

            data_block->dims[0].dim_n = equimapdata.efitdata[0].nlim;
            data_block->dims[0].dim = NULL;
            data_block->dims[0].compressed = 1;
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;
            data_block->dims[0].method = 0;
            data_block->dims[0].dim0 = 0.0;
            data_block->dims[0].diff = 1.0;
            data_block->dims[0].dim_units[0] = '\0';
            data_block->dims[0].dim_label[0] = '\0';

            data_block->data_n = data_block->dims[0].dim_n;
            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));
            float* arr = (float*)data_block->data;

            if (STR_IEQUALS(request_block->function, "Rlim")) {
                int i;
                for (i = 0; i < data_block->data_n; i++) {
                    arr[i] = equimapdata.efitdata[0].rlim[i];
                }
            } else if (STR_IEQUALS(request_block->function, "Zlim")) {
                int i;
                for (i = 0; i < data_block->data_n; i++) {
                    arr[i] = equimapdata.efitdata[0].zlim[i];
                }
            }

            int handle = 0;
            if (STR_IEQUALS(request_block->function, "Rlim")) {
                handle = whichHandle("Rlim");
            } else if (STR_IEQUALS(request_block->function, "Zlim")) {
                handle = whichHandle("Zlim");
            }
            if (handle >= 0) {
                strcpy(data_block->data_units, getIdamDataUnits(handle));
                strcpy(data_block->data_label, getIdamDataLabel(handle));
                strcpy(data_block->data_desc, getIdamDataDesc(handle));
            }
            break;

        } else if (STR_IEQUALS(request_block->function, "Rmin") ||
                   STR_IEQUALS(request_block->function, "Rmax") ||
                   STR_IEQUALS(request_block->function, "Rmag") ||
                   STR_IEQUALS(request_block->function, "Zmag") ||
                   STR_IEQUALS(request_block->function, "Bphi") ||
                   STR_IEQUALS(request_block->function, "Bvac") ||
                   STR_IEQUALS(request_block->function, "Rvac") ||
                   STR_IEQUALS(request_block->function, "Ip") ||
                   STR_IEQUALS(request_block->function, "psiBoundary") ||
                   STR_IEQUALS(request_block->function, "psiMag") ||
                   STR_IEQUALS(request_block->function, "Nlcfs") ||
                   STR_IEQUALS(request_block->function, "Npsiz0") ||
                   STR_IEQUALS(request_block->function, "rhotorb") ||

                   (equimapdata.readITMData && STR_IEQUALS(request_block->function, "Rgeom")) ||
                   (equimapdata.readITMData && STR_IEQUALS(request_block->function, "Zgeom")) ||
                   (equimapdata.readITMData && STR_IEQUALS(request_block->function, "Aminor")) ||
                   (equimapdata.readITMData && STR_IEQUALS(request_block->function, "TriangL")) ||
                   (equimapdata.readITMData && STR_IEQUALS(request_block->function, "TriangU")) ||
                   (equimapdata.readITMData && STR_IEQUALS(request_block->function, "Elong"))) {

            // Rank 1 Time Series Data?

            int handle = whichHandle("Rmag");        // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 1;
            data_block->order = 0;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));
            initDimBlock(&data_block->dims[0]);

            // Time Dimension

            data_block->dims[0].dim_n = equimapdata.timeCount;
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;
            data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
            memcpy(data_block->dims[0].dim, equimapdata.times, data_block->dims[0].dim_n * sizeof(float));

            data_block->dims[0].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[0].dim_units, tdim->dim_units);
                strcpy(data_block->dims[0].dim_label, tdim->dim_label);
            } else {
                data_block->dims[0].dim_units[0] = '\0';
                data_block->dims[0].dim_label[0] = '\0';
            }

            // Collect data trace labels

            handle = -1;

            data_block->data_n = data_block->dims[0].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));
            float* arr = (float*)data_block->data;

            int i;
            if (STR_IEQUALS(request_block->function, "Rmin")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].Rmin;
            } else if (STR_IEQUALS(request_block->function, "Rmax")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].Rmax;
            } else if (STR_IEQUALS(request_block->function, "Rmag")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].rmag;
            } else if (STR_IEQUALS(request_block->function, "Zmag")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].zmag;
            } else if (STR_IEQUALS(request_block->function, "Bphi")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].bphi;
            } else if (STR_IEQUALS(request_block->function, "Bvac")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].bvac;
            } else if (STR_IEQUALS(request_block->function, "Rvac")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].rvac;
            } else if (STR_IEQUALS(request_block->function, "Ip")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].ip;
            } else if (STR_IEQUALS(request_block->function, "psiBoundary")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].psi_bnd;
            } else if (STR_IEQUALS(request_block->function, "psiMag")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].psi_mag;
            } else if (STR_IEQUALS(request_block->function, "Nlcfs")) {
                data_block->data_type = UDA_TYPE_INT;
                int* iarr = (int*)data_block->data;
                for (i = 0; i < equimapdata.timeCount; i++) iarr[i] = equimapdata.efitdata[i].nlcfs;
            } else if (STR_IEQUALS(request_block->function, "Npsiz0")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].rz0Count;
            } else if (STR_IEQUALS(request_block->function, "rhotorb")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].rho_torb;
            } else if (STR_IEQUALS(request_block->function, "Rgeom")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].rgeom;
            } else if (STR_IEQUALS(request_block->function, "Zgeom")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].zgeom;
            } else if (STR_IEQUALS(request_block->function, "Aminor")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].aminor;
            } else if (STR_IEQUALS(request_block->function, "TriangL")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].triangL;
            } else if (STR_IEQUALS(request_block->function, "TriangU")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].triangU;
            } else if (STR_IEQUALS(request_block->function, "Elong")) {
                for (i = 0; i < equimapdata.timeCount; i++) arr[i] = equimapdata.efitdata[i].elong;
            } else if (STR_IEQUALS(request_block->function, "Rmin")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Rmin");
                strcpy(data_block->data_desc, "Inner Boundary Radius");
            } else if (STR_IEQUALS(request_block->function, "Rmax")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Rmax");
                strcpy(data_block->data_desc, "Outer Boundary Radius");
            } else if (STR_IEQUALS(request_block->function, "Rmag")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Rmag");
                strcpy(data_block->data_desc, "Magentic Axis Radius");
            } else if (STR_IEQUALS(request_block->function, "Zmag")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Zmag");
                strcpy(data_block->data_desc, "Magentic Axis Height");
            } else if (STR_IEQUALS(request_block->function, "Bphi")) {
                strcpy(data_block->data_units, "T");
                strcpy(data_block->data_label, "Bphi");
                strcpy(data_block->data_desc, "Toroidal Magnetic Field");
            } else if (STR_IEQUALS(request_block->function, "Bvac")) {
                strcpy(data_block->data_units, "T");
                strcpy(data_block->data_label, "Bvac");
                strcpy(data_block->data_desc, "Vacuum Toroidal Magnetic Field at reference radius");
            } else if (STR_IEQUALS(request_block->function, "Rvac")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Rvac");
                strcpy(data_block->data_desc, "Reference Major Radius of Bvac");
            } else if (STR_IEQUALS(request_block->function, "Ip")) {
                strcpy(data_block->data_units, "A");
                strcpy(data_block->data_label, "Ip");
                strcpy(data_block->data_desc, "Toroidal Plasma Current");
            } else if (STR_IEQUALS(request_block->function, "psiBoundary")) {
                strcpy(data_block->data_units, "Wb");
                strcpy(data_block->data_label, "psiB");
                strcpy(data_block->data_desc, "Boundary Poloidal Magnetic Flux");
            } else if (STR_IEQUALS(request_block->function, "psiMag")) {
                strcpy(data_block->data_units, "Wb");
                strcpy(data_block->data_label, "psiMag");
                strcpy(data_block->data_desc, "Axial Poloidal Magnetic Flux");
            } else if (STR_IEQUALS(request_block->function, "Nlcfs")) {
                strcpy(data_block->data_units, "");
                strcpy(data_block->data_label, "Nlcfs");
                strcpy(data_block->data_desc, "Number of Coordinates in the LCFS Boundary");
            } else if (STR_IEQUALS(request_block->function, "Npsiz0")) {
                strcpy(data_block->data_units, "");
                strcpy(data_block->data_label, "Npsiz0");
                strcpy(data_block->data_desc, "Number of Coordinates in the Mid-Plane poloidal flux grid");
            } else if (STR_IEQUALS(request_block->function, "rhotorb")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "rho_torb");
                strcpy(data_block->data_desc, "ITM Toroidal Flux Radius at Boundary");
            } else if (STR_IEQUALS(request_block->function, "Rgeom")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Rgeom");
                strcpy(data_block->data_desc, "Geometrical Axis of boundary (R)");
            } else if (STR_IEQUALS(request_block->function, "Zgeom")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Zgeom");
                strcpy(data_block->data_desc, "Geometrical Axis of boundary (Z)");
            } else if (STR_IEQUALS(request_block->function, "Aminor")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "Aminor");
                strcpy(data_block->data_desc, "Minor Radius");
            } else if (STR_IEQUALS(request_block->function, "TriangL")) {
                strcpy(data_block->data_units, "");
                strcpy(data_block->data_label, "TriangL");
                strcpy(data_block->data_desc, "Lower Triagularity");
            } else if (STR_IEQUALS(request_block->function, "TriangU")) {
                strcpy(data_block->data_units, "");
                strcpy(data_block->data_label, "TriangU");
                strcpy(data_block->data_desc, "Upper Triagularity");
            } else if (STR_IEQUALS(request_block->function, "Elong")) {
                strcpy(data_block->data_units, "");
                strcpy(data_block->data_label, "Elong");
                strcpy(data_block->data_desc, "Elongation");
            } else {
                data_block->data_units[0] = '\0';
                data_block->data_label[0] = '\0';
                data_block->data_desc[0] = '\0';
            }

            break;
        }

        if (STR_IEQUALS(request_block->function, "psiCoord") ||
            STR_IEQUALS(request_block->function, "Phi") ||
            STR_IEQUALS(request_block->function, "Q") ||
            STR_IEQUALS(request_block->function, "PRho") ||
            STR_IEQUALS(request_block->function, "TRho") ||
            STR_IEQUALS(request_block->function, "RhoTor") ||
            STR_IEQUALS(request_block->function, "Rlcfs") ||
            STR_IEQUALS(request_block->function, "Zlcfs") ||
            (STR_IEQUALS(request_block->function, "P")) ||
            (STR_IEQUALS(request_block->function, "F")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "PPrime")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "FFPrime")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "ElongPsi")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "TriangLPsi")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "TriangUPsi")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "VolPsi")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "AreaPsi"))) {

            // Rank 2 Equilibrium Profile Data  [time][rho]

            unsigned short lcfsData = 0;

            int handle = whichHandle("Rmag");        // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));
            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Flux Surface Label: Normalised poloidal flux

            handle = -1;
            if (STR_IEQUALS(request_block->function, "Q")) {
                handle = whichHandle("Q");
            } else if (STR_IEQUALS(request_block->function, "P")) {
                handle = whichHandle("P");
            } else if (STR_IEQUALS(request_block->function, "F")) {
                handle = whichHandle("F");
            } else if (STR_IEQUALS(request_block->function, "PPrime")) {
                handle = whichHandle("PPrime");
            } else if (STR_IEQUALS(request_block->function, "FFPrime")) {
                handle = whichHandle("FFPrime");
            } else if (STR_IEQUALS(request_block->function, "ElongPsi")) {
                handle = whichHandle("ElongPsi");
            } else if (STR_IEQUALS(request_block->function, "TriangLPsi")) {
                handle = whichHandle("TriangLPsi");
            } else if (STR_IEQUALS(request_block->function, "TriangUPsi")) {
                handle = whichHandle("TriangUPsi");
            } else if (STR_IEQUALS(request_block->function, "VolPsi")) {
                handle = whichHandle("VolPsi");
            } else if (STR_IEQUALS(request_block->function, "AreaPsi")) {
                handle = whichHandle("AreaPsi");
            } else if (STR_IEQUALS(request_block->function, "psiCoord") ||
                       STR_IEQUALS(request_block->function, "phi") ||
                       STR_IEQUALS(request_block->function, "PRho") ||        // poloidal Flux Label
                       STR_IEQUALS(request_block->function, "TRho") ||        // toroidal Flux Label
                       STR_IEQUALS(request_block->function, "RhoTor")) {        // ITM Normalised Flux Radius
                handle = whichHandle("Q");                    // Use dimension coordinate labels from Q
            } else if (STR_IEQUALS(request_block->function, "Rlcfs")) {
                lcfsData = 1;
                handle = whichHandle("Rlcfs");
            } else if (STR_IEQUALS(request_block->function, "Zlcfs")) {
                lcfsData = 1;
                handle = whichHandle("Zlcfs");
            }

            if (handle >= 0) {
                if (!lcfsData) {
                    DIMS* xdim = getIdamDimBlock(handle, 0);
                    data_block->dims[0].data_type = UDA_TYPE_FLOAT;
                    strcpy(data_block->dims[0].dim_units, xdim->dim_units);
                    strcpy(data_block->dims[0].dim_label, xdim->dim_label);
                    data_block->dims[0].dim_n = xdim->dim_n;
                    data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
                    getIdamFloatDimData(handle, 0, (float*)data_block->dims[0].dim);

                    //memcpy(data_block->dims[0].dim, xdim->dim, data_block->dims[0].dim_n*sizeof(float));  // *** BUG - assumes FLOAT !

                    data_block->dims[0].compressed = 0;
                } else {
                    int maxn = 0;
                    int i;
                    for (i = 0; i < equimapdata.timeCount; i++) {
                        if (maxn < equimapdata.efitdata[i].nlcfs) {
                            maxn = equimapdata.efitdata[i].nlcfs;
                        }
                    }
                    data_block->dims[0].dim_n = maxn;
                    data_block->dims[0].dim = NULL;
                    data_block->dims[0].compressed = 1;
                    data_block->dims[0].data_type = UDA_TYPE_FLOAT;
                    data_block->dims[0].method = 0;
                    data_block->dims[0].dim0 = 0.0;
                    data_block->dims[0].diff = 1.0;
                    strcpy(data_block->dims[0].dim_units, "");
                    strcpy(data_block->dims[0].dim_label, "LCFS coordinate id");
                }
            }

            // Data

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            float* arr = (float*)data_block->data;
            int j;
            if (STR_IEQUALS(request_block->function, "Q")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].q[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "P")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].p[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "F")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].f[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "PPrime")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].pprime[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "FFPrime")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].ffprime[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "ElongPsi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].elongp[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "TriangLPsi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].trianglp[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "TriangUPsi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].triangup[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "VolPsi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].volp[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "AreaPsi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].areap[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "RhoTor")) {
                int i;
                handle = -1;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].rho_tor[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "PRho")) {
                int i;
                handle = -1;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].rho[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "TRho")) {
                int i;
                handle = -1;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].trho[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "PsiCoord")) {
                int i;
                handle = -1;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].psi[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Phi")) {
                int i;
                handle = -1;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].phi[j];
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Rlcfs")) {
                int i;
                int maxn = data_block->dims[0].dim_n;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < equimapdata.efitdata[i].nlcfs; j++) {
                        int offset = i * maxn + j;
                        arr[offset] = equimapdata.efitdata[i].rlcfs[j];
                    }
                    for (j = equimapdata.efitdata[i].nlcfs; j < maxn; j++) {
                        int offset = i * maxn + j;
                        arr[offset] = 0.0;
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Zlcfs")) {
                int i;
                int maxn = data_block->dims[0].dim_n;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    for (j = 0; j < equimapdata.efitdata[i].nlcfs; j++) {
                        int offset = i * maxn + j;
                        arr[offset] = equimapdata.efitdata[i].zlcfs[j];
                    }
                    for (j = equimapdata.efitdata[i].nlcfs; j < maxn; j++) {
                        int offset = i * maxn + j;
                        arr[offset] = 0.0;
                    }
                }
            }

            if (handle >= 0) {
                strcpy(data_block->data_units, getIdamDataUnits(handle));
                strcpy(data_block->data_label, getIdamDataLabel(handle));
                strcpy(data_block->data_desc, getIdamDataDesc(handle));
            } else {
                if (STR_IEQUALS(request_block->function, "PsiCoord")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "Psi");
                    strcpy(data_block->data_desc, "Poloidal Flux Coordinate");
                } else if (STR_IEQUALS(request_block->function, "Phi")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "Phi");
                    strcpy(data_block->data_desc, "Toroidal Flux Coordinate");
                } else if (STR_IEQUALS(request_block->function, "PRho")) {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "Rho");
                    strcpy(data_block->data_desc, "Normalised Poloidal Flux");
                } else if (STR_IEQUALS(request_block->function, "TRho")) {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "TRho");
                    strcpy(data_block->data_desc, "SQRT Normalised Toroidal Flux");
                } else if (STR_IEQUALS(request_block->function, "RhoTor")) {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "Rho_Tor");
                    strcpy(data_block->data_desc, "Normalised ITM Toroidal Flux Radius");
                } else if (STR_IEQUALS(request_block->function, "Rlcfs")) {
                    strcpy(data_block->data_units, "m");
                    strcpy(data_block->data_label, "Rlcfs");
                    strcpy(data_block->data_desc, "Major Radius of LCFS Boundary points");
                } else if (STR_IEQUALS(request_block->function, "Zlcfs")) {
                    strcpy(data_block->data_units, "m");
                    strcpy(data_block->data_label, "Zlcfs");
                    strcpy(data_block->data_desc, "Height above mid-plane of LCFS Boundary points");
                }

            }

            break;
        }

        if (STR_IEQUALS(request_block->function, "PsiZ0") ||
            STR_IEQUALS(request_block->function, "RPsiZ0")) {    // Generally ragged arrays !

            int handle = whichHandle("psi");                // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Mid-Plane Major Radius - needs regularising to a fixed size - use boundary psi value to pack extra points

            int rz0CountMax = 0;

            int i;
            for (i = 0; i < equimapdata.timeCount; i++) {
                if (equimapdata.efitdata[i].rz0Count > rz0CountMax) {
                    rz0CountMax = equimapdata.efitdata[i].rz0Count;
                }
            }

            data_block->dims[0].dim_n = rz0CountMax;
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;
            data_block->dims[0].dim = malloc(rz0CountMax * sizeof(float));

            data_block->dims[0].compressed = 1;
            data_block->dims[0].dim0 = 0.0;
            data_block->dims[0].diff = 1.0;
            data_block->dims[0].method = 0;
            strcpy(data_block->dims[0].dim_units, "");
            strcpy(data_block->dims[0].dim_label, "Ragged Radial Grid Index)");

            // Data

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            float* arr = (float*)data_block->data;

            if (STR_IEQUALS(request_block->function, "PsiZ0")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j;
                    for (j = 0; j < equimapdata.efitdata[i].rz0Count; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].psiz0[j];
                    }
                    if (rz0CountMax > equimapdata.efitdata[i].rz0Count) {
                        for (j = equimapdata.efitdata[i].rz0Count; j < rz0CountMax; j++) {
                            int offset = i * data_block->dims[0].dim_n + j;
                            arr[offset] = equimapdata.efitdata[i].psiz0[equimapdata.efitdata[i].rz0Count - 1];
                        }
                    }
                }
            } else {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j;
                    for (j = 0; j < equimapdata.efitdata[i].rz0Count; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].rz0[j];
                    }
                    if (rz0CountMax > equimapdata.efitdata[i].rz0Count) {
                        for (j = equimapdata.efitdata[i].rz0Count; j < rz0CountMax; j++) {
                            int offset = i * data_block->dims[0].dim_n + j;
                            arr[offset] = equimapdata.efitdata[i].rz0[equimapdata.efitdata[i].rz0Count - 1];
                        }
                    }
                }
            }

            if (STR_IEQUALS(request_block->function, "PsiZ0")) {
                if (handle >= 0) {
                    strcpy(data_block->data_units, getIdamDataUnits(handle));
                } else {
                    strcpy(data_block->data_units, "");
                }
                strcpy(data_block->data_label, "Psi(R,Z=0)");
                strcpy(data_block->data_desc, "Psi Profile (R,Z=0)");
            } else {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "R(Z=0)");
                strcpy(data_block->data_desc, "Mid-Plane Major Radii of Poloidal Flux");
            }
            break;
        }

        // Rank 3 Equilibrium Profile Data

        if (STR_IEQUALS(request_block->function, "Psi") ||
            STR_IEQUALS(request_block->function, "Br") ||
            STR_IEQUALS(request_block->function, "Bz") ||
            STR_IEQUALS(request_block->function, "Bt") ||
            STR_IEQUALS(request_block->function, "Jphi")) {

            int handle = whichHandle("Rmag");        // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 3;
            data_block->order = 2;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[2].dim_n = equimapdata.timeCount;
            data_block->dims[2].data_type = UDA_TYPE_FLOAT;
            data_block->dims[2].dim = malloc(data_block->dims[2].dim_n * sizeof(float));
            memcpy(data_block->dims[2].dim, equimapdata.times, data_block->dims[2].dim_n * sizeof(float));

            data_block->dims[2].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[2].dim_units, tdim->dim_units);
                strcpy(data_block->dims[2].dim_label, tdim->dim_label);
            } else {
                data_block->dims[2].dim_units[0] = '\0';
                data_block->dims[2].dim_label[0] = '\0';
            }

            // Spatial Coordinate Grid (R, Z)

            handle = whichHandle("psi");        // array[nt][nz][nr] = [2][1][0]

            if (handle >= 0) {
                DIMS* xdim = getIdamDimBlock(handle, 0);
                data_block->dims[0].dim_n = equimapdata.efitdata[0].psiCount[0];
                data_block->dims[0].data_type = UDA_TYPE_FLOAT;
                data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
                memcpy(data_block->dims[0].dim, equimapdata.efitdata[0].rgrid,
                       data_block->dims[0].dim_n * sizeof(float));
                data_block->dims[0].compressed = 0;
                strcpy(data_block->dims[0].dim_units, xdim->dim_units);
                strcpy(data_block->dims[0].dim_label, xdim->dim_label);

                xdim = getIdamDimBlock(handle, 1);
                data_block->dims[1].dim_n = equimapdata.efitdata[0].psiCount[1];
                data_block->dims[1].data_type = UDA_TYPE_FLOAT;
                data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
                memcpy(data_block->dims[1].dim, equimapdata.efitdata[0].zgrid,
                       data_block->dims[1].dim_n * sizeof(float));
                data_block->dims[1].compressed = 0;
                strcpy(data_block->dims[1].dim_units, xdim->dim_units);
                strcpy(data_block->dims[1].dim_label, xdim->dim_label);
            } else {
                RAISE_PLUGIN_ERROR("Corrupted Psi Data!");
            }

            // Data

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n * data_block->dims[2].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            float* arr = (float*)data_block->data;

            if (STR_IEQUALS(request_block->function, "Psi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].psig[j][k];
                        }
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Br")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].Br[j][k];
                        }
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Bz")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].Bz[j][k];
                        }
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Bt")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].Bphi[j][k];
                        }
                    }
                }
            } else if (STR_IEQUALS(request_block->function, "Jphi")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].Jphi[j][k];
                        }
                    }
                }
            }

            if (STR_IEQUALS(request_block->function, "Psi")) {
                if (handle >= 0) {
                    strcpy(data_block->data_units, getIdamDataUnits(handle));
                    strcpy(data_block->data_label, getIdamDataLabel(handle));
                    strcpy(data_block->data_desc, getIdamDataDesc(handle));
                } else {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "Psi");
                    strcpy(data_block->data_desc, "Psi Surface");
                }
            } else if (STR_IEQUALS(request_block->function, "Br")) {
                strcpy(data_block->data_units, "T");
                strcpy(data_block->data_label, "Br");
                strcpy(data_block->data_desc, "Radial Magnetic Field");
            } else if (STR_IEQUALS(request_block->function, "Bz")) {
                strcpy(data_block->data_units, "T");
                strcpy(data_block->data_label, "Bz");
                strcpy(data_block->data_desc, "Vertical Magnetic Field");
            } else if (STR_IEQUALS(request_block->function, "Bt")) {
                strcpy(data_block->data_units, "T");
                strcpy(data_block->data_label, "Bphi");
                strcpy(data_block->data_desc, "Toroidal Magnetic Field");
            } else if (STR_IEQUALS(request_block->function, "Jphi")) {
                strcpy(data_block->data_units, "Am-2");
                strcpy(data_block->data_label, "Jphi");
                strcpy(data_block->data_desc, "Toroidal Current Density");
            }

            break;
        }

        if (STR_IEQUALS(request_block->function, "PsiSR") || STR_IEQUALS(request_block->function, "PsiRZBox")) {

            int handle = whichHandle("Rmag");        // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 3;
            data_block->order = 2;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[2].dim_n = equimapdata.timeCount;
            data_block->dims[2].data_type = UDA_TYPE_FLOAT;
            data_block->dims[2].dim = malloc(data_block->dims[2].dim_n * sizeof(float));
            memcpy(data_block->dims[2].dim, equimapdata.times, data_block->dims[2].dim_n * sizeof(float));

            data_block->dims[2].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[2].dim_units, tdim->dim_units);
                strcpy(data_block->dims[2].dim_label, tdim->dim_label);
            } else {
                data_block->dims[2].dim_units[0] = '\0';
                data_block->dims[2].dim_label[0] = '\0';
            }

            // Spatial Coordinate Grid (R, Z)

            handle = whichHandle("psi");        // array[nt][nz][nr] = [2][1][0]

            if (handle >= 0) {
                DIMS* xdim = getIdamDimBlock(handle, 0);
                if (STR_IEQUALS(request_block->function, "PsiSR")) {
                    data_block->dims[0].dim_n = equimapdata.efitdata[0].psiCountSR[0];
                } else {
                    data_block->dims[0].dim_n = equimapdata.efitdata[0].psiCountRZBox[0];
                }
                data_block->dims[0].data_type = UDA_TYPE_FLOAT;
                data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
                if (STR_IEQUALS(request_block->function, "PsiSR")) {
                    memcpy(data_block->dims[0].dim, equimapdata.efitdata[0].rgridSR,
                           data_block->dims[0].dim_n * sizeof(float));
                } else {
                    memcpy(data_block->dims[0].dim, equimapdata.efitdata[0].rgridRZBox,
                           data_block->dims[0].dim_n * sizeof(float));
                }
                data_block->dims[0].compressed = 0;
                strcpy(data_block->dims[0].dim_units, xdim->dim_units);
                strcpy(data_block->dims[0].dim_label, xdim->dim_label);

                xdim = getIdamDimBlock(handle, 1);
                if (STR_IEQUALS(request_block->function, "PsiSR")) {
                    data_block->dims[1].dim_n = equimapdata.efitdata[0].psiCountSR[1];
                } else {
                    data_block->dims[1].dim_n = equimapdata.efitdata[0].psiCountRZBox[1];
                }
                data_block->dims[1].data_type = UDA_TYPE_FLOAT;
                data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
                if (STR_IEQUALS(request_block->function, "PsiSR")) {
                    memcpy(data_block->dims[1].dim, equimapdata.efitdata[0].zgridSR,
                           data_block->dims[1].dim_n * sizeof(float));
                } else {
                    memcpy(data_block->dims[1].dim, equimapdata.efitdata[0].zgridRZBox,
                           data_block->dims[1].dim_n * sizeof(float));
                }
                data_block->dims[1].compressed = 0;
                strcpy(data_block->dims[1].dim_units, xdim->dim_units);
                strcpy(data_block->dims[1].dim_label, xdim->dim_label);
            } else {
                RAISE_PLUGIN_ERROR("Corrupted PsiSR Data!");
            }

            // Data

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n * data_block->dims[2].dim_n;

            if (data_block->data_n == 0) {
                UDA_LOG(UDA_LOG_DEBUG, "dims[0].dim_n = %d\n", data_block->dims[0].dim_n);
                UDA_LOG(UDA_LOG_DEBUG, "dims[1].dim_n = %d\n", data_block->dims[1].dim_n);
                UDA_LOG(UDA_LOG_DEBUG, "dims[2].dim_n = %d\n", data_block->dims[2].dim_n);
                RAISE_PLUGIN_ERROR("No Data Values selected!");
            }

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            float* arr = (float*)data_block->data;

            if (STR_IEQUALS(request_block->function, "PsiSR")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].psigSR[j][k];
                        }
                    }
                }
            } else {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j, k;
                    for (j = 0; j < data_block->dims[1].dim_n; j++) {
                        for (k = 0; k < data_block->dims[0].dim_n; k++) {
                            int offset = j * data_block->dims[0].dim_n + k +
                                         i * data_block->dims[0].dim_n * data_block->dims[1].dim_n;
                            arr[offset] = equimapdata.efitdata[i].psigRZBox[j][k];
                        }
                    }
                }
            }

            if (handle >= 0) {
                strcpy(data_block->data_units, getIdamDataUnits(handle));
                strcpy(data_block->data_label, getIdamDataLabel(handle));
                strcpy(data_block->data_desc, getIdamDataDesc(handle));
            } else {
                strcpy(data_block->data_units, "");
                strcpy(data_block->data_label, "Psi");
                if (STR_IEQUALS(request_block->function, "PsiSR")) {
                    strcpy(data_block->data_desc, "Smoothed/Reduced Psi Surface");
                } else {
                    strcpy(data_block->data_desc, "R-Z Box constrained Psi Surface");
                }
            }

            break;
        }

        // Experimental Data?

        if (STR_IEQUALS(request_block->function, "yag_psi") ||
            STR_IEQUALS(request_block->function, "yag_phi") ||
            STR_IEQUALS(request_block->function, "yag_prho") ||
            STR_IEQUALS(request_block->function, "yag_trho") ||
            STR_IEQUALS(request_block->function, "yag_rhotor") ||
            STR_IEQUALS(request_block->function, "yag_R") ||
            STR_IEQUALS(request_block->function, "yag_ne") ||
            STR_IEQUALS(request_block->function, "yag_Te")) {

            int handle = whichHandle("EFM_MAGNETIC_AXIS_R");        // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Flux Surface Label

            data_block->dims[0].dim_n = equimapdata.efitdata[0].nne;
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;

            data_block->dims[0].dim = NULL;

            data_block->dims[0].compressed = 1;
            data_block->dims[0].dim0 = 0.0;
            data_block->dims[0].diff = 1.0;
            data_block->dims[0].method = 0;

            strcpy(data_block->dims[0].dim_units, "");
            strcpy(data_block->dims[0].dim_label, "Flux Surface Label)");

            // Collect data traces

            handle = -1;

            if (STR_IEQUALS(request_block->function, "yag_R")) {
                handle = whichHandle("ayc_r");
            } else if (STR_IEQUALS(request_block->function, "yag_ne")) {
                handle = whichHandle("ayc_ne");
            } else if (STR_IEQUALS(request_block->function, "yag_Te")) {
                handle = whichHandle("ayc_Te");
            }

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            int i;
            for (i = 0; i < equimapdata.timeCount; i++) {
                int j;
                if (STR_IEQUALS(request_block->function, "yag_R")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].rne[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_ne")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].ne[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_Te")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].te[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_psi")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].yagpsi[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_phi")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].yagphi[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_trho")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].yagtrho[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_prho")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].yagprho[j];
                    }
                } else if (STR_IEQUALS(request_block->function, "yag_rhotor")) {
                    float* arr = (float*)data_block->data;
                    for (j = 0; j < data_block->dims[0].dim_n; j++) {
                        int offset = i * data_block->dims[0].dim_n + j;
                        arr[offset] = equimapdata.efitdata[i].yagrhotor[j];
                    }
                }
            }

            if (handle >= 0) {
                strcpy(data_block->data_units, getIdamDataUnits(handle));
                strcpy(data_block->data_label, getIdamDataLabel(handle));
                strcpy(data_block->data_desc, getIdamDataDesc(handle));
            } else {
                if (STR_IEQUALS(request_block->function, "yag_psi")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "psi");
                    strcpy(data_block->data_desc, "Poloidal Flux");
                } else if (STR_IEQUALS(request_block->function, "yag_phi")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "phi");
                    strcpy(data_block->data_desc, "Toroidal Flux");
                } else if (STR_IEQUALS(request_block->function, "yag_trho")) {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "trho");
                    strcpy(data_block->data_desc, "SQRT Normalised Toroidal Flux");
                } else if (STR_IEQUALS(request_block->function, "yag_prho")) {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "rho");
                    strcpy(data_block->data_desc, "Normalised Poloidal Flux");
                } else if (STR_IEQUALS(request_block->function, "yag_rhotor")) {
                    strcpy(data_block->data_units, "");
                    strcpy(data_block->data_label, "rho_tor");
                    strcpy(data_block->data_desc, "Normalised ITM Toroidal Flux Radius");
                }
            }

            break;
        }

        // Experimental Data Mapped to Fixed Grid? (Volume or Mid-Points)

        if (STR_IEQUALS(request_block->function, "MPsi") ||
            STR_IEQUALS(request_block->function, "MQ") ||
            STR_IEQUALS(request_block->function, "MYPsi") ||
            STR_IEQUALS(request_block->function, "MYPsi_inner") ||
            STR_IEQUALS(request_block->function, "MYPsi_outer") ||
            STR_IEQUALS(request_block->function, "MYPhi") ||
            STR_IEQUALS(request_block->function, "MYPhi_inner") ||
            STR_IEQUALS(request_block->function, "MYPhi_outer") ||
            STR_IEQUALS(request_block->function, "R_inner") ||
            STR_IEQUALS(request_block->function, "R_outer") ||
            STR_IEQUALS(request_block->function, "ne") ||
            STR_IEQUALS(request_block->function, "ne_inner") ||
            STR_IEQUALS(request_block->function, "ne_outer") ||
            STR_IEQUALS(request_block->function, "Te") ||
            STR_IEQUALS(request_block->function, "Te_inner") ||
            STR_IEQUALS(request_block->function, "Te_outer") ||
            (STR_IEQUALS(request_block->function, "MP")) ||
            (STR_IEQUALS(request_block->function, "MF")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MPPrime")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MFFPrime")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MElong")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MTriangL")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MTriangU")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MVol")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MArea"))) {

            int handle = whichHandle("Rmag");

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Flux Surface Coordinate

            data_block->dims[0].dim_n = equimapdata.rhoCount;
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;

            data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
            memcpy(data_block->dims[0].dim, equimapdata.rho, data_block->dims[0].dim_n * sizeof(float));

            data_block->dims[0].compressed = 0;
            strcpy(data_block->dims[0].dim_units, "");

            switch (equimapdata.rhoType) {
                case SQRTNORMALISEDTOROIDALFLUX: {
                    strcpy(data_block->dims[0].dim_label, "sqrt(Normalised Toroidal Flux)");
                    break;
                }
                case NORMALISEDPOLOIDALFLUX: {
                    strcpy(data_block->dims[0].dim_label, "Normalised Poloidal Flux");
                    break;
                }
                case NORMALISEDITMFLUXRADIUS: {
                    strcpy(data_block->dims[0].dim_label, "Normalised ITM Toroidal Flux Radius");
                    break;
                }
            }

            // Collect data traces

            if (STR_IEQUALS(request_block->function, "R_inner") ||
                STR_IEQUALS(request_block->function, "R_outer")) {
                handle = whichHandle("ayc_r");
            } else if (STR_IEQUALS(request_block->function, "ne") ||
                       STR_IEQUALS(request_block->function, "ne_inner") ||
                       STR_IEQUALS(request_block->function, "ne_outer")) {
                handle = whichHandle("ayc_ne");
            } else if (STR_IEQUALS(request_block->function, "Te") ||
                       STR_IEQUALS(request_block->function, "Te_inner") ||
                       STR_IEQUALS(request_block->function, "Te_outer")) {
                handle = whichHandle("ayc_Te");
            } else if (STR_IEQUALS(request_block->function, "MPsi")) {
                handle = -1;
            } else if (STR_IEQUALS(request_block->function, "MQ")) {
                handle = whichHandle("Q");
            } else if (STR_IEQUALS(request_block->function, "MP")) {
                handle = whichHandle("P");
            } else if (STR_IEQUALS(request_block->function, "MF")) {
                handle = whichHandle("F");
            } else if (STR_IEQUALS(request_block->function, "MPPrime")) {
                handle = whichHandle("PPrime");
            } else if (STR_IEQUALS(request_block->function, "MFFPrime")) {
                handle = whichHandle("FFPrime");
            } else if (STR_IEQUALS(request_block->function, "MElong")) {
                handle = whichHandle("ElongPsi");
            } else if (STR_IEQUALS(request_block->function, "MTriangL")) {
                handle = whichHandle("TriangLPsi");
            } else if (STR_IEQUALS(request_block->function, "MTriangU")) {
                handle = whichHandle("TriangUPsi");
            } else if (STR_IEQUALS(request_block->function, "MVol")) {
                handle = whichHandle("VolPsi");
            } else if (STR_IEQUALS(request_block->function, "MArea")) {
                handle = whichHandle("AreaPsi");
            }

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            float* arr = (float*)data_block->data;

            int i;
            for (i = 0; i < equimapdata.timeCount; i++) {
                int j;
                for (j = 0; j < data_block->dims[0].dim_n; j++) {
                    int offset = i * data_block->dims[0].dim_n + j;
                    if (STR_IEQUALS(request_block->function, "R_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagr1[j];
                    } else if (STR_IEQUALS(request_block->function, "R_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagr2[j];
                    } else if (STR_IEQUALS(request_block->function, "ne")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagne[j];
                    } else if (STR_IEQUALS(request_block->function, "ne_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagne1[j];
                    } else if (STR_IEQUALS(request_block->function, "ne_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagne2[j];
                    } else if (STR_IEQUALS(request_block->function, "Te")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagte[j];
                    } else if (STR_IEQUALS(request_block->function, "Te_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagte1[j];
                    } else if (STR_IEQUALS(request_block->function, "Te_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagte2[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPsi")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagpsi[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPsi_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagpsi1[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPsi_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagpsi2[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPhi")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagphi[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPhi_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagphi1[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPhi_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagphi2[j];
                    } else if (STR_IEQUALS(request_block->function, "MPsi")) {
                        arr[offset] = equimapdata.efitdata[i].mappsi[j];
                    } else if (STR_IEQUALS(request_block->function, "MQ")) {
                        arr[offset] = equimapdata.efitdata[i].mapq[j];
                    } else if (STR_IEQUALS(request_block->function, "MP")) {
                        arr[offset] = equimapdata.efitdata[i].mapp[j];
                    } else if (STR_IEQUALS(request_block->function, "MF")) {
                        arr[offset] = equimapdata.efitdata[i].mapf[j];
                    } else if (STR_IEQUALS(request_block->function, "MPPrime")) {
                        arr[offset] = equimapdata.efitdata[i].mappprime[j];
                    } else if (STR_IEQUALS(request_block->function, "MFFPrime")) {
                        arr[offset] = equimapdata.efitdata[i].mapffprime[j];
                    } else if (STR_IEQUALS(request_block->function, "MElong")) {
                        arr[offset] = equimapdata.efitdata[i].mapelongp[j];
                    } else if (STR_IEQUALS(request_block->function, "MTriangL")) {
                        arr[offset] = equimapdata.efitdata[i].maptrianglp[j];
                    } else if (STR_IEQUALS(request_block->function, "MTriangU")) {
                        arr[offset] = equimapdata.efitdata[i].maptriangup[j];
                    } else if (STR_IEQUALS(request_block->function, "MVol")) {
                        arr[offset] = equimapdata.efitdata[i].mapvolp[j];
                    } else if (STR_IEQUALS(request_block->function, "MArea")) {
                        arr[offset] = equimapdata.efitdata[i].mapareap[j];
                    }
                }
            }

            if (handle >= 0) {
                strcpy(data_block->data_units, getIdamDataUnits(handle));
                strcpy(data_block->data_label, getIdamDataLabel(handle));
                strcpy(data_block->data_desc, getIdamDataDesc(handle));
            } else {
                if (STR_IEQUALS(request_block->function, "MPsi") ||
                    STR_IEQUALS(request_block->function, "MYPsi") ||
                    STR_IEQUALS(request_block->function, "MYPsi_inner") ||
                    STR_IEQUALS(request_block->function, "MYPsi_outer")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "psi");
                    strcpy(data_block->data_desc, "Poloidal Flux");
                } else if (STR_IEQUALS(request_block->function, "MYPhi") ||
                           STR_IEQUALS(request_block->function, "MYPhi_inner") ||
                           STR_IEQUALS(request_block->function, "MYPhi_outer")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "phi");
                    strcpy(data_block->data_desc, "Toroidal Flux");
                } else {
                    data_block->data_units[0] = '\0';
                    data_block->data_label[0] = '\0';
                    data_block->data_desc[0] = '\0';
                }
            }

            break;
        }

        // Experimental Data Mapped to Fixed Grid? (Surface-Points)

        if (STR_IEQUALS(request_block->function, "MPsib") ||
            STR_IEQUALS(request_block->function, "MQb") ||
            STR_IEQUALS(request_block->function, "MYPsib") ||
            STR_IEQUALS(request_block->function, "MYPsib_inner") ||
            STR_IEQUALS(request_block->function, "MYPsib_outer") ||
            STR_IEQUALS(request_block->function, "MYPhib") ||
            STR_IEQUALS(request_block->function, "MYPhib_inner") ||
            STR_IEQUALS(request_block->function, "MYPhib_outer") ||
            STR_IEQUALS(request_block->function, "Rb_inner") ||
            STR_IEQUALS(request_block->function, "Rb_outer") ||
            STR_IEQUALS(request_block->function, "neb") ||
            STR_IEQUALS(request_block->function, "neb_inner") ||
            STR_IEQUALS(request_block->function, "neb_outer") ||
            STR_IEQUALS(request_block->function, "Teb") ||
            STR_IEQUALS(request_block->function, "Teb_inner") ||
            STR_IEQUALS(request_block->function, "Teb_outer") ||
            (STR_IEQUALS(request_block->function, "MPB")) ||
            (STR_IEQUALS(request_block->function, "MFB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MPPrimeB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MFFPrimeB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MElongB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MTriangLB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MTriangUB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MVolB")) ||
            (equimapdata.readITMData && STR_IEQUALS(request_block->function, "MAreaB"))) {

            int handle = whichHandle("Rmag");

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Normalised SQRT Toroidal Flux Dimension

            data_block->dims[0].dim_n = equimapdata.rhoBCount;
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;

            data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
            memcpy(data_block->dims[0].dim, equimapdata.rhoB, data_block->dims[0].dim_n * sizeof(float));

            data_block->dims[0].compressed = 0;
            strcpy(data_block->dims[0].dim_units, "");
            strcpy(data_block->dims[0].dim_label, "sqrt(Normalised Toroidal Flux)");

            // Collect data traces

            if (STR_IEQUALS(request_block->function, "Rb_inner") ||
                STR_IEQUALS(request_block->function, "Rb_outer")) {
                handle = whichHandle("ayc_r");
            } else if (STR_IEQUALS(request_block->function, "neb") ||
                       STR_IEQUALS(request_block->function, "neb_inner") ||
                       STR_IEQUALS(request_block->function, "neb_outer")) {
                handle = whichHandle("ayc_ne");
            } else if (STR_IEQUALS(request_block->function, "Teb") ||
                       STR_IEQUALS(request_block->function, "Teb_inner") ||
                       STR_IEQUALS(request_block->function, "Teb_outer")) {
                handle = whichHandle("ayc_Te");
            } else if (STR_IEQUALS(request_block->function, "MPsiB")) {
                handle = -1;
            } else if (STR_IEQUALS(request_block->function, "MQB")) {
                handle = whichHandle("Q");
            } else if (STR_IEQUALS(request_block->function, "MPB")) {
                handle = whichHandle("P");
            } else if (STR_IEQUALS(request_block->function, "MFB")) {
                handle = whichHandle("B");
            } else if (STR_IEQUALS(request_block->function, "MPPrimeB")) {
                handle = whichHandle("PPrime");
            } else if (STR_IEQUALS(request_block->function, "MFFPrimeB")) {
                handle = whichHandle("FFPrime");
            } else if (STR_IEQUALS(request_block->function, "MElongB")) {
                handle = whichHandle("ElongPsi");
            } else if (STR_IEQUALS(request_block->function, "MTriangLB")) {
                handle = whichHandle("TriangLPsi");
            } else if (STR_IEQUALS(request_block->function, "MTriangUB")) {
                handle = whichHandle("TriangUPsi");
            } else if (STR_IEQUALS(request_block->function, "MVolB")) {
                handle = whichHandle("VolPsi");
            } else if (STR_IEQUALS(request_block->function, "MAreaB")) {
                handle = whichHandle("AreaPsi");
            }

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));

            float* arr = (float*)data_block->data;

            int i;
            for (i = 0; i < equimapdata.timeCount; i++) {
                int j;
                for (j = 0; j < data_block->dims[0].dim_n; j++) {
                    int offset = i * data_block->dims[0].dim_n + j;
                    if (STR_IEQUALS(request_block->function, "Rb_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagr1B[j];
                    } else if (STR_IEQUALS(request_block->function, "Rb_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagr2B[j];
                    } else if (STR_IEQUALS(request_block->function, "neb")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagneB[j];
                    } else if (STR_IEQUALS(request_block->function, "neb_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagne1B[j];
                    } else if (STR_IEQUALS(request_block->function, "neb_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagne2B[j];
                    } else if (STR_IEQUALS(request_block->function, "Teb")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagteB[j];
                    } else if (STR_IEQUALS(request_block->function, "Teb_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagte1B[j];
                    } else if (STR_IEQUALS(request_block->function, "Teb_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagte2B[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPsib")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagpsiB[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPsib_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagpsi1B[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPsib_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagpsi2B[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPhib")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagphiB[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPhib_inner")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagphi1B[j];
                    } else if (STR_IEQUALS(request_block->function, "MYPhib_outer")) {
                        arr[offset] = equimapdata.efitdata[i].mapyagphi2B[j];
                    } else if (STR_IEQUALS(request_block->function, "MPsib")) {
                        arr[offset] = equimapdata.efitdata[i].mappsiB[j];
                    } else if (STR_IEQUALS(request_block->function, "MQB")) {
                        arr[offset] = equimapdata.efitdata[i].mapqB[j];
                    } else if (STR_IEQUALS(request_block->function, "MPB")) {
                        arr[offset] = equimapdata.efitdata[i].mappB[j];
                    } else if (STR_IEQUALS(request_block->function, "MFB")) {
                        arr[offset] = equimapdata.efitdata[i].mapfB[j];
                    } else if (STR_IEQUALS(request_block->function, "MPPrimeB")) {
                        arr[offset] = equimapdata.efitdata[i].mappprimeB[j];
                    } else if (STR_IEQUALS(request_block->function, "MFFPrimeB")) {
                        arr[offset] = equimapdata.efitdata[i].mapffprimeB[j];
                    } else if (STR_IEQUALS(request_block->function, "MElongB")) {
                        arr[offset] = equimapdata.efitdata[i].mapelongpB[j];
                    } else if (STR_IEQUALS(request_block->function, "MTriangLB")) {
                        arr[offset] = equimapdata.efitdata[i].maptrianglpB[j];
                    } else if (STR_IEQUALS(request_block->function, "MTriangUB")) {
                        arr[offset] = equimapdata.efitdata[i].maptriangupB[j];
                    } else if (STR_IEQUALS(request_block->function, "MVolB")) {
                        arr[offset] = equimapdata.efitdata[i].mapvolpB[j];
                    } else if (STR_IEQUALS(request_block->function, "MAreaB")) {
                        arr[offset] = equimapdata.efitdata[i].mapareapB[j];
                    }
                }
            }

            if (handle >= 0) {
                strcpy(data_block->data_units, getIdamDataUnits(handle));
                strcpy(data_block->data_label, getIdamDataLabel(handle));
                strcpy(data_block->data_desc, getIdamDataDesc(handle));
            } else {
                if (STR_IEQUALS(request_block->function, "MPsiB") ||
                    STR_IEQUALS(request_block->function, "MYPsiB") ||
                    STR_IEQUALS(request_block->function, "MYPsiB_inner") ||
                    STR_IEQUALS(request_block->function, "MYPsiB_outer")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "psi");
                    strcpy(data_block->data_desc, "Poloidal Flux");
                } else if (STR_IEQUALS(request_block->function, "MYPhiB") ||
                           STR_IEQUALS(request_block->function, "MYPhiB_inner") ||
                           STR_IEQUALS(request_block->function, "MYPhiB_outer")) {
                    strcpy(data_block->data_units, "Wb");
                    strcpy(data_block->data_label, "phi");
                    strcpy(data_block->data_desc, "Toroidal Flux");
                } else {
                    data_block->data_units[0] = '\0';
                    data_block->data_label[0] = '\0';
                    data_block->data_desc[0] = '\0';
                }
            }

            break;
        }

        // Fixed Grids

        if (STR_IEQUALS(request_block->function, "FRho") || STR_IEQUALS(request_block->function, "FRhoB")) {

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 1;
            data_block->order = -1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            unsigned int i;
            for (i = 0; i < data_block->rank; i++) {
                initDimBlock(&data_block->dims[i]);
            }

            // Array Index

            if (STR_IEQUALS(request_block->function, "FRho")) {
                data_block->dims[0].dim_n = equimapdata.rhoCount;
            } else {
                data_block->dims[0].dim_n = equimapdata.rhoBCount;
            }
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;

            data_block->dims[0].dim = NULL;

            data_block->dims[0].compressed = 1;
            data_block->dims[0].dim0 = 0.0;
            data_block->dims[0].diff = 1.0;
            data_block->dims[0].method = 0;

            strcpy(data_block->dims[0].dim_units, "");
            strcpy(data_block->dims[0].dim_label, "Flux Surface Index");

            data_block->data_n = data_block->dims[0].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));
            if (STR_IEQUALS(request_block->function, "FRho"))
                memcpy((void*)data_block->data, (void*)equimapdata.rho, equimapdata.rhoCount * sizeof(float));
            else
                memcpy((void*)data_block->data, (void*)equimapdata.rhoB, equimapdata.rhoBCount * sizeof(float));

            strcpy(data_block->data_units, "");

            switch (equimapdata.rhoType) {
                case SQRTNORMALISEDTOROIDALFLUX: {
                    strcpy(data_block->data_desc, "Sqrt Normalised Toroidal Magnetic Flux at ");
                    break;
                }
                case NORMALISEDPOLOIDALFLUX: {
                    strcpy(data_block->data_desc, "Normalised Poloidal Magnetic Flux at ");
                    break;
                }
                case NORMALISEDITMFLUXRADIUS: {
                    strcpy(data_block->data_desc, "Normalised ITM Toroidal Flux Radius at ");
                    break;
                }
            }

            if (STR_IEQUALS(request_block->function, "FRho")) {
                strcpy(data_block->data_label, "Rho");
                strcat(data_block->data_desc, "Mid-Points");
            } else {
                strcpy(data_block->data_label, "RhoB");
                strcat(data_block->data_desc, "Surface-Points");
            }

            break;
        } else if (STR_IEQUALS(request_block->function, "Rho") || STR_IEQUALS(request_block->function, "RhoB")) {
            // Fixed Grids: Create rank 2 array rho[t][x]	// Array Shape: data[2][1][0]

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            int handle = whichHandle("EFM_MAGNETIC_AXIS_R");        // Provides Timing Labels only - not data

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 1) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Array Index

            if (STR_IEQUALS(request_block->function, "Rho")) {
                data_block->dims[0].dim_n = equimapdata.rhoCount;
            } else {
                data_block->dims[0].dim_n = equimapdata.rhoBCount;
            }
            data_block->dims[0].data_type = UDA_TYPE_FLOAT;

            data_block->dims[0].dim = NULL;

            data_block->dims[0].compressed = 1;
            data_block->dims[0].dim0 = 0.0;
            data_block->dims[0].diff = 1.0;
            data_block->dims[0].method = 0;

            strcpy(data_block->dims[0].dim_units, "");
            strcpy(data_block->dims[0].dim_label, "Flux Surface Index");

            // Data

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));
            float* arr = (float*)data_block->data;

            if (STR_IEQUALS(request_block->function, "Rho")) {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j;
                    for (j = 0; j < equimapdata.rhoCount; j++) {
                        int offset = i * equimapdata.rhoCount + j;
                        arr[offset] = equimapdata.rho[j];
                    }
                }
            } else {
                int i;
                for (i = 0; i < equimapdata.timeCount; i++) {
                    int j;
                    for (j = 0; j < equimapdata.rhoBCount; j++) {
                        int offset = i * equimapdata.rhoBCount + j;
                        arr[offset] = equimapdata.rhoB[j];
                    }
                }
            }

            switch (equimapdata.rhoType) {
                case SQRTNORMALISEDTOROIDALFLUX: {
                    strcpy(data_block->data_desc, "Sqrt Normalised Toroidal Magnetic Flux at ");
                    break;
                }
                case NORMALISEDPOLOIDALFLUX: {
                    strcpy(data_block->data_desc, "Normalised Poloidal Magnetic Flux at ");
                    break;
                }
                case NORMALISEDITMFLUXRADIUS: {
                    strcpy(data_block->data_desc, "Normalised ITM Toroidal Flux Radius at ");
                    break;
                }
            }

            strcpy(data_block->data_units, "");
            if (STR_IEQUALS(request_block->function, "Rho")) {
                strcpy(data_block->data_label, "Rho");
                strcat(data_block->data_desc, "Mid-Points");
            } else {
                strcpy(data_block->data_label, "RhoB");
                strcat(data_block->data_desc, "Surface-Points");
            }

            break;

        } else if (STR_IEQUALS(request_block->function, "mapgm0") ||
            STR_IEQUALS(request_block->function, "mapgm1") ||
            STR_IEQUALS(request_block->function, "mapgm2") ||
            STR_IEQUALS(request_block->function, "mapgm99") ||
            STR_IEQUALS(request_block->function, "mapgm3")) {

            // Rank 2 Flux Surface Average Data  [time][rho]
            // ************ //fluxSurfaceAverage();

            int handle = whichHandle("Rmag");        // Provides Timing Labels only - not data

            DATA_BLOCK* data_block = idam_plugin_interface->data_block;
            initDataBlock(data_block);
            data_block->rank = 2;
            data_block->order = 1;

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

            {
                unsigned int i;
                for (i = 0; i < data_block->rank; i++) {
                    initDimBlock(&data_block->dims[i]);
                }
            }

            // Time Dimension

            data_block->dims[1].dim_n = equimapdata.timeCount;
            data_block->dims[1].data_type = UDA_TYPE_FLOAT;
            data_block->dims[1].dim = malloc(data_block->dims[1].dim_n * sizeof(float));
            memcpy(data_block->dims[1].dim, equimapdata.times, data_block->dims[1].dim_n * sizeof(float));

            data_block->dims[1].compressed = 0;
            if (handle >= 0) {
                DIMS* tdim = getIdamDimBlock(handle, getIdamOrder(handle));
                strcpy(data_block->dims[1].dim_units, tdim->dim_units);
                strcpy(data_block->dims[1].dim_label, tdim->dim_label);
            } else {
                data_block->dims[1].dim_units[0] = '\0';
                data_block->dims[1].dim_label[0] = '\0';
            }

            // Flux surface label

            data_block->dims[0].data_type = UDA_TYPE_FLOAT;
            strcpy(data_block->dims[0].dim_units, "");
            strcpy(data_block->dims[0].dim_label, "Rho");
            data_block->dims[0].dim_n = equimapdata.rhoCount;
            data_block->dims[0].dim = malloc(data_block->dims[0].dim_n * sizeof(float));
            memcpy(data_block->dims[0].dim, equimapdata.rho, data_block->dims[0].dim_n * sizeof(float));
            data_block->dims[0].compressed = 0;

            switch (equimapdata.rhoType) {
                case SQRTNORMALISEDTOROIDALFLUX: {
                    strcpy(data_block->dims[0].dim_label, "sqrt(Normalised Toroidal Flux)");
                    break;
                }
                case NORMALISEDPOLOIDALFLUX: {
                    strcpy(data_block->dims[0].dim_label, "Normalised Poloidal Flux");
                    break;
                }
                case NORMALISEDITMFLUXRADIUS: {
                    strcpy(data_block->dims[0].dim_label, "Normalised ITM Toroidal Flux Radius");
                    break;
                }
            }

            // Data

            data_block->data_n = data_block->dims[0].dim_n * data_block->dims[1].dim_n;

            data_block->data_type = UDA_TYPE_FLOAT;
            data_block->data = malloc(data_block->data_n * sizeof(float));
            float* arr = (float*)data_block->data;

            int i;
            for (i = 0; i < equimapdata.timeCount; i++) {
                int j;
                for (j = 0; j < data_block->dims[0].dim_n; j++) {
                    int offset = i * data_block->dims[0].dim_n + j;
                    if (STR_IEQUALS(request_block->function, "mapgm0")) {
                        arr[offset] = equimapdata.fluxAverages[i].metrics.grho[j];
                    } else if (STR_IEQUALS(request_block->function, "mapgm1")) {
                        arr[offset] = equimapdata.fluxAverages[i].metrics.grho2[j];
                    } else if (STR_IEQUALS(request_block->function, "mapgm2")) {
                        arr[offset] = equimapdata.fluxAverages[i].metrics.gm2[j];
                    } else if (STR_IEQUALS(request_block->function, "mapgm3")) {
                        arr[offset] = equimapdata.fluxAverages[i].metrics.gm3[j];
                    }
                }
            }

            if (STR_IEQUALS(request_block->function, "mapgm0")) {
                strcpy(data_block->data_units, "m^-1");
                strcpy(data_block->data_label, "<|Grad Rho|>");
                strcpy(data_block->data_desc, "Flux Surface Average <|Grad Rho|>");

            } else if (STR_IEQUALS(request_block->function, "mapgm1")) {
                strcpy(data_block->data_units, "m^-2");
                strcpy(data_block->data_label, "<|Grad Rho|^2>");
                strcpy(data_block->data_desc, "Flux Surface Average <|Grad Rho|^2>");

            } else if (STR_IEQUALS(request_block->function, "mapgm2")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "<R>");
                strcpy(data_block->data_desc, "Flux Surface Average <R>");
            } else if (STR_IEQUALS(request_block->function, "mapgm3")) {
                strcpy(data_block->data_units, "m");
                strcpy(data_block->data_label, "<|Grad(Rho/R)|^2>");
                strcpy(data_block->data_desc, "Flux Surface Average <|Grad(Rho/R)|^2>");
            }

            break;
        }

        RAISE_PLUGIN_ERROR("Unknown function requested!");

    } while (0);

    return err;
}

//----------------------------------------------------------------------------------------
// Ping - am I here?
static int do_ping(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* str = "equimap pinged!";

    DATA_BLOCK* data_block = idam_plugin_interface->data_block;
    initDataBlock(data_block);
    data_block->rank = 1;
    data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));
    unsigned int i;
    for (i = 0; i < data_block->rank; i++) {
        initDimBlock(&data_block->dims[i]);
    }
    data_block->dims[0].data_type = UDA_TYPE_UNSIGNED_INT;
    data_block->dims[0].dim_n = strlen(str) + 1;
    data_block->dims[0].compressed = 1;
    data_block->dims[0].dim0 = 0.0;
    data_block->dims[0].diff = 1.0;
    data_block->dims[0].method = 0;
    data_block->data_n = strlen(str) + 1;
    data_block->data_type = UDA_TYPE_STRING;
    data_block->data = strdup(str);

    return 0;
}

//----------------------------------------------------------------------------------------
// Help: A Description of library functionality
static int do_help(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* help = "psiRZBox Enabled!";

    DATA_BLOCK* data_block = idam_plugin_interface->data_block;
    initDataBlock(data_block);
    data_block->rank = 1;
    data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));
    unsigned int i;
    for (i = 0; i < data_block->rank; i++) {
        initDimBlock(&data_block->dims[i]);
    }
    data_block->dims[0].data_type = UDA_TYPE_UNSIGNED_INT;
    data_block->dims[0].dim_n = strlen(help) + 1;
    data_block->dims[0].compressed = 1;
    data_block->dims[0].dim0 = 0.0;
    data_block->dims[0].diff = 1.0;
    data_block->dims[0].method = 0;
    data_block->data_n = strlen(help) + 1;
    data_block->data_type = UDA_TYPE_STRING;
    data_block->data = strdup(help);

    return 0;
}

void initEquiMapData()
{
    int i;
    equimapdata.exp_number = 0;
    equimapdata.timeCount = 0;
    equimapdata.readITMData = 0;
    equimapdata.times = NULL;
    equimapdata.rhoType = SQRTNORMALISEDTOROIDALFLUX;
    equimapdata.rhoBCount = COORDINATECOUNT;
    equimapdata.rhoCount = equimapdata.rhoBCount - 1;
    equimapdata.rho = NULL;
    equimapdata.rhoB = NULL;
    equimapdata.efitdata = NULL;
    equimapdata.fluxAverages = NULL;
    handleCount = 0;
    for (i = 0; i < MAXHANDLES; i++) handles[i] = -1;
}

void initEfitData(EFITDATA* efitdata)
{
    efitdata->psi_bnd = 0.0;
    efitdata->psi_mag = 0.0;
    efitdata->rmag = 0.0;
    efitdata->zmag = 0.0;
    efitdata->ip = 0.0;
    efitdata->bphi = 0.0;
    efitdata->bvac = 0.0;
    efitdata->rvac = 0.0;
    efitdata->Rmin = 0.0;
    efitdata->Rmax = 0.0;

    efitdata->rgeom = 0.0;
    efitdata->zgeom = 0.0;
    efitdata->aminor = 0.0;
    efitdata->triangL = 0.0;
    efitdata->triangU = 0.0;
    efitdata->elong = 0.0;

    efitdata->nlcfs = 0;
    efitdata->rlcfs = NULL;
    efitdata->zlcfs = NULL;

    efitdata->nlim = 0;
    efitdata->rlim = NULL;
    efitdata->zlim = NULL;

    efitdata->psiCount[0] = 0;
    efitdata->psiCount[1] = 0;
    efitdata->psig = NULL;
    efitdata->rgrid = NULL;
    efitdata->zgrid = NULL;

    efitdata->psiCountSR[0] = 0;
    efitdata->psiCountSR[1] = 0;
    efitdata->psigSR = NULL;
    efitdata->rgridSR = NULL;
    efitdata->zgridSR = NULL;

    efitdata->psiCountRZBox[0] = 0;
    efitdata->psiCountRZBox[1] = 0;
    efitdata->psigRZBox = NULL;
    efitdata->rgridRZBox = NULL;
    efitdata->zgridRZBox = NULL;

    efitdata->dpsidr = NULL;
    efitdata->dpsidz = NULL;
    efitdata->Br = NULL;
    efitdata->Bz = NULL;
    efitdata->Bphi = NULL;
    efitdata->Jphi = NULL;

    efitdata->rz0Count = 0;
    efitdata->psiz0 = NULL;
    efitdata->rz0 = NULL;
    efitdata->qCount = 0;
    efitdata->q = NULL;
    efitdata->p = NULL;
    efitdata->f = NULL;
    efitdata->rho = NULL;
    efitdata->psi = NULL;
    efitdata->phi = NULL;
    efitdata->trho = NULL;
    efitdata->rho_torb = 1.0;
    efitdata->rho_tor = NULL;

    efitdata->pprime = NULL;
    efitdata->ffprime = NULL;
    efitdata->elongp = NULL;
    efitdata->trianglp = NULL;
    efitdata->triangup = NULL;
    efitdata->volp = NULL;
    efitdata->areap = NULL;

    efitdata->nne = 0;
    efitdata->ne = NULL;
    efitdata->te = NULL;
    efitdata->rne = NULL;

    efitdata->yagpsi = NULL;
    efitdata->yagtrho = NULL;
    efitdata->yagprho = NULL;
    efitdata->yagrhotor = NULL;

    efitdata->mappsi = NULL;
    efitdata->mappsiB = NULL;
    efitdata->mapq = NULL;
    efitdata->mapqB = NULL;
    efitdata->mapp = NULL;
    efitdata->mappB = NULL;
    efitdata->mapf = NULL;
    efitdata->mapfB = NULL;

    efitdata->mapgm0 = NULL;
    efitdata->mapgm1 = NULL;
    efitdata->mapgm2 = NULL;
    efitdata->mapgm3 = NULL;

    efitdata->mappprime = NULL;
    efitdata->mappprimeB = NULL;
    efitdata->mapffprime = NULL;
    efitdata->mapffprimeB = NULL;
    efitdata->mapelongp = NULL;
    efitdata->mapelongpB = NULL;
    efitdata->maptrianglp = NULL;
    efitdata->maptrianglpB = NULL;

    efitdata->maptriangup = NULL;
    efitdata->maptriangupB = NULL;
    efitdata->mapvolp = NULL;
    efitdata->mapvolpB = NULL;
    efitdata->mapareap = NULL;
    efitdata->mapareapB = NULL;

    efitdata->mapyagne = NULL;
    efitdata->mapyagte = NULL;
    efitdata->mapyagpsi = NULL;
    efitdata->mapyagphi = NULL;
    efitdata->mapyagr1 = NULL;
    efitdata->mapyagne1 = NULL;
    efitdata->mapyagte1 = NULL;
    efitdata->mapyagpsi1 = NULL;
    efitdata->mapyagphi1 = NULL;
    efitdata->mapyagr2 = NULL;
    efitdata->mapyagne2 = NULL;
    efitdata->mapyagte2 = NULL;
    efitdata->mapyagpsi2 = NULL;
    efitdata->mapyagphi2 = NULL;
    efitdata->mapyagneB = NULL;
    efitdata->mapyagteB = NULL;
    efitdata->mapyagpsiB = NULL;
    efitdata->mapyagphiB = NULL;
    efitdata->mapyagr1B = NULL;
    efitdata->mapyagne1B = NULL;
    efitdata->mapyagte1B = NULL;
    efitdata->mapyagpsi1B = NULL;
    efitdata->mapyagphi1B = NULL;
    efitdata->mapyagr2B = NULL;
    efitdata->mapyagne2B = NULL;
    efitdata->mapyagte2B = NULL;
    efitdata->mapyagpsi2B = NULL;
    efitdata->mapyagphi2B = NULL;
}

void freeEquiMapData()
{
    int i, j;

    for (i = 0; i < equimapdata.timeCount; i++) {
        free(equimapdata.efitdata[i].rlim);
        free(equimapdata.efitdata[i].zlim);
        free(equimapdata.efitdata[i].rlcfs);
        free(equimapdata.efitdata[i].zlcfs);
        free(equimapdata.efitdata[i].rgrid);
        free(equimapdata.efitdata[i].zgrid);
        free(equimapdata.efitdata[i].rgridSR);
        free(equimapdata.efitdata[i].zgridSR);
        free(equimapdata.efitdata[i].rgridRZBox);
        free(equimapdata.efitdata[i].zgridRZBox);

        free(equimapdata.efitdata[i].psiz0);
        free(equimapdata.efitdata[i].rz0);
        free(equimapdata.efitdata[i].q);
        free(equimapdata.efitdata[i].p);
        free(equimapdata.efitdata[i].f);
        free(equimapdata.efitdata[i].rho);
        free(equimapdata.efitdata[i].psi);
        free(equimapdata.efitdata[i].phi);
        free(equimapdata.efitdata[i].trho);
        free(equimapdata.efitdata[i].rho_tor);

        free(equimapdata.efitdata[i].pprime);
        free(equimapdata.efitdata[i].ffprime);
        free(equimapdata.efitdata[i].elongp);
        free(equimapdata.efitdata[i].trianglp);
        free(equimapdata.efitdata[i].triangup);
        free(equimapdata.efitdata[i].volp);
        free(equimapdata.efitdata[i].areap);

        free(equimapdata.efitdata[i].ne);
        free(equimapdata.efitdata[i].te);
        free(equimapdata.efitdata[i].rne);
        free(equimapdata.efitdata[i].yagpsi);
        free(equimapdata.efitdata[i].yagphi);
        free(equimapdata.efitdata[i].yagtrho);
        free(equimapdata.efitdata[i].yagprho);
        free(equimapdata.efitdata[i].yagrhotor);
        free(equimapdata.efitdata[i].mappsi);
        free(equimapdata.efitdata[i].mappsiB);
        free(equimapdata.efitdata[i].mapq);
        free(equimapdata.efitdata[i].mapqB);
        free(equimapdata.efitdata[i].mapp);
        free(equimapdata.efitdata[i].mappB);
        free(equimapdata.efitdata[i].mapf);
        free(equimapdata.efitdata[i].mapfB);
        free(equimapdata.efitdata[i].mappprime);
        free(equimapdata.efitdata[i].mappprimeB);
        free(equimapdata.efitdata[i].mapffprime);
        free(equimapdata.efitdata[i].mapffprimeB);
        free(equimapdata.efitdata[i].mapelongp);
        free(equimapdata.efitdata[i].mapelongpB);
        free(equimapdata.efitdata[i].maptrianglp);
        free(equimapdata.efitdata[i].maptrianglpB);
        free(equimapdata.efitdata[i].maptriangup);
        free(equimapdata.efitdata[i].maptriangupB);
        free(equimapdata.efitdata[i].mapvolp);
        free(equimapdata.efitdata[i].mapvolpB);
        free(equimapdata.efitdata[i].mapareap);
        free(equimapdata.efitdata[i].mapareapB);
        free(equimapdata.efitdata[i].mapgm0);
        free(equimapdata.efitdata[i].mapgm1);
        free(equimapdata.efitdata[i].mapgm2);
        free(equimapdata.efitdata[i].mapgm3);

        free(equimapdata.efitdata[i].mapyagne);
        free(equimapdata.efitdata[i].mapyagte);
        free(equimapdata.efitdata[i].mapyagpsi);
        free(equimapdata.efitdata[i].mapyagphi);
        free(equimapdata.efitdata[i].mapyagr1);
        free(equimapdata.efitdata[i].mapyagne1);
        free(equimapdata.efitdata[i].mapyagte1);
        free(equimapdata.efitdata[i].mapyagpsi1);
        free(equimapdata.efitdata[i].mapyagphi1);
        free(equimapdata.efitdata[i].mapyagr2);
        free(equimapdata.efitdata[i].mapyagne2);
        free(equimapdata.efitdata[i].mapyagte2);
        free(equimapdata.efitdata[i].mapyagpsi2);
        free(equimapdata.efitdata[i].mapyagphi2);
        free(equimapdata.efitdata[i].mapyagneB);
        free(equimapdata.efitdata[i].mapyagteB);
        free(equimapdata.efitdata[i].mapyagpsiB);
        free(equimapdata.efitdata[i].mapyagphiB);
        free(equimapdata.efitdata[i].mapyagr1B);
        free(equimapdata.efitdata[i].mapyagne1B);
        free(equimapdata.efitdata[i].mapyagte1B);
        free(equimapdata.efitdata[i].mapyagpsi1B);
        free(equimapdata.efitdata[i].mapyagphi1B);
        free(equimapdata.efitdata[i].mapyagr2B);
        free(equimapdata.efitdata[i].mapyagne2B);
        free(equimapdata.efitdata[i].mapyagte2B);
        free(equimapdata.efitdata[i].mapyagpsi2B);
        free(equimapdata.efitdata[i].mapyagphi2B);

        for (j = 0; j < equimapdata.efitdata[i].psiCount[1]; j++) {
            free(equimapdata.efitdata[i].psig[j]);
        }
        free(equimapdata.efitdata[i].psig);

        for (j = 0; j < equimapdata.efitdata[i].psiCountSR[1]; j++) {
            free(equimapdata.efitdata[i].psigSR[j]);
        }
        free(equimapdata.efitdata[i].psigSR);

        for (j = 0; j < equimapdata.efitdata[i].psiCountRZBox[1]; j++) {
            free(equimapdata.efitdata[i].psigRZBox[j]);
        }
        free(equimapdata.efitdata[i].psigRZBox);

        for (j = 0; j < equimapdata.efitdata[i].psiCount[1]; j++) {
            free(equimapdata.efitdata[i].Jphi[j]);
        }
        free(equimapdata.efitdata[i].Jphi);

        if (equimapdata.readITMData) {
            for (j = 0; j < equimapdata.efitdata[i].psiCount[1]; j++) {
                free(equimapdata.efitdata[i].Br[j]);
                free(equimapdata.efitdata[i].Bz[j]);
                free(equimapdata.efitdata[i].Bphi[j]);
            }
            free(equimapdata.efitdata[i].Br);
            free(equimapdata.efitdata[i].Bz);
            free(equimapdata.efitdata[i].Bphi);
        }

        initEfitData(&equimapdata.efitdata[i]);

        if (equimapdata.readITMData) {

            if (equimapdata.fluxAverages == NULL) continue;

            if (equimapdata.fluxAverages[i].contours != NULL) {
                for (j = 0; j < equimapdata.rhoCount; j++) {
                    if (equimapdata.fluxAverages[i].contours[j].rcontour != NULL) {
                        free((void*)equimapdata.fluxAverages[i].contours[j].rcontour);
                    }
                    if (equimapdata.fluxAverages[i].contours[j].zcontour != NULL) {
                        free((void*)equimapdata.fluxAverages[i].contours[j].zcontour);
                    }
                }
                free((void*)equimapdata.fluxAverages[i].contours);
            }

            if (equimapdata.fluxAverages[i].scrunch != NULL) {
                for (j = 0; j < equimapdata.rhoCount; j++) {
                    if (equimapdata.fluxAverages[i].scrunch[j].rcos != NULL) {
                        free((void*)equimapdata.fluxAverages[i].scrunch[j].rcos);
                    }
                    if (equimapdata.fluxAverages[i].scrunch[j].rsin != NULL) {
                        free((void*)equimapdata.fluxAverages[i].scrunch[j].rsin);
                    }
                    if (equimapdata.fluxAverages[i].scrunch[j].zcos != NULL) {
                        free((void*)equimapdata.fluxAverages[i].scrunch[j].zcos);
                    }
                    if (equimapdata.fluxAverages[i].scrunch[j].zsin != NULL) {
                        free((void*)equimapdata.fluxAverages[i].scrunch[j].zsin);
                    }
                }
                free((void*)equimapdata.fluxAverages[i].scrunch);
            }

            for (j = 0; j < equimapdata.rhoCount; j++) {
                if (equimapdata.fluxAverages[i].metrics.drcosdrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.drcosdrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.dzcosdrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.dzcosdrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.drsindrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.drsindrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.dzsindrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.dzsindrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.r[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.r[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.z[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.z[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.drdrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.drdrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.dzdrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.dzdrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.drdtheta[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.drdtheta[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.dzdtheta[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.dzdtheta[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.d2[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.d2[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.gradrho[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.gradrho[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.gradrhoR2[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.gradrhoR2[j]);
                }
                if (equimapdata.fluxAverages[i].metrics.dvdtheta[j] != NULL) {
                    free((void*)equimapdata.fluxAverages[i].metrics.dvdtheta[j]);
                }
            }

            if (equimapdata.fluxAverages[i].metrics.drcosdrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.drcosdrho);
            }
            if (equimapdata.fluxAverages[i].metrics.dzcosdrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.dzcosdrho);
            }
            if (equimapdata.fluxAverages[i].metrics.drsindrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.drsindrho);
            }
            if (equimapdata.fluxAverages[i].metrics.dzsindrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.dzsindrho);
            }
            if (equimapdata.fluxAverages[i].metrics.r != NULL)free((void*)equimapdata.fluxAverages[i].metrics.r);
            if (equimapdata.fluxAverages[i].metrics.z != NULL)free((void*)equimapdata.fluxAverages[i].metrics.z);
            if (equimapdata.fluxAverages[i].metrics.drdrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.drdrho);
            }
            if (equimapdata.fluxAverages[i].metrics.dzdrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.dzdrho);
            }
            if (equimapdata.fluxAverages[i].metrics.drdtheta != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.drdtheta);
            }
            if (equimapdata.fluxAverages[i].metrics.dzdtheta != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.dzdtheta);
            }
            if (equimapdata.fluxAverages[i].metrics.d2 != NULL)free((void*)equimapdata.fluxAverages[i].metrics.d2);
            if (equimapdata.fluxAverages[i].metrics.gradrho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.gradrho);
            }
            if (equimapdata.fluxAverages[i].metrics.gradrhoR2 != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.gradrhoR2);
            }
            if (equimapdata.fluxAverages[i].metrics.dvdtheta != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.dvdtheta);
            }

            if (equimapdata.fluxAverages[i].metrics.vprime != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.vprime);
            }
            if (equimapdata.fluxAverages[i].metrics.xaprime != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.xaprime);
            }
            free(equimapdata.fluxAverages[i].metrics.len);
            free(equimapdata.fluxAverages[i].metrics.sur);
            if (equimapdata.fluxAverages[i].metrics.grho != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.grho);
            }
            if (equimapdata.fluxAverages[i].metrics.grho2 != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.grho2);
            }
            if (equimapdata.fluxAverages[i].metrics.volume != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.volume);
            }
            if (equimapdata.fluxAverages[i].metrics.xarea != NULL) {
                free((void*)equimapdata.fluxAverages[i].metrics.xarea);
            }
            free(equimapdata.fluxAverages[i].metrics.gm2);
            free(equimapdata.fluxAverages[i].metrics.gm3);
        }
    }

    free(equimapdata.times);
    free(equimapdata.rho);
    free(equimapdata.rhoB);
    free(equimapdata.efitdata);
    free(equimapdata.fluxAverages);

    for (i = 0; i < handleCount; i++) {
        idamFree(handles[i]);        // Free IDAM Heap
    }

    initEquiMapData();
}
