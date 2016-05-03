/*
 * @file json.h
 *
 * Arduino bridge JSON data processor module.
 *
 * This module handles JSON data serialization/deserialization as well as providing and
 * implementation for handling JSON message requests and creating JSON message responses.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "legato.h"

#ifndef SWI_MANGOH_BRIDGE_JSON_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_JSON_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_JSON_UNICODE_BUFFER_LEN          4
#define SWI_MANGOH_BRIDGE_JSON_BUFFER_ALLOC_LEN            64
#define SWI_MANGOH_BRIDGE_JSON_STRING_ALLOC_LEN            32
#define SWI_MANGOH_BRIDGE_JSON_INTEGER_MAX_LEN             32
#define SWI_MANGOH_BRIDGE_JSON_FLOAT_MAX_LEN               64

#define SWI_MANGOH_BRIDGE_JSON_MESSAGE_RESPONSE            "response"
#define SWI_MANGOH_BRIDGE_JSON_MESSAGE_REQUEST             "request"
#define SWI_MANGOH_BRIDGE_JSON_MESSAGE_KEY                 "key"
#define SWI_MANGOH_BRIDGE_JSON_MESSAGE_VALUE               "value"
#define SWI_MANGOH_BRIDGE_JSON_MESSAGE_DATA                "data"

#define SWI_MANGOH_BRIDGE_JSON_NULL                        "null"
#define SWI_MANGOH_BRIDGE_JSON_NULL_LEN                    4
#define SWI_MANGOH_BRIDGE_JSON_TRUE                        "true"
#define SWI_MANGOH_BRIDGE_JSON_TRUE_LEN                    4
#define SWI_MANGOH_BRIDGE_JSON_FALSE                       "false"
#define SWI_MANGOH_BRIDGE_JSON_FALSE_LEN                   5

//------------------------------------------------------------------------------------------------------------------
/**
 * JSON data types
 */
//------------------------------------------------------------------------------------------------------------------
typedef enum _swi_mangoh_bridge_json_data_type_e
{
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_INVALID = 0,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_INT,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_FLOAT,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_UNICODE,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_STRING,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT,
    SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_NULL,
} swi_mangoh_bridge_json_data_type_e;

//------------------------------------------------------------------------------------------------------------------
/**
 * JSON data
 */
//------------------------------------------------------------------------------------------------------------------
typedef union _swi_mangoh_bridge_json_data_u
{
    le_sls_List_t                            objVal;                                              ///< JSON object
    le_sls_List_t                            arrayVal;                                            ///< Array data
    union {
        char                                 bytes[SWI_MANGOH_BRIDGE_JSON_UNICODE_BUFFER_LEN];
        uint16_t                             val;
    } unicodeVal;                                                                                 ///< Unicode data
    char*                                    strVal;                                              ///< String data
    double                                   dVal;                                                ///< Floating point data
    int64_t                                  iVal;                                                ///< Integer data
    bool                                     bVal;                                                ///< Boolean data
} swi_mangoh_bridge_json_data_u;

//------------------------------------------------------------------------------------------------------------------
/**
 * JSON data object
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_json_data_t
{
    swi_mangoh_bridge_json_data_u            data;                                                ///< JSON data
    swi_mangoh_bridge_json_data_type_e       type;                                                ///< JSON data type
    uint32_t                                 len;                                                 ///< JSON data length
} swi_mangoh_bridge_json_data_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * JSON data array item
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_json_array_item_t
{
    le_sls_Link_t                            link;                                                ///< Linked list link
    swi_mangoh_bridge_json_data_t*           item;                                                ///< Linked list JSON data
} swi_mangoh_bridge_json_array_item_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * JSON data object array item
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_json_array_obj_item_t
{
    le_sls_Link_t                            link;                                                ///< Linked list link
    swi_mangoh_bridge_json_data_t*           item;                                                ///< JSON object data
    char*                                    attribute;                                           ///< JSON object attribute
} swi_mangoh_bridge_json_array_obj_item_t;

int swi_mangoh_bridge_json_getCommand(const swi_mangoh_bridge_json_data_t*, char**);
int swi_mangoh_bridge_json_getKey(const swi_mangoh_bridge_json_data_t*, char**);
int swi_mangoh_bridge_json_getValue(const swi_mangoh_bridge_json_data_t*, swi_mangoh_bridge_json_data_t**);
int swi_mangoh_bridge_json_getData(const swi_mangoh_bridge_json_data_t*, swi_mangoh_bridge_json_data_t**);

int swi_mangoh_bridge_json_setRequestCommand(swi_mangoh_bridge_json_data_t*, const char*);
int swi_mangoh_bridge_json_setResponseCommand(swi_mangoh_bridge_json_data_t*, const char*);
int swi_mangoh_bridge_json_setData(swi_mangoh_bridge_json_data_t*, const uint8_t*, uint32_t);
int swi_mangoh_bridge_json_setKey(swi_mangoh_bridge_json_data_t*, const char*);
int swi_mangoh_bridge_json_setValue(swi_mangoh_bridge_json_data_t*, const swi_mangoh_bridge_json_data_t*);

int swi_mangoh_bridge_json_createArray(swi_mangoh_bridge_json_data_t**);
int swi_mangoh_bridge_json_copyObject(swi_mangoh_bridge_json_data_t**, const swi_mangoh_bridge_json_data_t*);
int swi_mangoh_bridge_json_addObject(swi_mangoh_bridge_json_data_t*, const swi_mangoh_bridge_json_data_t*);

int swi_mangoh_bridge_json_createObject(swi_mangoh_bridge_json_data_t**);

int swi_mangoh_bridge_json_read(const uint8_t* const, uint32_t*, swi_mangoh_bridge_json_data_t**);
int swi_mangoh_bridge_json_write(const swi_mangoh_bridge_json_data_t*, uint8_t**, uint32_t*);
int swi_mangoh_bridge_json_destroy(swi_mangoh_bridge_json_data_t**);

#endif
