//
// To Do:
//
//	Only rank 1 arrays are handled. Higher rank causes a seg fault
//
// Change History:
//
// 05Jan2010	DGMuir	Original version
// 04May2010	DGMuir	Corrected bug when type is unsigned
//			Corrected for final structure packing: alignment is largest of element alignments
// 06Feb2015	DGMuir  Added test for record beginning */
//----------------------------------------------------------------------------------------------------------------------------
// parse a standard include/header file for structure definitions.
// alternatively, query an SQL database for the structure definition: Always up-to-date!
// XML Schema -> gSOAP -> header files -> SQL Database -> Parsed on demand -> Definition -> XDR decode/encode
// IDAM call backs for additional data
//
// Structure Packing using the alignment boundary for this compiler/system architecture. Deserialisation
// of structure defintions on the client will correct for client side local alignment boundaries and system
// architrecture.
//
// enumerated types become unsigned integer types
//
// use gcc pre-processor with the -E option: strips away text etc; resolves #ifdef statements etc.
// comment out unwanted header files as these are expanded in line
// add any required type defs, e.g. xmlChar
// e.g., gcc -E ./tree.h -I/usr/include/libxml2 > etree.h
//----------------------------------------------------------------------------------------------------------------------------

#include "parseIncludeFile.h"

#include "idamclientserver.h"
#include "TrimString.h"
#include "idamErrorLog.h"
#include "struct.h"

#if defined(SERVERBUILD) || !defined(CLEANNAMESPACE25SEP14)

