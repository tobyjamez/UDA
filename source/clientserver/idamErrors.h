//
// Created by Jonathan Hollocombe on 17/10/2016.
//

#ifndef IDAM_IDAMERRORS_H
#define IDAM_IDAMERRORS_H

//-------------------------------------------------------
// Request specific errors

#define SIGNAL_ARG_TOO_LONG         18000
#define SOURCE_ARG_TOO_LONG         18001
#define ARCHIVE_NAME_TOO_LONG       18002
#define DEVICE_NAME_TOO_LONG        18003
#define NO_SERVER_SPECIFIED         18004

//-------------------------------------------------------
// Fatal Server Errors

#define FATAL_ERROR_LOGS    666

//-------------------------------------------------------
// Client Server Conversaton Protocol XDR Errors

#define PROTOCOL_ERROR_1    1
#define PROTOCOL_ERROR_2    2
#define PROTOCOL_ERROR_3    3
#define PROTOCOL_ERROR_4    4
#define PROTOCOL_ERROR_5    5
#define PROTOCOL_ERROR_61   61
#define PROTOCOL_ERROR_62   62
#define PROTOCOL_ERROR_63   63
#define PROTOCOL_ERROR_64   64
#define PROTOCOL_ERROR_65   65
#define PROTOCOL_ERROR_7    7
#define PROTOCOL_ERROR_8    8
#define PROTOCOL_ERROR_9    9
#define PROTOCOL_ERROR_10   10
#define PROTOCOL_ERROR_11   11
#define PROTOCOL_ERROR_12   12
#define PROTOCOL_ERROR_13   13
#define PROTOCOL_ERROR_14   14
#define PROTOCOL_ERROR_15   15
#define PROTOCOL_ERROR_16   16
#define PROTOCOL_ERROR_17   17
#define PROTOCOL_ERROR_18   18
#define PROTOCOL_ERROR_19   19
#define PROTOCOL_ERROR_20   20
#define PROTOCOL_ERROR_21   21
#define PROTOCOL_ERROR_22   22
#define PROTOCOL_ERROR_23   23
#define PROTOCOL_ERROR_24   24

#define PROTOCOL_ERROR_1001 1001
#define PROTOCOL_ERROR_1011 1011
#define PROTOCOL_ERROR_1012 1012
#define PROTOCOL_ERROR_1021 1021
#define PROTOCOL_ERROR_1022 1022
#define PROTOCOL_ERROR_1031 1031
#define PROTOCOL_ERROR_1032 1032
#define PROTOCOL_ERROR_1041 1041
#define PROTOCOL_ERROR_1042 1042
#define PROTOCOL_ERROR_1051 1051
#define PROTOCOL_ERROR_1052 1052
#define PROTOCOL_ERROR_1061 1061
#define PROTOCOL_ERROR_1062 1062
#define PROTOCOL_ERROR_1071 1071
#define PROTOCOL_ERROR_1072 1072
#define PROTOCOL_ERROR_1081 1081
#define PROTOCOL_ERROR_1082 1082
#define PROTOCOL_ERROR_1091 1091
#define PROTOCOL_ERROR_1092 1092
#define PROTOCOL_ERROR_9999 9999

//-------------------------------------------------------
// Server Side Error Codes

#define MDS_ERROR_CONNECTING_TO_SERVER          1
#define MDS_ERROR_OPENING_TREE                  2
#define MDS_ERROR_ALLOCATING_HEAP_TDI_RANK      3
#define MDS_ERROR_MDSVALUE_RANK                 4
#define MDS_ERROR_ALLOCATING_HEAP_TDI_SIZE      5
#define MDS_ERROR_MDSVALUE_SIZE                 6
#define MDS_ERROR_ALLOCATING_HEAP_DATA_BLOCK    7
#define MDS_ERROR_MDSVALUE_DATA                 8
#define MDS_ERROR_ALLOCATING_HEAP_TDI_LEN_UNITS 9
#define MDS_ERROR_MDSVALUE_LEN_UNITS            10
#define MDS_ERROR_UNITS_LENGTH_TOO_LARGE        11
#define MDS_ERROR_ALLOCATING_HEAP_TDI_UNITS     12
#define MDS_ERROR_ALLOCATING_HEAP_UNITS         13
#define MDS_ERROR_MDSVALUE_UNITS                14
#define MDS_ERROR_READING_DIMENSIONAL_DATA      15
#define MDS_ERROR_MDSVALUE_TYPE                 16

#define MDS_ERROR_DIM_DIMENSION_NUMBER_EXCEEDED     17
#define MDS_ERROR_DIM_ALLOCATING_HEAP_TDI_SIZE      18
#define MDS_ERROR_DIM_MDSVALUE_SIZE                 19
#define MDS_ERROR_DIM_ALLOCATING_HEAP_DATA_BLOCK    20
#define MDS_ERROR_DIM_ALLOCATING_HEAP_TDI_DIM_OF    21
#define MDS_ERROR_DIM_MDSVALUE_DATA                 22
#define MDS_ERROR_DIM_ALLOCATING_HEAP_TDI_LEN_UNITS 23
#define MDS_ERROR_DIM_MDSVALUE_LEN_UNITS            24
#define MDS_ERROR_DIM_UNITS_LENGTH_TOO_LARGE        25
#define MDS_ERROR_DIM_ALLOCATING_HEAP_TDI_UNITS     26
#define MDS_ERROR_DIM_ALLOCATING_HEAP_DIM_UNITS     27
#define MDS_ERROR_DIM_MDSVALUE_UNITS                28

#define MDS_SERVER_NOT_RESPONDING           30
#define MDS_SERVER_NOT_KNOWN                31
#define MDS_SERVER_PING_TEST_PROBLEM        32

#define UNCOMPRESS_ALLOCATING_HEAP          40
#define UNKNOWN_DATA_TYPE                   41
#define ERROR_ALLOCATING_HEAP               42
#define ERROR_ALLOCATING_META_DATA_HEAP     43

//-------------------------------------------------------
#define MDS_ERROR_READING_SERVER_NAME           50
#define MDS_ERROR_RETURNING_RANK                51
#define MDS_ERROR_DATA_ARRAY_SIZE               52
#define MDS_ERROR_XDR_ENDOFRECORD               53
#define MDS_ERROR_RETURNING_DATA_UNITS          54
#define MDS_ERROR_RETURNING_DATA_VALUES         55
#define MDS_ERROR_DIMENSION_ARRAY_SIZE          56
#define MDS_ERROR_RETURNING_DIMENSION_UNITS     57
#define MDS_ERROR_RETURNING_DIMENSION_VALUES    58

//-------------------------------------------------------

#define IDA_CLIENT_FILE_NAME_TOO_LONG           100
#define IDA_CLIENT_SIGNAL_NAME_TOO_LONG         101
#define IDA_ERROR_OPENING_FILE                  102
#define IDA_ERROR_IDENTIFYING_ITEM              103
#define IDA_ERROR_READING_DATA                  104
#define IDA_ERROR_RETURNING_RANK                105
#define IDA_ERROR_DATA_ARRAY_SIZE               106
#define IDA_ERROR_XDR_ENDOFRECORD               107
#define IDA_ERROR_ALLOCATING_HEAP               108
#define IDA_ERROR_RETURNING_DATA_UNITS          109
#define IDA_ERROR_RETURNING_DATA_VALUES         110
#define IDA_ERROR_DIMENSION_ARRAY_SIZE          111
#define IDA_ERROR_RETURNING_DIMENSION_UNITS     112
#define IDA_ERROR_RETURNING_DIMENSION_VALUES    113

#endif //IDAM_IDAMERRORS_H
