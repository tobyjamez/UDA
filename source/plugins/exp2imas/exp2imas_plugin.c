#include "exp2imas_plugin.h"

#include <string.h>
#include <assert.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <clientserver/initStructs.h>
#include <clientserver/udaTypes.h>
#include <logging/logging.h>
#include <clientserver/errorLog.h>
#include <clientserver/stringUtils.h>
#include <plugins/udaPlugin.h>

#include "exp2imas_mds.h"
#include "exp2imas_xml.h"

enum MAPPING_TYPE {
    NONE, CONSTANT, STATIC, DYNAMIC
};

static int do_help(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static int do_version(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static int do_builddate(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static int do_defaultmethod(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static int do_maxinterfaceversion(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static int do_read(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
static char* getMappingFileName(const char* IDSversion);
static char* getMachineMappingFileName(const char* element);
static xmlChar* getMappingValue(const char* mapping_file_name, const char* request, int* request_type);
static char* deblank(char* token);

#ifndef strndup

char*
strndup(const char* s, size_t n)
{
    char* result;
    size_t len = strlen(s);

    if (n < len) {
        len = n;
    }

    result = (char*)malloc(len + 1);
    if (!result) {
        return 0;
    }

    result[len] = '\0';
    return (char*)memcpy(result, s, len);
}

#endif

int exp2imasPlugin(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    // ----------------------------------------------------------------------------------------
    // Standard v1 Plugin Interface

    if (idam_plugin_interface->interfaceVersion > THISPLUGIN_MAX_INTERFACE_VERSION) {
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown to this plugin: Unable to execute the request!");
    }

    idam_plugin_interface->pluginVersion = THISPLUGIN_VERSION;

    REQUEST_BLOCK* request_block = idam_plugin_interface->request_block;

    static short init = 0;

    // ----------------------------------------------------------------------------------------
    // Heap Housekeeping

    if (idam_plugin_interface->housekeeping || STR_IEQUALS(request_block->function, "reset")) {

        if (!init) {
            return 0;
        }    // Not previously initialised: Nothing to do!

        // Free Heap & reset counters

        init = 0;

        return 0;
    }

    // ----------------------------------------------------------------------------------------
    // Initialise

    if (!init || STR_IEQUALS(request_block->function, "init")
        || STR_IEQUALS(request_block->function, "initialise")) {

        init = 1;
        if (STR_IEQUALS(request_block->function, "init")
            || STR_IEQUALS(request_block->function, "initialise")) {
            return 0;
        }
    }

    // ----------------------------------------------------------------------------------------
    // Plugin Functions
    // ----------------------------------------------------------------------------------------

    int err = 0;

    if (STR_IEQUALS(request_block->function, "help")) {
        err = do_help(idam_plugin_interface);
    } else if (STR_IEQUALS(request_block->function, "version")) {
        err = do_version(idam_plugin_interface);
    } else if (STR_IEQUALS(request_block->function, "builddate")) {
        err = do_builddate(idam_plugin_interface);
    } else if (STR_IEQUALS(request_block->function, "defaultmethod")) {
        err = do_defaultmethod(idam_plugin_interface);
    } else if (STR_IEQUALS(request_block->function, "maxinterfaceversion")) {
        err = do_maxinterfaceversion(idam_plugin_interface);
    } else if (STR_IEQUALS(request_block->function, "read")) {
        err = do_read(idam_plugin_interface);
    } else {
        RAISE_PLUGIN_ERROR("Unknown function requested!");
    }

    return err;
}

// Help: A Description of library functionality
int do_help(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* help = "\ntsPlugin: this plugin maps Tore Supra data to IDS\n\n";
    const char* desc = "tsPlugin: help = plugin used for mapping Tore Supra experimental data to IDS";

    return setReturnDataString(idam_plugin_interface->data_block, help, desc);
}

int do_version(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* desc = "Plugin version number";

    return setReturnDataIntScalar(idam_plugin_interface->data_block, THISPLUGIN_VERSION, desc);
}

// Plugin Build Date
int do_builddate(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* desc = "Plugin build date";

    return setReturnDataString(idam_plugin_interface->data_block, __DATE__, desc);
}

// Plugin Default Method
int do_defaultmethod(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* desc = "Plugin default method";

    return setReturnDataString(idam_plugin_interface->data_block, THISPLUGIN_DEFAULT_METHOD, desc);
}

// Plugin Maximum Interface Version
int do_maxinterfaceversion(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    const char* desc = "Maximum Interface Version";

    return setReturnDataIntScalar(idam_plugin_interface->data_block, THISPLUGIN_MAX_INTERFACE_VERSION, desc);
}

// ----------------------------------------------------------------------------------------
// Add functionality here ....
int do_read(IDAM_PLUGIN_INTERFACE* idam_plugin_interface)
{
    int err = 0;

    DATA_BLOCK* data_block = idam_plugin_interface->data_block;

    initDataBlock(data_block);

    data_block->rank = 1;
    data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));

    int i;
    for (i = 0; i < data_block->rank; i++) {
        initDimBlock(&data_block->dims[i]);
    }

    REQUEST_BLOCK* request_block = idam_plugin_interface->request_block;

    const char* element = NULL;
    FIND_REQUIRED_STRING_VALUE(request_block->nameValueList, element);

    int shot = 0;
    FIND_REQUIRED_INT_VALUE(request_block->nameValueList, shot);

    int* indices = NULL;
    size_t nindices = 0;
    FIND_REQUIRED_INT_ARRAY(request_block->nameValueList, indices);

    const char* IDS_version = NULL;
    FIND_REQUIRED_STRING_VALUE(request_block->nameValueList, IDS_version);

    // Search mapping value and request type (static or dynamic)
    char* experiment_mapping_file_name = getMachineMappingFileName(element);
    char* mapping_file_name = getMappingFileName(IDS_version);

    int request_type;
    const xmlChar* xPath = getMappingValue(mapping_file_name, element, &request_type);

    if (xPath == NULL) {
        return -1;
    }

    if (request_type == STATIC) {

        // Executing XPath

        char* data;
        int data_type;
        int time_dim;
        int size;

        int status = execute_xpath_expression(experiment_mapping_file_name, xPath, indices, &data, &data_type, &time_dim, &size);
        if (status != 0) {
            return status;
        }

        if (data_type == TYPE_DOUBLE) {
            double* ddata = (double*)data;
            if (indices[0] > 0) {
                setReturnDataDblScalar(data_block, ddata[indices[0] - 1], NULL);
            } else {
                setReturnDataDblScalar(data_block, ddata[0], NULL);
            }
            free(data);
        } else if (data_type == TYPE_FLOAT) {
            float* fdata = (float*)data;
            if (indices[0] > 0) {
                setReturnDataFltScalar(data_block, fdata[indices[0] - 1], NULL);
            } else {
                setReturnDataFltScalar(data_block, fdata[0], NULL);
            }
            free(data);
        } else if (data_type == TYPE_LONG) {
            long* ldata = (long*)data;
            if (indices[0] > 0) {
                setReturnDataLongScalar(data_block, ldata[indices[0] - 1], NULL);
            } else {
                setReturnDataLongScalar(data_block, ldata[0], NULL);
            }
            free(data);
        } else if (data_type == TYPE_INT) {
            int* idata = (int*)data;
            if (indices[0] > 0) {
                setReturnDataIntScalar(data_block, idata[indices[0] - 1], NULL);
            } else {
                setReturnDataIntScalar(data_block, idata[0], NULL);
            }
            free(data);
        } else if (data_type == TYPE_SHORT) {
            short* sdata = (short*)data;
            if (indices[0] > 0) {
                setReturnDataShortScalar(data_block, sdata[indices[0] - 1], NULL);
            } else {
                setReturnDataShortScalar(data_block, sdata[0], NULL);
            }
            free(data);
        } else if (data_type == TYPE_STRING) {
            char** sdata = (char**)data;
            if (indices[0] > 0) {
                setReturnDataString(data_block, deblank(strdup(sdata[indices[0] - 1])), NULL);
            } else {
                setReturnDataString(data_block, deblank(strdup(sdata[0])), NULL);
            }
            FreeSplitStringTokens((char***)&data);
        } else {
            err = 999;
            addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, err, "Unsupported data type");
        }

        return 0;

    } else {

        // DYNAMIC case

        char* data;
        int data_type;
        int time_dim;
        int size;

        int status = execute_xpath_expression(experiment_mapping_file_name, xPath, indices, &data, &data_type, &time_dim, &size);
        if (status != 0) {
            return status;
        }

        if (data_type == TYPE_STRING) {
            // data_block->data contains MDS+ signal name -- use to get
            // data from MDS+ server
            char* signalName = *(char**)data;

            float* time;
            int len;
            float* fdata;
            status = mds_get(signalName, shot, &time, &fdata, &len, time_dim);

            if (status != 0) {
                return status;
            }

            free(data_block->dims);
            data_block->dims = NULL;

            int data_n = len / size;

            if (StringEndsWith(element, "/time")) {
                data_block->rank = 1;
                data_block->data_type = TYPE_FLOAT;
                data_block->data_n = data_n;
                data_block->data = (char*)time;
            } else {
                data_block->rank = 1;
                data_block->data_type = TYPE_FLOAT;
                data_block->data_n = data_n;
                if (indices[0] > 0) {
                    data_block->data = (char*)(fdata + (indices[0] - 1) *  data_n);
                } else {
                    data_block->data = (char*)fdata;
                }
            }

            data_block->dims = (DIMS*)malloc(data_block->rank * sizeof(DIMS));
            for (i = 0; i < data_block->rank; i++) {
                initDimBlock(&data_block->dims[i]);
            }

            data_block->dims[0].data_type = TYPE_FLOAT;
            data_block->dims[0].dim_n = data_n;
            data_block->dims[0].compressed = 0;
            data_block->dims[0].dim = (char*)time;

            strcpy(data_block->data_label, "");
            strcpy(data_block->data_units, "");
            strcpy(data_block->data_desc, "");
        } else {
            err = 999;
            addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, err, "Unsupported data type");
        }
    }

    return 0;
}

char* getMappingFileName(const char* IDSversion)
{
    char* dir = getenv("UDA_EXP2IMAS_MAPPING_FILE_DIRECTORY");
    return FormatString("%s/IMAS_mapping.xml", dir);
//    return FormatString("%s/IMAS_mapping_%s.xml", dir, IDSversion);
}

char* getMachineMappingFileName(const char* element)
{
    char* dir = getenv("UDA_EXP2IMAS_MAPPING_FILE_DIRECTORY");

    char* slash = strchr(element, '/');
    char* token = strndup(element, slash - element);

    char* name = FormatString("%s/JET_%s.xml", dir, token);
    free(token);

    return name;
}

xmlChar* getMappingValue(const char* mapping_file_name, const char* request, int* request_type)
{
    /*
     * Load XML document
     */
    xmlDocPtr doc = xmlParseFile(mapping_file_name);
    if (doc == NULL) {
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, 999, "unable to parse file");
        IDAM_LOGF(UDA_LOG_ERROR, "unable to parse file \"%s\"\n", mapping_file_name);
        return NULL;
    }

    /*
     * Create xpath evaluation context
     */
    xmlXPathContextPtr xpath_ctx = xmlXPathNewContext(doc);
    if (xpath_ctx == NULL) {
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, 999, "unable to create new XPath context");
        IDAM_LOGF(UDA_LOG_ERROR, "unable to create new XPath context\n", mapping_file_name);
        xmlFreeDoc(doc);
        return NULL;
    }
    // Creating the Xpath request

    XML_FMT_TYPE fmt = "//mapping[@key='%s']/@value";
    size_t len = strlen(request) + strlen(fmt) + 1;
    xmlChar* xpath_expr = malloc(len + sizeof(xmlChar));
    xmlStrPrintf(xpath_expr, (int)len, fmt, request);

    /*
     * Evaluate xpath expression for the type
     */
    xmlXPathObjectPtr xpath_obj = xmlXPathEvalExpression(xpath_expr, xpath_ctx);
    if (xpath_obj == NULL) {
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, 999, "unable to evaluate xpath expression");
        IDAM_LOGF(UDA_LOG_ERROR, "unable to evaluate xpath expression \"%s\"\n", xpath_expr);
        xmlXPathFreeContext(xpath_ctx);
        xmlFreeDoc(doc);
        return NULL;
    }

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;
    xmlChar* value = NULL;

    xmlNodePtr current_node = NULL;
    int err = 0;

    if (size != 0) {
        current_node = nodes->nodeTab[0];
        current_node = current_node->children;
        value = xmlStrdup(current_node->content);
    } else {
        err = 998;
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, err, "no result on XPath request, no key attribute defined?");
    }

    fmt = "//mapping[@key='%s']/@type";
    xmlStrPrintf(xpath_expr, (int)len, fmt, request);

    /*
     * Evaluate xpath expression for the type
     */
    xpath_obj = xmlXPathEvalExpression(xpath_expr, xpath_ctx);
    if (xpath_obj == NULL) {
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, 999, "unable to evaluate xpath expression");
        IDAM_LOGF(UDA_LOG_ERROR, "unable to evaluate xpath expression \"%s\"\n", xpath_expr);
        xmlXPathFreeContext(xpath_ctx);
        xmlFreeDoc(doc);
        return NULL;
    }

    nodes = xpath_obj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    char* type_str = NULL;

    if (size != 0) {
        current_node = nodes->nodeTab[0];
        current_node = current_node->children;
        type_str = strdup((char*)current_node->content);
    } else {
        err = 998;
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, err, "no result on XPath request, no key attribute defined?");
    }

    if (type_str == NULL) {
        *request_type = NONE;
    } else if (STR_IEQUALS(type_str, "constant")) {
        *request_type = CONSTANT;
    } else if (STR_IEQUALS(type_str, "dynamic")) {
        *request_type = DYNAMIC;
    } else if (STR_IEQUALS(type_str, "static")) {
        *request_type = STATIC;
    } else {
        addIdamError(&idamerrorstack, CODEERRORTYPE, __func__, 999, "unknown mapping type");
        IDAM_LOGF(UDA_LOG_ERROR, "unknown mapping type \"%s\"\n", type_str);
        value = NULL;
    }

    /*
     * Cleanup
     */
    xmlXPathFreeObject(xpath_obj);
    xmlXPathFreeContext(xpath_ctx);
    xmlFreeDoc(doc);

    return value;
}

char* deblank(char* input)
{
    int i, j;
    char* output = input;
    for (i = 0, j = 0; i < strlen(input); i++, j++) {
        if (input[i] != ' ' && input[i] != '\'') {
            output[j] = input[i];
        } else {
            j--;
        }
    }
    output[j] = 0;
    return output;
}