int parseIncludeFile(char* header)
{
#else
    int parseIdamIncludeFile(char *header, USERDEFINEDTYPELIST *userdefinedtypelist){
#endif

    int i, j, lstr, rnk = 0, status = 0, err, model = 0, space, isStruct, isConst, isUnsigned, isLong64, isEnum = 0, maxAlign = 0;
    FILE* fh = NULL;

    char buffer[STRING_LENGTH];
    char work[STRING_LENGTH];
    char name[MAXELEMENTNAME];
    char name1[MAXELEMENTNAME], name2[MAXELEMENTNAME], name3[MAXELEMENTNAME];
    char* p = NULL;
    char* image = NULL;

    int itemCount = 0, defCount = 0, byteCount, imagecount = 0, typeStrCount = 0, typeDefCount = 0, typeEnumCount = 0;
    char type[MAXELEMENTS][MAXELEMENTNAME];
    char item[MAXELEMENTS][MAXELEMENTNAME];
    char desc[MAXELEMENTS][MAXELEMENTNAME];
    char typeStrName[MAXELEMENTS][MAXELEMENTNAME];
    char typeStruct[MAXELEMENTS][MAXELEMENTNAME];    // structure types
    char typeDefName[MAXELEMENTS][MAXELEMENTNAME];
    char typeDef[MAXELEMENTS][MAXELEMENTNAME];        // Type Definitions
    int typeStrPtr[MAXELEMENTS];            // Flag pointer type def
    int typeDefPtr[MAXELEMENTS];

    char typeEnum[MAXELEMENTS][MAXELEMENTNAME];        // enumerated Types
    int pointer[MAXELEMENTS];
    int offset[MAXELEMENTS];
    int offpad[MAXELEMENTS];        // Position before the structure element
    int size[MAXELEMENTS];
    int count[MAXELEMENTS];
    int rank[MAXELEMENTS];
    int shape[MAXELEMENTS][MAXRANK];

    char defnames[MAXELEMENTS][MAXELEMENTNAME];
    int defvalues[MAXELEMENTS];

    USERDEFINEDTYPE* userdefinedtype = NULL;

//------------------------------------------------------------------------------------------
// Initialise

    for (i = 0; i < MAXELEMENTS; i++) {
        typeStrPtr[i] = 0;
        typeDefPtr[i] = 0;
        pointer[i] = 0;
        offset[i] = 0;
        offpad[i] = 0;
        size[i] = 0;
        count[i] = 0;
        rank[i] = 0;
    }

//------------------------------------------------------------------------------------------
// Open the Test File

    errno = 0;
    fh = fopen(header, "r");

    if (fh == NULL || ferror(fh) || errno != 0) {
        err = 999;
        if (errno != 0) {
            addIdamError(&idamerrorstack, SYSTEMERRORTYPE, "parseIncludeFile", errno,
                         "Unable to Open Structure Definition Header file for Read Access!");
        }
        if (fh != NULL) fclose(fh);
        return err;
    }

//------------------------------------------------------------------------------------------
// Parse the Test File
//
// 3 type of structure type defintion:
//
// struct AAA {		or
// struct AAA
// {
//	int 	a;		// Descriptions are captured if the begin with //
//	float 	b;
// };
// typedef struct AAA AAA ;	assumed not split over lines
//
// typedef struct
// {			or
// typedef struct{
//	int 	a;
//	float 	b;
// } AAA;
//
// struct AAA {		or
// struct AAA			This does not use a typedef: instead the type defintion
// {				struct AAA aaa; is used
//	int 	a;
//	float 	b;
// };				This is identical to the first definition. However, it is not
//				followed by a typedef statement.
//
//
// Assumed rules:
//
// 1> 	retain all #define using integer values for resolving array lengths.
//	Arithmetic products like 123*234 cannot be resolved. Why recreate the preprocessor if not necessary.
//	Compiler options cannot be resolved: Prune as necessary
//	There are defCount defined values. Names do Not Match.
//      Don't add constants to #defined values within array definitions.
// 2>	The start of a Structure definition begins with 'struct' then the structure name.
//	The structure contents are immediately followed by 'typedef struct' then the structure and type name (must be the same)
// 3>	The start of a Structure definition begins with 'typedef struct'.
//	The structure contents are immediately followed by the type name.
// 4>	Don't use /* */ comment block syntax
// 5>	Structure UNIONS are not used, not anything complex: simple structures are required.
// 6>	multiple pointer types, e.g., char **, are not yet impemented
// 7>	Structure elements should be aligned to avoid packing bytes: these may cause problems client side
// 8>   If a typedef is not used, prefix the structure name with 'struct'
//
//
//
    while (fgets(buffer, STRING_LENGTH, fh) != NULL) {

        LeftTrimString(TrimString(buffer));

        do {

            if (!strncmp(buffer, "# ", 2)) break;
            if (!strncmp(buffer, "//", 2)) break;
            if (!strncmp(buffer, "/*", 2)) break;
            if (!strncmp(buffer, "*/", 2)) break;
            if (buffer[0] == '\n') break;
            if (buffer[0] == '\0') break;

            if (!strncmp(buffer, "#define ", 8)) {
                strcpy(defnames[defCount], &buffer[8]);
                if ((p = strstr(defnames[defCount], "//")) != NULL) p[0] = '\0';        // drop Comments
                convertNonPrintable2(defnames[defCount]);
                LeftTrimString(defnames[defCount]);
                TrimString(defnames[defCount]);
                if ((p = strchr(defnames[defCount], ' ')) != NULL) {
                    strcpy(buffer, &p[1]);
                    LeftTrimString(buffer);
                    TrimString(buffer);
                    if (IsNumber(buffer)) {
                        defvalues[defCount] = atoi(buffer);
                        p[0] = '\0';
                        defCount++;

// TO DO: Check MAXELEMENTS not exceeded!

                    }
                }
                break;
            }

// *** Scan for free standing typedef statements


            if (!isEnum && !strncmp(buffer, "typedef enum", 12)) {
                isEnum = 1;    // Enumerated Type Definition
                break;
            }

            if (isEnum) {
                if (buffer[0] == '}') {
                    isEnum = 0;
                    strcpy(name1, &buffer[2]);
                    if ((p = strchr(name1, ';')) != NULL) p[0] = '\0';
                    LeftTrimString(name1);
                    TrimString(name1);
                    strcpy(typeEnum[typeEnumCount++], name1);        // type name
                    name1[0] = '\0';
                } else break;
            }

            if (!isEnum && !strncmp(buffer, "typedef", 7) &&
                (strncmp(buffer, "typedef struct", 14) != 0)) { // Regular Type Definition
                strcpy(name1, &buffer[7]);
                LeftTrimString(name1);
                TrimString(name1);
                if ((p = strchr(name1, ' ')) != NULL) {
                    p[0] = '\0';
                    strcpy(name2, &p[1]);
                    if ((p = strchr(name2, ';')) != NULL) p[0] = '\0';
                    if ((p = strchr(name2, '*')) != NULL) {
                        p[0] = ' ';
                        typeDefPtr[typeDefCount] = 1;
                    } else typeDefPtr[typeDefCount] = 0;
                    LeftTrimString(name2);
                    TrimString(name2);
                    strcpy(typeDefName[typeDefCount], name2);        // type synonym
                    strcpy(typeDef[typeDefCount++], name1);        // actual type
                    name1[0] = '\0';
                    name2[0] = '\0';
                } else {
                    addIdamError(&idamerrorstack, CODEERRORTYPE, "parseIncludeFile", 999,
                                 "typedef statement does not conform to syntax model!");
                }
                buffer[0] = '\0';
                break;
            }

            if (!isEnum && !strncmp(buffer, "typedef struct", 14) &&
                (p = strchr(buffer, '{')) == NULL) { // Type Definition
                strcpy(name1, &buffer[14]);
                LeftTrimString(name1);
                TrimString(name1);
                if ((p = strchr(name1, ' ')) != NULL) {
                    p[0] = '\0';
                    strcpy(name2, &p[1]);
                    if ((p = strchr(name2, ';')) != NULL) p[0] = '\0';
                    if ((p = strchr(name2, '*')) != NULL) {
                        p[0] = ' ';
                        typeStrPtr[typeDefCount] = 1;
                    } else typeStrPtr[typeDefCount] = 0;
                    LeftTrimString(name2);
                    TrimString(name2);
                    strcpy(typeStrName[typeStrCount], name2);        // type synonym
                    strcpy(typeStruct[typeStrCount++], name1);        // actual type
                    name1[0] = '\0';
                    name2[0] = '\0';
                } else {
                    addIdamError(&idamerrorstack, CODEERRORTYPE, "parseIncludeFile", 999,
                                 "typedef statement does not conform to syntax model!");
                }
                buffer[0] = '\0';
                break;
            }


            if (!status && !strncmp(buffer, "struct", 6)) {
                model = 1;                            // Start of stucture definition using model 1
            } else {
                if (!status &&
                    !strncmp(buffer, "typedef struct", 14)) {        // Start of stucture definition using model 2
                    model = 2;
                }
            }

            do {

                if (status && model == 1 && (!strcmp(buffer, "{\n") || !strncmp(buffer, "#endif", 5) ||
                                             !strncmp(buffer, "#define", 6) || !strncmp(buffer, "#ifdef", 5) ||
                                             !strncmp(buffer, "#ifndef", 6)))
                    break;

                if (status || model != 0) {                    // Start of structure definition
                    if (!status) {                        // Structure Name (also Type) follows (model 1 only)

                        image = NULL;
                        imagecount = 0;
                        addImage(&image, &imagecount, buffer);

                        if (model == 1) {
                            strcpy(name, &buffer[7]);
                            if ((p = strchr(name, ' ')) != NULL) p[0] = '\0';
                            if ((p = strchr(name, '{')) != NULL) p[0] = '\0';
                            convertNonPrintable2(name);
                            LeftTrimString(name);
                            TrimString(name);
                        } else {
                            name[0] = '\0';
                        }

                        itemCount = 0;    // New Structure Contents
                        status = 1;    // Flag start of definition

                    } else {
                        // End of the Structure definition?
                        if (model != 0 && !strncmp(buffer, "}", 1)) {

                            addImage(&image, &imagecount, "};\n");
                            addImage(&image, &imagecount, buffer);
                            addImage(&image, &imagecount, "\n");

                            if (model == 1) {
                                if (buffer[0] == '}') {        // No typedef statement
                                    //strcpy(name1, "struct ");
                                    //strcat(name1, name);
                                    //strcpy(name, name1);
                                    strcpy(name1, name);
                                    strcpy(name2, name1);
                                    strcpy(name3, name1);
                                } else {                            // typedef statement
                                    //if(!strncmp(buffer, "typedef struct ", 15)){
                                    //   p = &buffer[15];
                                    if ((p = strchr(&buffer[14], ' ')) != NULL) {
                                        strcpy(name2, &p[1]);
                                        if ((p = strchr(name2, ' ')) != NULL) {
                                            p[0] = '\0';
                                            strcpy(name1, name2);
                                            strcpy(name3, &p[1]);
                                            strcpy(name2, name3);
                                            MidTrimString(name1);
                                            MidTrimString(name2);
                                            if ((p = strchr(name2, ';')) != NULL) p[0] = '\0';
                                            strcpy(typeStrName[typeStrCount], name1);        // type synonym
                                            strcpy(typeStruct[typeStrCount++], name2);        // actual type
                                        }
                                    }
                                }
                            } else {
                                if ((p = strchr(buffer, ' ')) != NULL) {
                                    strcpy(name, &p[1]);
                                    MidTrimString(name);
                                    if ((p = strchr(name, ';')) != NULL) p[0] = '\0';
                                    strcpy(name1, name);
                                    strcpy(name2, name);

                                    strcpy(typeStrName[typeStrCount], name);        // type synonym
                                    strcpy(typeStruct[typeStrCount++], name);        // actual type

                                }
                            }

                            if (!strcmp(name, name1) && !strcmp(name, name2)) {    // Create Meta data on structure
                                if (itemCount > 0) {
                                    userdefinedtypelist->userdefinedtype = (USERDEFINEDTYPE*) realloc(
                                            (void*) userdefinedtypelist->userdefinedtype,
                                            (userdefinedtypelist->listCount + 1) * sizeof(USERDEFINEDTYPE));
                                    userdefinedtype = &userdefinedtypelist->userdefinedtype[userdefinedtypelist->listCount];
                                    initUserDefinedType(userdefinedtype);

                                    userdefinedtypelist->listCount = userdefinedtypelist->listCount + 1;

                                    strcpy(userdefinedtype->name, name);            // Object Name
                                    strcpy(userdefinedtype->source, header);            // Source of the Definition
                                    userdefinedtype->idamclass = TYPE_COMPOUND;        // Class of object

                                    userdefinedtype->compoundfield = (COMPOUNDFIELD*) malloc(
                                            itemCount * sizeof(COMPOUNDFIELD));
                                    byteCount = 0;
                                    maxAlign = 0;
                                    for (i = 0; i < itemCount; i++) {
                                        initCompoundField(&userdefinedtype->compoundfield[i]);
                                        strcpy(userdefinedtype->compoundfield[i].name, item[i]);
                                        strcpy(userdefinedtype->compoundfield[i].type, type[i]);
                                        strcpy(userdefinedtype->compoundfield[i].desc, desc[i]);
                                        userdefinedtype->compoundfield[i].pointer = pointer[i];
                                        userdefinedtype->compoundfield[i].size = size[i];
                                        userdefinedtype->compoundfield[i].offset = offset[i];
                                        userdefinedtype->compoundfield[i].offpad = offpad[i];
                                        if (pointer[i]) {
                                            userdefinedtype->compoundfield[i].alignment = getalignmentof("*");
                                        } else {
                                            userdefinedtype->compoundfield[i].alignment = getalignmentof(type[i]);
                                        }
                                        if (userdefinedtype->compoundfield[i].alignment > maxAlign)
                                            maxAlign = userdefinedtype->compoundfield[i].alignment;
                                        userdefinedtype->compoundfield[i].atomictype = gettypeof(type[i]);
                                        userdefinedtype->compoundfield[i].rank = rank[i];
                                        userdefinedtype->compoundfield[i].count = count[i];
                                        if (rank[i] > 0)
                                            userdefinedtype->compoundfield[i].shape = (int*) malloc(
                                                    rank[i] * sizeof(int));
                                        else
                                            userdefinedtype->compoundfield[i].shape = NULL;
                                        for (j = 0; j < rank[i]; j++)
                                            userdefinedtype->compoundfield[i].shape[j] = shape[i][j];

                                        byteCount = byteCount + size[i] * count[i] + offpad[i];
                                    }

// Add a final Packing to align the structure if necessary

                                    byteCount = byteCount + ((maxAlign - (byteCount % maxAlign)) %
                                                             maxAlign);        // padding(byteCount, "STRUCTURE");

                                    userdefinedtype->fieldcount = itemCount;    // Count of child elements
                                    userdefinedtype->size = byteCount;    // Size of the Object (on the run time system)

                                    userdefinedtype->imagecount = imagecount;
                                    userdefinedtype->image = image;
                                }
                            } else {
                                //fprintf(stdout,"Names do Not Match: %s, %s, %s\n", name, name1, name2);
                                err = 999;
                                addIdamError(&idamerrorstack, CODEERRORTYPE, "parseIncludeFile", 999,
                                             "typedef statement does not conform to syntax model!");
                                return (err);
                            }

                            status = 0;        // Structure Definition Completed
                            model = 0;

                        } else {

                            if (buffer[0] != '}' && strlen(buffer) > 0) {    // Structure Contents

                                expandImage(buffer, defnames, defvalues, defCount, work);
                                addImage(&image, &imagecount, work);

                                convertNonPrintable2(buffer);
                                LeftTrimString(buffer);

                                isStruct = 0;
                                isConst = 0;
                                isUnsigned = 0;
                                isLong64 = 0;

                                if ((isStruct = !strncmp(buffer, "struct", 6))) {
                                    strcpy(work, &buffer[7]);
                                    strcpy(buffer, work);
                                } else {
                                    if ((isConst = !strncmp(buffer, "const", 5))) {
                                        strcpy(work, &buffer[6]);
                                        strcpy(buffer, work);
                                    }
                                    if ((isUnsigned = !strncmp(buffer, "unsigned", 8))) {
                                        strcpy(work, &buffer[9]);
                                        strcpy(buffer, work);
                                    }
                                    if ((isLong64 = !strncmp(buffer, "long long", 9))) {
                                        strcpy(work, &buffer[9]);
                                        strcpy(buffer, work);
                                    }
                                }


                                if ((p = strchr(buffer, ' ')) != NULL && strncmp(buffer, "//", 2) != 0) {
                                    p[0] = '\0';

                                    if (!isConst && !isUnsigned && !isLong64) {
                                        strcpy(type[itemCount],
                                               buffer);            // Atomic or User Defined Type of element
                                    } else {
                                        type[itemCount][0] = '\0';
                                        if (isConst) {
                                            // strcat(type[itemCount], "const ");		// Not required
                                        }
                                        if (isUnsigned) {
                                            strcpy(type[itemCount], "unsigned ");
                                        }
                                        if (isLong64) {
                                            strcpy(type[itemCount], "long long ");
                                        }
                                        strcat(type[itemCount], buffer);
                                    }

                                    strcpy(item[itemCount], &p[1]);            // Element Name

// Is this a Description or comment on the item?

                                    desc[itemCount][0] = '\0';
                                    if ((p = strstr(item[itemCount], "//")) != NULL) {
                                        p[0] = '\0';
                                        strcpy(desc[itemCount], &p[2]);
                                    }
                                    if ((p = strstr(item[itemCount], "/*")) != NULL) {
                                        p[0] = '\0';
                                        strcpy(desc[itemCount], &p[2]);
                                        if ((p = strstr(desc[itemCount], "*/")) != NULL) p[0] = '\0';
                                    }

// Compact strings and remove unprintable characters and terminating ;

                                    LeftTrimString(desc[itemCount]);
                                    TrimString(desc[itemCount]);

                                    LeftTrimString(type[itemCount]);
                                    if (!isConst && !isUnsigned && !isLong64) MidTrimString(type[itemCount]);
                                    TrimString(type[itemCount]);

                                    convertNonPrintable2(item[itemCount]);
                                    LeftTrimString(item[itemCount]);
                                    MidTrimString(item[itemCount]);
                                    TrimString(item[itemCount]);
                                    if ((p = strstr(item[itemCount], ";")) != NULL) p[0] = '\0';

// Is this a pointer ?	(pointer size is NOT passed: 32/64 bit dependent)

                                    pointer[itemCount] = 0;
                                    if (item[itemCount][0] == '*') {
                                        pointer[itemCount] = 1;
                                        item[itemCount][0] = ' ';
                                        LeftTrimString(item[itemCount]);
                                    } else {
                                        if (type[itemCount][strlen(type[itemCount]) - 1] == '*') {
                                            pointer[itemCount] = 1;
                                            type[itemCount][strlen(type[itemCount]) - 1] = ' ';
                                            TrimString(type[itemCount]);
                                        }
                                    }

// Substitute type defs and enum types

                                    for (j = 0; j < typeEnumCount; j++) {
                                        if (!strcmp(typeEnum[j], type[itemCount])) {
                                            strcpy(type[itemCount], "unsigned int");
                                            break;
                                        }
                                    }
                                    for (j = 0; j < typeDefCount; j++) {
                                        if (!strcmp(typeDefName[j], type[itemCount]) &&
                                            strcmp(type[itemCount], "STRING") != 0) {
                                            strcpy(type[itemCount], typeDef[j]);
                                            if (typeDefPtr[j]) pointer[itemCount] = 1;
                                            break;
                                        }
                                    }
                                    for (j = 0; j < typeStrCount; j++) {
                                        if (!strcmp(typeStrName[j], type[itemCount])) {
                                            strcpy(type[itemCount], typeStruct[j]);
                                            if (typeStrPtr[j]) pointer[itemCount] = 1;
                                            break;
                                        }
                                    }

// repeat for nested type defs

                                    for (j = 0; j < typeEnumCount; j++) {
                                        if (!strcmp(typeEnum[j], type[itemCount])) {
                                            strcpy(type[itemCount], "unsigned int");
                                            break;
                                        }
                                    }
                                    for (j = 0; j < typeDefCount; j++) {
                                        if (!strcmp(typeDefName[j], type[itemCount]) &&
                                            strcmp(type[itemCount], "STRING") != 0) {
                                            strcpy(type[itemCount], typeDef[j]);
                                            if (typeDefPtr[j]) pointer[itemCount] = 1;
                                            break;
                                        }
                                    }
                                    for (j = 0; j < typeStrCount; j++) {
                                        if (!strcmp(typeStrName[j], type[itemCount])) {
                                            strcpy(type[itemCount], typeStruct[j]);
                                            if (typeStrPtr[j]) pointer[itemCount] = 1;
                                            break;
                                        }
                                    }

// Size of this type (Not pointer size)

                                    if (!pointer[itemCount]) {
                                        if (!strcmp(type[itemCount], "STRING")) {
                                            size[itemCount] = getsizeof("char");
                                        } else {
                                            size[itemCount] = getsizeof(type[itemCount]);
                                        }
                                        //if(size[itemCount] == 0)fprintf(stdout," **** WARNING: type %s is not recognised ****\n", type[itemCount]);
                                    }

// Rank and Shape (Pointer rank and shape are unknown)

                                    rnk = 0;
                                    count[itemCount] = 0;
                                    if (!pointer[itemCount] &&
                                        ((p = strchr(item[itemCount], '[')) != NULL)) {        // Array
                                        char* p1 = p;
                                        char* p2 = NULL;
                                        do {
                                            if ((p2 = strchr(item[itemCount], ']')) != NULL) {
                                                lstr = (int) (p2 - p) - 1;
                                                strncpy(buffer, &p[1], lstr);
                                                buffer[lstr] = '\0';
                                                shape[itemCount][rnk] = 0;

                                                if (IsNumber(buffer)) {                        // hard coded size
                                                    shape[itemCount][rnk] = atoi(buffer);
                                                    count[itemCount] = count[itemCount] + shape[itemCount][rnk++];
                                                } else {

                                                    for (j = 0; j < defCount; j++) {
                                                        if (!strcmp(defnames[j], buffer)) {
                                                            shape[itemCount][rnk++] = defvalues[j];            // Array Shape
                                                            count[itemCount] = count[itemCount] +
                                                                               defvalues[j];    // Total Count of Array elements
                                                            break;
                                                        }
                                                    }

                                                }
                                            }
                                        } while (p2 != NULL && (p = strchr(p2, '[')) != NULL);
                                        p1[0] = '\0';
                                    }
                                    rank[itemCount] = rnk;

// Scalar values and Pointers must have a count of at least 1

                                    if (count[itemCount] == 0) count[itemCount] = 1;

// Offset: Adjusted for structure packing because of alignment boundaries

                                    if (pointer[itemCount]) {
                                        size[itemCount] = sizeof(void*);    // Offsets and Pointer sizes are architecture dependent
                                        if (itemCount > 0) {
                                            space = size[itemCount - 1] * count[itemCount - 1];
                                            offpad[itemCount] = padding(offset[itemCount - 1] + space, "*");
                                            offset[itemCount] = newoffset(offset[itemCount - 1] + space, "*");
                                        } else {
                                            offpad[itemCount] = 0;
                                            offset[itemCount] = 0;
                                        }
                                    } else {
                                        if (itemCount > 0) {
                                            space = size[itemCount - 1] * count[itemCount - 1];
                                            offpad[itemCount] = padding(offset[itemCount - 1] + space, type[itemCount]);
                                            offset[itemCount] = newoffset(offset[itemCount - 1] + space,
                                                                          type[itemCount]);
                                        } else {
                                            offpad[itemCount] = 0;
                                            offset[itemCount] = 0;
                                        }
                                    }

                                    itemCount++;

// TO DO: Check MAXELEMENTS not exceeded!

                                }
                            }
                        }
                    }
                }
            } while (0);
        } while (0);
    }

//------------------------------------------------------------------------------------------
// Housekeeping

    fclose(fh);

    return (0);
}








