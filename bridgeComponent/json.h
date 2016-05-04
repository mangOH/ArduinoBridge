/*
 * @file mangoh_bridge_json.h
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

#ifndef MANGOH_BRIDGE_JSON_INCLUDE_GUARD
#define MANGOH_BRIDGE_JSON_INCLUDE_GUARD

#define MANGOH_BRIDGE_JSON_UNICODE_BUFFER_LEN          4
#define MANGOH_BRIDGE_JSON_BUFFER_ALLOC_LEN            64
#define MANGOH_BRIDGE_JSON_STRING_ALLOC_LEN            32
#define MANGOH_BRIDGE_JSON_INTEGER_MAX_LEN             32
#define MANGOH_BRIDGE_JSON_FLOAT_MAX_LEN               64

#define MANGOH_BRIDGE_JSON_MESSAGE_RESPONSE            "response"
#define MANGOH_BRIDGE_JSON_MESSAGE_REQUEST             "request"
#define MANGOH_BRIDGE_JSON_MESSAGE_KEY                 "key"
#define MANGOH_BRIDGE_JSON_MESSAGE_VALUE               "value"
#define MANGOH_BRIDGE_JSON_MESSAGE_DATA                "data"

#define MANGOH_BRIDGE_JSON_NULL                        "null"
#define MANGOH_BRIDGE_JSON_NULL_LEN                    4
#define MANGOH_BRIDGE_JSON_TRUE                        "true"
#define MANGOH_BRIDGE_JSON_TRUE_LEN                    4
#define MANGOH_BRIDGE_JSON_FALSE                       "false"
#define MANGOH_BRIDGE_JSON_FALSE_LEN                   5

//--------------------------------------------------------------------------------------------------
/**
 * JSON data types
 */
//--------------------------------------------------------------------------------------------------
typedef enum _mangoh_bridge_json_data_type_e
{
    MANGOH_BRIDGE_JSON_DATA_TYPE_INVALID = 0,
    MANGOH_BRIDGE_JSON_DATA_TYPE_INT,
    MANGOH_BRIDGE_JSON_DATA_TYPE_FLOAT,
    MANGOH_BRIDGE_JSON_DATA_TYPE_UNICODE,
    MANGOH_BRIDGE_JSON_DATA_TYPE_STRING,
    MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN,
    MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY,
    MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT,
    MANGOH_BRIDGE_JSON_DATA_TYPE_NULL,
} mangoh_bridge_json_data_type_e;

//--------------------------------------------------------------------------------------------------
/**
 * JSON data
 */
//--------------------------------------------------------------------------------------------------
typedef union _mangoh_bridge_json_data_u
{
    le_sls_List_t objVal;   ///< JSON object
    le_sls_List_t arrayVal; ///< Array data
    union {
        char      bytes[MANGOH_BRIDGE_JSON_UNICODE_BUFFER_LEN];
        uint16_t  val;
    } unicodeVal;           ///< Unicode data
    char*         strVal;   ///< String data
    double        dVal;     ///< Floating point data
    int64_t       iVal;     ///< Integer data
    bool          bVal;     ///< Boolean data
} mangoh_bridge_json_data_u;

//--------------------------------------------------------------------------------------------------
/**
 * JSON data object
 */
//--------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_json_data_t
{
    mangoh_bridge_json_data_u      data; ///< JSON data
    mangoh_bridge_json_data_type_e type; ///< JSON data type
    uint32_t                       len;  ///< JSON data length
} mangoh_bridge_json_data_t;

//--------------------------------------------------------------------------------------------------
/**
 * JSON data array item
 */
//--------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_json_array_item_t
{
    le_sls_Link_t              link; ///< Linked list link
    mangoh_bridge_json_data_t* item; ///< Linked list JSON data
} mangoh_bridge_json_array_item_t;

//--------------------------------------------------------------------------------------------------
/**
 * JSON data object array item
 */
//--------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_json_array_obj_item_t
{
    le_sls_Link_t              link;      ///< Linked list link
    mangoh_bridge_json_data_t* item;      ///< JSON object data
    char*                      attribute; ///< JSON object attribute
} mangoh_bridge_json_array_obj_item_t;

int mangoh_bridge_json_getCommand(const mangoh_bridge_json_data_t*, char**);
int mangoh_bridge_json_getKey(const mangoh_bridge_json_data_t*, char**);
int mangoh_bridge_json_getValue(const mangoh_bridge_json_data_t*, mangoh_bridge_json_data_t**);
int mangoh_bridge_json_getData(const mangoh_bridge_json_data_t*, mangoh_bridge_json_data_t**);

int mangoh_bridge_json_setRequestCommand(mangoh_bridge_json_data_t*, const char*);
int mangoh_bridge_json_setResponseCommand(mangoh_bridge_json_data_t*, const char*);
int mangoh_bridge_json_setData(mangoh_bridge_json_data_t*, const uint8_t*, uint32_t);
int mangoh_bridge_json_setKey(mangoh_bridge_json_data_t*, const char*);
int mangoh_bridge_json_setValue(mangoh_bridge_json_data_t*, const mangoh_bridge_json_data_t*);

int mangoh_bridge_json_createArray(mangoh_bridge_json_data_t**);
int mangoh_bridge_json_copyObject(mangoh_bridge_json_data_t**, const mangoh_bridge_json_data_t*);
int mangoh_bridge_json_addObject(mangoh_bridge_json_data_t*, const mangoh_bridge_json_data_t*);

int mangoh_bridge_json_createObject(mangoh_bridge_json_data_t**);

int mangoh_bridge_json_read(const uint8_t* const, uint32_t*, mangoh_bridge_json_data_t**);
int mangoh_bridge_json_write(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*);
int mangoh_bridge_json_destroy(mangoh_bridge_json_data_t**);

#endif
