// Plugin Template

#ifndef IdamIMASMDSPluginInclude
#define IdamIMASMDSPluginInclude

// Change History:
//
// 26Aug2015	dgm	Original imas.h Version with additions for imas_mds mdsplus components

#include <idamplugin.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THISPLUGIN_VERSION                  1
#define THISPLUGIN_MAX_INTERFACE_VERSION    1        // Interface versions higher than this will not be understood!
#define THISPLUGIN_DEFAULT_METHOD           "help"

#define FILEISNEW       1
#define FILEISOPEN      2
#define FILEISCLOSED    3

#define PUT_OPERATION               0
#define ISTIMED_OPERATION           1
#define PUTSLICE_OPERATION          2
#define REPLACELASTSLICE_OPERATION  3

#define GET_OPERATION               0
#define GETSLICE_OPERATION          1
#define GETDIMENSION_OPERATION      2

#define CLOSEST_SAMPLE  1
#define PREVIOUS_SAMPLE 2
#define INTERPOLATION   3

#define UNKNOWN_TYPE    0
#define STRING          1        // Must be the same as the client for consistency
#define INT             2
#define FLOAT           3
#define DOUBLE          4

#define STRING_VECTOR   5
//#define DIMENSION       -1    // if we need to read the dimensions only

#define TIMEBASEPATHLENGTH      256

#define ERROR_RETURN_VALUE      1
#define OK_RETURN_VALUE         0

extern int imas_mds(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);

typedef struct PluginArgs {
    unsigned short isCPOPath;
    char* CPOPath;
    unsigned short isPath;
    char* path;
    unsigned short isTypeName;
    char* typeName;
    unsigned short isClientIdx;
    int clientIdx;
    unsigned short isClientObjectId;
    int clientObjectId;
    unsigned short isRank;
    int rank;
    unsigned short isIndex;
    int index;
    unsigned short isCount;
    int count;
    unsigned short isShapeString;
    char* shapeString;
    unsigned short isDataString;
    char* dataString;
    unsigned short isFileName;
    char* filename;
    unsigned short isShotNumber;
    int shotNumber;
    unsigned short isRunNumber;
    int runNumber;
    unsigned short isRefShotNumber;
    int refShotNumber;
    unsigned short isRefRunNumber;
    int refRunNumber;
    unsigned short isTimedArg;
    int isTimed;
    unsigned short isInterpolMode;
    int interpolMode;
    unsigned short isSignal;
    char* signal;
    unsigned short isSource;
    char* source;
    unsigned short isFormat;
    char* format;
    unsigned short isOwner;
    char* owner;
    unsigned short isServer;
    char* server;
    unsigned short isSetLevel;
    int setLevel;
    unsigned short isCommand;
    char* command;
    unsigned short isIPAddress;
    char* IPAddress;
    unsigned short isTimes;
    char* timesString;
    unsigned short isPutDataSlice;
    unsigned short isReplaceLastDataSlice;
    unsigned short isGetDataSlice;
    unsigned short isGetDimension;
    unsigned short isCreateFromModel;
    unsigned short isFlush;
    unsigned short isDiscard;
    unsigned short isGetLevel;
    unsigned short isFlushCPO;
    unsigned short isDisable;
    unsigned short isEnable;
    unsigned short isBeginIDSSlice;
    unsigned short isEndIDSSlice;
    unsigned short isReplaceIDSSlice;
    unsigned short isBeginIDS;
    unsigned short isEndIDS;
    unsigned short isBeginIDSTimed;
    unsigned short isEndIDSTimed;
    unsigned short isBeginIDSNonTimed;
    unsigned short isEndIDSNonTimed;
    char quote;
    char delimiter;
    unsigned short isPutData;
    unsigned short isImasIdsVersion;
    char* imasIdsVersion;
    unsigned short isImasIdsDevice;
    char* imasIdsDevice;

} PLUGIN_ARGS;

char* getImasIdsVersion();

char* getImasIdsDevice();

void putImasIdsVersion(const char* version);

void putImasIdsDevice(const char* device);

int imas_mds_putDataSlice(int idx, char* cpoPath, char* path, char* timeBasePath, int type, int nDims,
                          int* dims, void* data, double time);

int imas_mds_replaceLastDataSlice(int idx, char* cpoPath, char* path, int type, int nDims, int* dims,
                                  void* data);

int imas_mds_putData(int idx, char* cpoPath, char* path, int type, int nDims, int* dims, int isTimed,
                     void* data, double time);

int imas_mds_putDataX(int idx, char* cpoPath, char* path, int type, int nDims, int* dims, int operation,
                      void* data, double time);

int imas_mds_getData(int idx, char* cpoPath, char* path, int type, int nDims, int* dims, void** dataOut);

int imas_mds_getDataSlices(int idx, char* cpoPath, char* path, int type, int rank, int* shape, void** data,
                           double time, double* retTime, int interpolMode);

void* imas_mds_putDataSliceInObject(void* obj, char* path, int index, int type, int nDims, int* dims,
                                    void* data);

int imas_mds_getDataSliceInObject(void* obj, char* path, int index, int type, int nDims, int* dims,
                                  void** data);

void putTimeBasePath(char* timeBasePath);

char* getTimeBasePath();

int getSliceIdxs(int expIdx, char* cpoPath, double time);

void setSliceIdx(int index1, int index2);

void setSliceTime(double time1, double time2);

#ifdef __cplusplus
}
#endif

#endif
