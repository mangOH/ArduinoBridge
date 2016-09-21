#include "legato.h"
#include "utils.h"
#include "json.h"

static bool mangoh_bridge_json_isDigit(const uint8_t*);
static int mangoh_bridge_json_hexToInt(const uint8_t*);
static uint32_t mangoh_bridge_json_getNextPowerOfTwo(uint32_t);
static int32_t mangoh_bridge_json_checkOutputBufferSize(uint8_t**, uint8_t**, uint32_t*, ssize_t);
static uint8_t mangoh_bridge_json_getEscapeChar(uint8_t);
static bool mangoh_bridge_json_readNext(uint8_t const**, uint32_t*);
static void mangoh_bridge_json_writeNext(uint8_t**, uint32_t*, uint32_t*, uint32_t);
static int mangoh_bridge_json_readChar(mangoh_bridge_json_data_t*, uint8_t, uint32_t*, uint8_t**);
static int mangoh_bridge_json_skipWhitespace(uint8_t const**, uint32_t*, bool);

static int mangoh_bridge_json_readArray(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readObject(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readNumber(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readString(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readTrue(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readFalse(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readNull(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_readComment(uint8_t const**, uint32_t*);
static int mangoh_bridge_json_readData(uint8_t const**, uint32_t*, mangoh_bridge_json_data_t**);

static int mangoh_bridge_json_writeBool(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeInteger(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeFloat(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeUnicode(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeString(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeNull(uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeArray(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeObject(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
static int mangoh_bridge_json_writeData(const mangoh_bridge_json_data_t*, uint8_t**, uint32_t*, uint8_t**, uint32_t*);

static void mangoh_bridge_json_destroyData(mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_destroyArray(mangoh_bridge_json_data_t**);
static int mangoh_bridge_json_destroyObject(mangoh_bridge_json_data_t**);

static int mangoh_bridge_json_getAttribute(const mangoh_bridge_json_data_t*, const char*, mangoh_bridge_json_data_t**);

static __inline bool mangoh_bridge_json_isDigit(const uint8_t* in)
{
    LE_ASSERT(in);
    return ((*in <= '9') && (*in >= '0'));
}

static __inline int mangoh_bridge_json_hexToInt(const uint8_t* in)
{
    LE_ASSERT(in);
    return (((*in <= '9') && (*in >= '0')) ? (*in - '0'):
            ((*in <= 'F') && (*in >= 'A')) ? (*in - 'A' + 10):
            ((*in <= 'f') && (*in >= 'a')) ? (*in - 'a' + 10):0);
}

static __inline uint32_t mangoh_bridge_json_getNextPowerOfTwo(uint32_t n)
{
  uint32_t k = 1;
  while (k < n)
      k <<= 1;

  return k;
}

static int32_t mangoh_bridge_json_checkOutputBufferSize(uint8_t** outBuff, uint8_t** outBuffPtr, uint32_t* allocLen, ssize_t size)
{
    int32_t res = LE_OK;

    LE_ASSERT(outBuff && *outBuff);
    LE_ASSERT(outBuffPtr);
    LE_ASSERT(*outBuffPtr >= *outBuff);
    LE_ASSERT(allocLen);

    if (*allocLen < size)
    {
        uint32_t len = *outBuffPtr - *outBuff;
        uint32_t incr = mangoh_bridge_json_getNextPowerOfTwo(len + size);

        LE_DEBUG("realloc(%u)", incr);
        uint8_t* tempPtr = realloc(*outBuff, incr);
        if (!tempPtr)
        {
            LE_ERROR("ERROR realloc() failed");
            free(*outBuff);
            *outBuff = NULL;
            res = LE_NO_MEMORY;
            goto cleanup;
        }

        *outBuff = tempPtr;
        *allocLen += (incr >> 1);
        *outBuffPtr = tempPtr + len;
    }

cleanup:
    return res;
}

static uint8_t mangoh_bridge_json_getEscapeChar(uint8_t val)
{
    switch (val)
    {
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'f':
        return '\f';
    case 'b':
        return '\b';
    case 'r':
        return '\r';
    }

    LE_ERROR("ERROR invalid escape('%c')", val);
    return 0;
}

static bool mangoh_bridge_json_readNext(uint8_t const** ptr, uint32_t* len)
{
    LE_ASSERT(ptr && *ptr);
    LE_ASSERT(len);

    (*ptr)++;
    (*len)--;
    return *len ? true:false;
}

static void mangoh_bridge_json_writeNext(uint8_t** ptr, uint32_t* allocLen, uint32_t* len, uint32_t size)
{
    LE_ASSERT(ptr && *ptr);
    LE_ASSERT(allocLen);
    LE_ASSERT(len);
    LE_ASSERT(*allocLen >= size);

    *len += size;
    *ptr += size;
    *allocLen -= size;
}

static int mangoh_bridge_json_readChar(mangoh_bridge_json_data_t* jsonData, uint8_t val, uint32_t* allocLen, uint8_t** out)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(allocLen);
    LE_ASSERT(out && *out);

    if (!*allocLen)
    {
        jsonData->len <<= 1;
        LE_DEBUG("realloc(%u)", jsonData->len);
        uint8_t* tempPtr = realloc(jsonData->data.strVal, jsonData->len);
        if (!tempPtr)
        {
            LE_ERROR("ERROR realloc() failed");
            free(jsonData->data.strVal);
            jsonData->data.strVal = NULL;
            res = LE_NO_MEMORY;
            goto cleanup;
        }

        jsonData->data.strVal = (char*)tempPtr;
        *allocLen = jsonData->len >> 1;
        *out = (uint8_t*)(jsonData->data.strVal + *allocLen);
    }

    **out = val;
    (*out)++;
    (*allocLen)--;

cleanup:
    return res;
}

static int mangoh_bridge_json_skipWhitespace(uint8_t const** in, uint32_t* len, bool forward)
{
    int32_t res = LE_OK;

    LE_ASSERT(in);
    LE_ASSERT(len);

    const uint8_t* ptr = (uint8_t*)*in;
    while ((*len > 0) && ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\r') || (*ptr == '\n') || (*ptr == '/')))
    {
        if (*ptr == '/')
        {
            res = mangoh_bridge_json_readComment(&ptr, len);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_json_readComment() failed(%d)", res);
                goto cleanup;
            }
        }
        else
        {
            if (forward) ptr++;
            else ptr--;
            (*len)--;
        }
    }

    *in = ptr;

cleanup:
    return res;
}

static int mangoh_bridge_json_readArray(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    LE_DEBUG("ARRAY START");
    (*jsonData)->len = sizeof((*jsonData)->data.arrayVal);
    (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY;
    (*jsonData)->data.arrayVal = LE_SLS_LIST_INIT;

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != '[')
    {
        LE_ERROR("ERROR invalid array('%c')", *ptr);
        goto cleanup;
    }

    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
    res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
        goto cleanup;
    }

    while (*ptr != ']')
    {
        mangoh_bridge_json_array_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_item_t));
        if (!jsonItemData)
        {
            LE_ERROR("ERROR calloc() failed");
            res = LE_NO_MEMORY;
            goto cleanup;
        }

        res = mangoh_bridge_json_readData(&ptr, len, &jsonItemData->item);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_readData() failed(%d)", res);
            goto cleanup;
        }

        jsonItemData->link = LE_SLS_LINK_INIT;
        le_sls_Queue(&(*jsonData)->data.arrayVal, &jsonItemData->link);

        res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
            goto cleanup;
        }

        if ((*ptr != ',') && (*ptr != ']'))
        {
            LE_ERROR("ERROR invalid array('%c')", *ptr);
            goto cleanup;
        }

        if (*ptr == ',')
        {
            if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
            res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
                goto cleanup;
            }
        }
    }

    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
    res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("ARRAY END");
    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readObject(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    mangoh_bridge_json_data_t* jsonKeyData = NULL;
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    LE_DEBUG("OBJECT START");
    (*jsonData)->len = sizeof((*jsonData)->data.objVal);
    (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT;
    (*jsonData)->data.objVal = LE_SLS_LIST_INIT;

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != '{')
    {
        LE_ERROR("ERROR invalid object");
        goto cleanup;
    }

    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
    res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
        goto cleanup;
    }

    while (*ptr != '}')
    {
        res = mangoh_bridge_json_readString(&ptr, len, &jsonKeyData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_readString() failed(%d)", res);
            goto cleanup;
        }

        res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
            goto cleanup;
        }

        if (*ptr != ':')
        {
            LE_ERROR("ERROR invalid object");
            goto cleanup;
        }

        if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
        res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
            goto cleanup;
        }

        mangoh_bridge_json_array_obj_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_obj_item_t));
        if (!jsonItemData)
        {
            LE_ERROR("ERROR calloc() failed");
            res = LE_NO_MEMORY;
            goto cleanup;
        }

        jsonItemData->attribute = malloc(strlen(jsonKeyData->data.strVal) + 1);
        if (!jsonItemData->attribute)
        {
            LE_ERROR("ERROR malloc() failed");
            res = LE_NO_MEMORY;
            goto cleanup;
        }
        strcpy(jsonItemData->attribute, jsonKeyData->data.strVal);

        res = mangoh_bridge_json_readData(&ptr, len, &jsonItemData->item);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_readData() failed(%d)", res);
            goto cleanup;
        }

        jsonItemData->link = LE_SLS_LINK_INIT;
        le_sls_Queue(&(*jsonData)->data.objVal, &jsonItemData->link);

        res = mangoh_bridge_json_destroy(&jsonKeyData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroy() failed(%d)", res);
            goto cleanup;
        }

        if (*len)
        {
            res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
                goto cleanup;
            }

            if ((*ptr != '}') && (*ptr != ','))
            {
                LE_ERROR("ERROR not valid JSON array");
                res = -EINVAL;
                goto cleanup;
            }

            if (*ptr == ',')
            {
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
                res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
                if (res != LE_OK)
                {
                    LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
                    goto cleanup;
                }
            }
        }
    }

    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
    res = mangoh_bridge_json_skipWhitespace(&ptr, len, true);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("OBJECT END");
    *data = ptr;
    res = LE_OK;

cleanup:
    if (res)
    {
        int32_t err = mangoh_bridge_json_destroy(&jsonKeyData);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroy() failed(%d)", err);
        }
    }

    return res;
}

static int mangoh_bridge_json_readNumber(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    const uint8_t* ptr = (uint8_t*)*data;
    bool isFloat = false;
    bool isNegative = false;
    while (mangoh_bridge_json_isDigit(ptr) || (!isFloat && (*ptr == '.')) || (!isNegative && (*ptr == '-')))
    {
        isFloat = isFloat || (*ptr == '.');
        isNegative = isNegative || (*ptr == '-');

        if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
    }

    if (isFloat)
    {
        (*jsonData)->data.dVal = atof((const char*)*data);
        (*jsonData)->len = sizeof((*jsonData)->data.dVal);
        (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_FLOAT;
        LE_DEBUG("FLOAT(%lf)", (*jsonData)->data.dVal);
    }
    else
    {
        (*jsonData)->data.iVal = atoi((const char*)*data);
        (*jsonData)->len = sizeof((*jsonData)->data.iVal);
        (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_INT;
        LE_DEBUG("INTEGER(%" PRId64 ")", (*jsonData)->data.iVal);
    }

    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readString(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    uint32_t allocLen = MANGOH_BRIDGE_JSON_STRING_ALLOC_LEN;
    (*jsonData)->len = allocLen;
    (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_STRING;
    (*jsonData)->data.strVal = malloc(allocLen);
    if (!(*jsonData)->data.strVal)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != '"')
    {
        LE_ERROR("ERROR invalid object");
        goto cleanup;
    }

    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    char* out = (*jsonData)->data.strVal;
    while ((*len > 0) && (*ptr != '"'))
    {
        if (*ptr == '\\')
        {
            if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

            if ((*ptr == 'b') || (*ptr == 'r') || (*ptr == 'n') || (*ptr == 'f') || (*ptr == 't'))
            {
                const uint8_t escape = mangoh_bridge_json_getEscapeChar(*ptr);
                if (mangoh_bridge_json_readChar(*jsonData, escape, &allocLen, (uint8_t**)&out) != LE_OK) goto cleanup;
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
            }
            else if (*ptr == 'u')
            {
                (*jsonData)->data.unicodeVal.val = mangoh_bridge_json_hexToInt(ptr) * 4096;
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

                (*jsonData)->data.unicodeVal.val += mangoh_bridge_json_hexToInt(ptr) * 256;
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

                (*jsonData)->data.unicodeVal.val += mangoh_bridge_json_hexToInt(ptr) * 16;
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

                (*jsonData)->data.unicodeVal.val += mangoh_bridge_json_hexToInt(ptr);
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

                free((*jsonData)->data.strVal);
                (*jsonData)->data.strVal = NULL;

                LE_DEBUG("UNICODE('\\u%04x')", (*jsonData)->data.unicodeVal.val);
                (*jsonData)->len = sizeof((*jsonData)->data.unicodeVal.val);
                (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_UNICODE;
            }
            else if ((*ptr != '"') && (*ptr != '\\') && (*ptr != '/'))
            {
                LE_ERROR("invalid JSON string('%c')", *ptr);
                goto cleanup;
            }
        }
        else
        {
            if (mangoh_bridge_json_readChar(*jsonData, *ptr, &allocLen, (uint8_t**)&out) != LE_OK) goto cleanup;
            if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
        }
    }

    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
    if (mangoh_bridge_json_readChar(*jsonData, '\0', &allocLen, (uint8_t**)&out) != LE_OK) goto cleanup;

    if ((*jsonData)->data.strVal)
    {
        LE_DEBUG("STRING('%s')", (*jsonData)->data.strVal);
    }

    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readTrue(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    LE_DEBUG("TRUE");
    (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN;
    (*jsonData)->data.bVal = true;
    (*jsonData)->len = sizeof((*jsonData)->data.bVal);

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != 't') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'r') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'u') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'e') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readFalse(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    LE_DEBUG("FALSE");
    (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN;
    (*jsonData)->data.bVal = false;
    (*jsonData)->len = sizeof((*jsonData)->data.bVal);

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != 'f') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'a') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'l') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 's') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'e') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readNull(uint8_t const** data, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);
    LE_ASSERT(jsonData && (*jsonData == NULL));

    *jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    LE_DEBUG("NULL");
    (*jsonData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_NULL;

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != 'n') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'u') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'l') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr != 'l') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readComment(uint8_t const** data, uint32_t* len)
{
    int32_t res = LE_FORMAT_ERROR;

    LE_ASSERT(data && *data);
    LE_ASSERT(len);

    LE_DEBUG("COMMENT");

    const uint8_t* ptr = (uint8_t*)*data;
    if (*ptr != '/') goto cleanup;
    if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

    if (*ptr == '/')
    {
        if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

        while ((*ptr != '\n') && (*ptr != '\r'))
        {
            if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
        }
    }
    else if (*ptr == '*')
    {
        if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

        while (1)
        {
            if (*ptr == '*')
            {
                if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;

                if (*ptr == '/') break;
            }

            if (!mangoh_bridge_json_readNext(&ptr, len)) goto cleanup;
        }
    }
    else
    {
        goto cleanup;
    }

    *data = ptr;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_json_readData(uint8_t const** ptr, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_OK;

    LE_ASSERT(ptr && *ptr);
    LE_ASSERT(len);
    LE_ASSERT(jsonData);

    res = mangoh_bridge_json_skipWhitespace(ptr, len, true);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
        goto cleanup;
    }

    if (!len)
    {
        LE_WARN("Nothing to read");
        res = -EINVAL;
        goto cleanup;
    }

    bool isDigit = mangoh_bridge_json_isDigit(*ptr);
    switch (*(*ptr))
    {
    case '{':
        res = mangoh_bridge_json_readObject(ptr, len, jsonData);
        break;

    case '[':
        res = mangoh_bridge_json_readArray(ptr, len, jsonData);
        break;

    case '"':
        res = mangoh_bridge_json_readString(ptr, len, jsonData);
        break;

    case '-':
        res = mangoh_bridge_json_readNumber(ptr, len, jsonData);
        break;

    case 't':
        res = mangoh_bridge_json_readTrue(ptr, len, jsonData);
        break;

    case 'f':
        res = mangoh_bridge_json_readFalse(ptr, len, jsonData);
        break;

    case '/':
        res = mangoh_bridge_json_readComment(ptr, len);
        break;

    case 'n':
        res = mangoh_bridge_json_readNull(ptr, len, jsonData);
        break;

    default:
        if (isDigit) res = mangoh_bridge_json_readNumber(ptr, len, jsonData);
        else res = LE_BAD_PARAMETER;
        break;
    }

cleanup:
     if (res != LE_OK) LE_WARN("input('%c') is not valid JSON", *(*ptr));
    return res;
}

static int mangoh_bridge_json_writeBool(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    const char* boolStr = jsonData->data.bVal ? "true":"false";
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN));
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    const uint32_t size = jsonData->data.bVal ? MANGOH_BRIDGE_JSON_TRUE_LEN:MANGOH_BRIDGE_JSON_FALSE_LEN;
    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("BOOL('%s')", jsonData->data.bVal ? MANGOH_BRIDGE_JSON_TRUE:MANGOH_BRIDGE_JSON_FALSE);
    memcpy(*ptr, boolStr, size);
    mangoh_bridge_json_writeNext(ptr, allocLen, len, size);

cleanup:
    return res;
}

static int mangoh_bridge_json_writeInteger(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_INT));
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    char buffer[MANGOH_BRIDGE_JSON_INTEGER_MAX_LEN] = {0};
    snprintf(buffer, MANGOH_BRIDGE_JSON_INTEGER_MAX_LEN, "%" PRId64, jsonData->data.iVal);

    const uint32_t size = strlen(buffer);
    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("INTEGER('%s')", buffer);
    memcpy(*ptr, buffer, size);
    mangoh_bridge_json_writeNext(ptr, allocLen, len, size);

cleanup:
    return res;
}

static int mangoh_bridge_json_writeFloat(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_FLOAT));
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    char buffer[MANGOH_BRIDGE_JSON_FLOAT_MAX_LEN] = {0};
    snprintf(buffer, MANGOH_BRIDGE_JSON_FLOAT_MAX_LEN, "%lf", jsonData->data.dVal);

    const uint32_t size = strlen(buffer);
    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("FLOAT('%s')", buffer);
    memcpy(*ptr, buffer, size);
    mangoh_bridge_json_writeNext(ptr, allocLen, len, size);

cleanup:
    return res;
}

static int mangoh_bridge_json_writeUnicode(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_UNICODE));
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    char unicodeStr[MANGOH_BRIDGE_JSON_UNICODE_BUFFER_LEN + strlen("\\u") + 1];
    snprintf(unicodeStr, MANGOH_BRIDGE_JSON_UNICODE_BUFFER_LEN, "\\u%x%x%x%x",
            jsonData->data.unicodeVal.bytes[0], jsonData->data.unicodeVal.bytes[1], jsonData->data.unicodeVal.bytes[2], jsonData->data.unicodeVal.bytes[3]);

    const uint32_t size = strlen(unicodeStr);
    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("UNICODE('%s')", unicodeStr);
    memcpy(*ptr, unicodeStr, size);
    mangoh_bridge_json_writeNext(ptr, allocLen, len, size);

cleanup:
    return res;
}

static int mangoh_bridge_json_writeString(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    char* repl = NULL;
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_STRING) && jsonData->data.strVal);
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    char* temp = malloc(jsonData->len + 1);
    if (!temp)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    strcpy(temp, jsonData->data.strVal);
    repl = strreplace(temp, "\"", "\\\"");
    if (!repl)
    {
        LE_ERROR("ERROR strreplace() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    free(temp);

    temp = strreplace((const char*)repl, "\f", "\\f");
    if (!temp)
    {
        LE_ERROR("ERROR strreplace() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    free(repl);
    repl = temp;

    temp = strreplace((const char*)repl, "\b", "\\b");
    if (!temp)
    {
        LE_ERROR("ERROR strreplace() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    free(repl);
    repl = temp;

    temp = strreplace((const char*)repl, "\r", "\\r");
    if (!temp)
    {
        LE_ERROR("ERROR strreplace() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    free(repl);
    repl = temp;

    temp = strreplace((const char*)repl, "\n", "\\n");
    if (!temp)
    {
        LE_ERROR("ERROR strreplace() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    free(repl);
    repl = temp;

    temp = strreplace((const char*)repl, "\t", "\\t");
    if (!temp)
    {
        LE_ERROR("ERROR strreplace() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    free(repl);
    repl = temp;

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("\""));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    **ptr = '"';
    mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("\""));

    const uint32_t size = strlen(repl);
    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("STRING('%s')", repl);
    memcpy(*ptr, repl, size);
    mangoh_bridge_json_writeNext(ptr, allocLen, len, size);

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("\""));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    **ptr = '"';
    mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("\""));

cleanup:
    if (repl) free(repl);
    return res;
}

static int mangoh_bridge_json_writeNull(uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    const char* nullStr = MANGOH_BRIDGE_JSON_NULL;
    int32_t res = LE_OK;

    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, MANGOH_BRIDGE_JSON_NULL_LEN);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("NULL");
    memcpy(*ptr, nullStr, MANGOH_BRIDGE_JSON_NULL_LEN);
    mangoh_bridge_json_writeNext(ptr, allocLen, len, MANGOH_BRIDGE_JSON_NULL_LEN);

cleanup:
    return res;
}

static int mangoh_bridge_json_writeArray(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY));
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("["));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("ARRAY BEGIN");
    **ptr = '[';
    mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("["));

    const le_sls_Link_t* link = le_sls_Peek(&jsonData->data.arrayVal);
    while (link)
    {
        mangoh_bridge_json_array_item_t* jsonItemData = CONTAINER_OF(link, mangoh_bridge_json_array_item_t, link);

        res = mangoh_bridge_json_writeData(jsonItemData->item, buff, allocLen, ptr, len);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_writeData() failed(%d)", res);
            res = LE_NO_MEMORY;
            goto cleanup;
        }

        res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen(","));
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
            goto cleanup;
        }

        link = le_sls_PeekNext(&jsonData->data.arrayVal, link);
        if (link)
        {
            **ptr = ',';
            mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen(","));
        }
    }

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("]"));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    **ptr = ']';
    mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("]"));
    LE_DEBUG("ARRAY END");

cleanup:
    return res;
}

static int mangoh_bridge_json_writeObject(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT));
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("{"));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("OBJECT BEGIN");
    **ptr = '{';
    mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("{"));

    const le_sls_Link_t* link = le_sls_Peek(&jsonData->data.objVal);
    while (link)
    {
        mangoh_bridge_json_array_obj_item_t* jsonItemData = CONTAINER_OF(link, mangoh_bridge_json_array_obj_item_t, link);

        res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("\""));
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
            goto cleanup;
        }

        **ptr = '"';
        mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("\""));

        const uint32_t size = strlen(jsonItemData->attribute);
        res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, size);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
            goto cleanup;
        }

        LE_DEBUG("ATTRIBUTE('%s')", jsonItemData->attribute);
        memcpy(*ptr, jsonItemData->attribute, strlen(jsonItemData->attribute));
        mangoh_bridge_json_writeNext(ptr, allocLen, len, size);

        res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("\""));
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
            goto cleanup;
        }

        **ptr = '"';
        mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("\""));

        res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen(":"));
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
            goto cleanup;
        }

        **ptr = ':';
        mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen(":"));

        if (jsonItemData->item)
        {
            res = mangoh_bridge_json_writeData(jsonItemData->item, buff, allocLen, ptr, len);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_json_writeData() failed(%d)", res);
                goto cleanup;
            }
        }
        else
        {
            res = mangoh_bridge_json_writeNull(buff, allocLen, ptr, len);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_json_writeNull() failed(%d)", res);
                goto cleanup;
            }
        }

        link = le_sls_PeekNext(&jsonData->data.objVal, link);
        if (link)
        {
            **ptr = ',';
            mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen(","));
        }
    }

    res = mangoh_bridge_json_checkOutputBufferSize(buff, ptr, allocLen, strlen("}"));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_checkOutputBufferSize() failed(%d)", res);
        goto cleanup;
    }

    **ptr = '}';
    mangoh_bridge_json_writeNext(ptr, allocLen, len, strlen("}"));
    LE_DEBUG("OBJECT END");

cleanup:
    return res;
}

static int mangoh_bridge_json_writeData(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* allocLen, uint8_t** ptr, uint32_t* len)
{
    int res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(buff);
    LE_ASSERT(allocLen);
    LE_ASSERT(ptr);
    LE_ASSERT(len);

    switch (jsonData->type)
    {
    case MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN:
        LE_DEBUG("BOOL");
        res = mangoh_bridge_json_writeBool(jsonData, buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_INT:
        LE_DEBUG("INTEGER");
        res = mangoh_bridge_json_writeInteger(jsonData, buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_FLOAT:
        LE_DEBUG("FLOAT");
        res = mangoh_bridge_json_writeFloat(jsonData, buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_UNICODE:
        LE_DEBUG("UNICODE");
        res = mangoh_bridge_json_writeUnicode(jsonData, buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_STRING:
        LE_DEBUG("STRING");
        res = mangoh_bridge_json_writeString(jsonData, buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_NULL:
        LE_DEBUG("NULL");
        res = mangoh_bridge_json_writeNull(buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY:
        LE_DEBUG("ARRAY");
        res = mangoh_bridge_json_writeArray(jsonData, buff, allocLen, ptr, len);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT:
        LE_DEBUG("OBJECT");
        res = mangoh_bridge_json_writeObject(jsonData, buff, allocLen, ptr, len);
        break;

    default:
        LE_ERROR("ERROR invalid JSON data type(%d)", jsonData->type);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

cleanup:
    return res;
}

static void mangoh_bridge_json_destroyData(mangoh_bridge_json_data_t** jsonData)
{
    LE_ASSERT(jsonData && *jsonData);

    if ((*jsonData)->type == MANGOH_BRIDGE_JSON_DATA_TYPE_STRING)
    {
        LE_DEBUG("STRING");
        if ((*jsonData)->data.strVal)
        {
            free((*jsonData)->data.strVal);
            (*jsonData)->data.strVal = NULL;
        }
    }
}

static int mangoh_bridge_json_destroyArray(mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && *jsonData);
    LE_ASSERT((*jsonData)->type == MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY);

    LE_DEBUG("ARRAY BEGIN");
    le_sls_Link_t* link = le_sls_Pop(&(*jsonData)->data.arrayVal);
    while (link)
    {
        mangoh_bridge_json_array_item_t* jsonItemData = CONTAINER_OF(link, mangoh_bridge_json_array_item_t, link);
        LE_ASSERT(jsonItemData && jsonItemData->item);

        LE_DEBUG("ITEM");
        res = mangoh_bridge_json_destroy(&jsonItemData->item);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroy() failed(%d)", res);
            goto cleanup;
        }

        LE_ASSERT(!jsonItemData->item);
        free(jsonItemData);
        jsonItemData = NULL;

        link = le_sls_Pop(&(*jsonData)->data.arrayVal);
    }

    LE_DEBUG("ARRAY END");
    LE_ASSERT(le_sls_IsEmpty(&(*jsonData)->data.arrayVal));
    free(*jsonData);
    *jsonData = NULL;

cleanup:
    return res;
}

static int mangoh_bridge_json_destroyObject(mangoh_bridge_json_data_t** jsonData)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData && *jsonData);
    LE_ASSERT((*jsonData)->type == MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT);

    LE_DEBUG("ARRAY BEGIN");
    le_sls_Link_t* link = le_sls_Pop(&(*jsonData)->data.objVal);
    while (link)
    {
        mangoh_bridge_json_array_obj_item_t* jsonItemData = CONTAINER_OF(link, mangoh_bridge_json_array_obj_item_t, link);
        LE_ASSERT(jsonItemData && jsonItemData->item);

        LE_DEBUG("ITEM");

        free(jsonItemData->attribute);
        jsonItemData->attribute = NULL;

        res = mangoh_bridge_json_destroy(&jsonItemData->item);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroy() failed(%d)", res);
            goto cleanup;
        }

        LE_ASSERT(!jsonItemData->item);
        free(jsonItemData);
        jsonItemData = NULL;

        link = le_sls_Pop(&(*jsonData)->data.objVal);
    }

    LE_DEBUG("ARRAY END");
    LE_ASSERT(le_sls_IsEmpty(&(*jsonData)->data.objVal));
    free(*jsonData);
    *jsonData = NULL;

cleanup:
    return res;
}

static int mangoh_bridge_json_getAttribute(const mangoh_bridge_json_data_t* jsonData, const char* attribute, mangoh_bridge_json_data_t** jsonSearchObj)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(attribute);
    LE_ASSERT(jsonSearchObj);

    if (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY)
    {
        const le_sls_Link_t* link = le_sls_Peek(&jsonData->data.arrayVal);
        if (link)
        {
            const mangoh_bridge_json_array_item_t* jsonItemData = CONTAINER_OF(link, mangoh_bridge_json_array_item_t, link);
            const mangoh_bridge_json_data_t* jsonObjData = (const mangoh_bridge_json_data_t*)(jsonItemData->item);

            if (jsonObjData->type != MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT)
            {
                LE_ERROR("invalid JSON command");
                res = LE_BAD_PARAMETER;
                goto cleanup;
            }

            const le_sls_Link_t* objLink = le_sls_Peek(&jsonObjData->data.objVal);
            while (objLink)
            {
                const mangoh_bridge_json_array_obj_item_t* jsonObjItemData = CONTAINER_OF(objLink, mangoh_bridge_json_array_obj_item_t, link);

                LE_DEBUG("key('%s')", jsonObjItemData->attribute);
                if (!strcmp(jsonObjItemData->attribute, attribute))
                {
                    *jsonSearchObj = jsonObjItemData->item;
                    break;
                }

                objLink = le_sls_PeekNext(&jsonObjData->data.objVal, objLink);
            }
        }
    }
    else if (jsonData->type == MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT)
    {
        const le_sls_Link_t* link = le_sls_Peek(&jsonData->data.objVal);
        while (link)
        {
            const mangoh_bridge_json_array_obj_item_t* jsonItemData = CONTAINER_OF(link, mangoh_bridge_json_array_obj_item_t, link);

            LE_DEBUG("key('%s')", jsonItemData->attribute);
            if (!strcmp(jsonItemData->attribute, attribute))
            {
                *jsonSearchObj = jsonItemData->item;
                break;
            }

            link = le_sls_PeekNext(&jsonData->data.objVal, link);
        }
    }
    else
    {
        LE_ERROR("ERROR invalid request");
        res = -EINVAL;
        goto cleanup;
    }

cleanup:
      return res;
}

int mangoh_bridge_json_getKey(const mangoh_bridge_json_data_t* jsonData, char** key)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(key);

    mangoh_bridge_json_data_t* jsonSearchData = NULL;
    res = mangoh_bridge_json_getAttribute(jsonData, MANGOH_BRIDGE_JSON_MESSAGE_KEY, &jsonSearchData);
    if (res != LE_OK)
    {
        LE_ERROR("mangoh_bridge_json_getAttribute() failed(%d)", res);
        goto cleanup;
    }

    LE_ASSERT(jsonSearchData);
    *key = jsonSearchData->data.strVal;

cleanup:
    return res;
}

int mangoh_bridge_json_getCommand(const mangoh_bridge_json_data_t* jsonData, char** cmd)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(cmd);

    mangoh_bridge_json_data_t* jsonSearchData = NULL;
    res = mangoh_bridge_json_getAttribute(jsonData, MANGOH_BRIDGE_JSON_MESSAGE_REQUEST, &jsonSearchData);
    if (res != LE_OK)
    {
        LE_ERROR("mangoh_bridge_json_getAttribute() failed(%d)", res);
        goto cleanup;
    }

    LE_ASSERT(jsonSearchData);
    *cmd = jsonSearchData->data.strVal;

cleanup:
      return res;
}

int mangoh_bridge_json_getValue(const mangoh_bridge_json_data_t* jsonData, mangoh_bridge_json_data_t** value)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(value);

    res = mangoh_bridge_json_getAttribute(jsonData, MANGOH_BRIDGE_JSON_MESSAGE_VALUE, value);
    if (res != LE_OK)
    {
        LE_ERROR("mangoh_bridge_json_getAttribute() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

int mangoh_bridge_json_getData(const mangoh_bridge_json_data_t* jsonData, mangoh_bridge_json_data_t** data)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(data);

    res = mangoh_bridge_json_getAttribute(jsonData, MANGOH_BRIDGE_JSON_MESSAGE_DATA, data);
    if (res != LE_OK)
    {
        LE_ERROR("mangoh_bridge_json_getAttribute() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

int mangoh_bridge_json_setRequestCommand(mangoh_bridge_json_data_t* jsonRspData, const char* cmd)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonRspData);
    LE_ASSERT(cmd);

    mangoh_bridge_json_data_t* jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonData->type = MANGOH_BRIDGE_JSON_DATA_TYPE_STRING;
    jsonData->len = strlen(cmd) + 1;
    jsonData->data.strVal = malloc(jsonData->len);
    if (!jsonData->data.strVal)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonData->data.strVal, cmd);

    mangoh_bridge_json_array_obj_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_obj_item_t));
    if (!jsonItemData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonItemData->attribute = malloc(strlen(MANGOH_BRIDGE_JSON_MESSAGE_REQUEST) + 1);
    if (!jsonItemData->attribute)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonItemData->attribute, MANGOH_BRIDGE_JSON_MESSAGE_REQUEST);

    jsonItemData->item = jsonData;
    jsonItemData->link = LE_SLS_LINK_INIT;
    le_sls_Queue(&jsonRspData->data.objVal, &jsonItemData->link);

cleanup:
    return res;
}

int mangoh_bridge_json_setData(mangoh_bridge_json_data_t* jsonRspData, const uint8_t* data, uint32_t len)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonRspData);
    LE_ASSERT(data);

    mangoh_bridge_json_data_t* jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonData->type = MANGOH_BRIDGE_JSON_DATA_TYPE_STRING;
    jsonData->len = len;
    jsonData->data.strVal = calloc(1, jsonData->len);
    if (!jsonData->data.strVal)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonData->data.strVal, (const char*)data);

    mangoh_bridge_json_array_obj_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_obj_item_t));
    if (!jsonItemData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonItemData->attribute = malloc(strlen(MANGOH_BRIDGE_JSON_MESSAGE_DATA) + 1);
    if (!jsonItemData->attribute)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonItemData->attribute, MANGOH_BRIDGE_JSON_MESSAGE_DATA);

    jsonItemData->item = jsonData;
    jsonItemData->link = LE_SLS_LINK_INIT;
    le_sls_Queue(&jsonRspData->data.objVal, &jsonItemData->link);

cleanup:
    return res;
}

int mangoh_bridge_json_copyObject(mangoh_bridge_json_data_t** dest, const mangoh_bridge_json_data_t* src)
{
    uint8_t* buff = NULL;
    uint32_t len = 0;
    int32_t res = LE_OK;

    LE_ASSERT(src);
    LE_ASSERT(dest && !dest);

    res = mangoh_bridge_json_write(src, &buff, &len);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_json_read(buff, &len, dest);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_read() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    if (buff) free(buff);
    return res;
}

int mangoh_bridge_json_createObject(mangoh_bridge_json_data_t** jsonRspData)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonRspData && (*jsonRspData == NULL));

    *jsonRspData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonRspData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    (*jsonRspData)->data.objVal = LE_SLS_LIST_INIT;
    (*jsonRspData)->len = sizeof((*jsonRspData)->data.objVal);
    (*jsonRspData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT;

cleanup:
    return res;
}

int mangoh_bridge_json_createArray(mangoh_bridge_json_data_t** jsonArrayData)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonArrayData && (*jsonArrayData == NULL));

    *jsonArrayData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!*jsonArrayData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    (*jsonArrayData)->len = sizeof((*jsonArrayData)->data.arrayVal);
    (*jsonArrayData)->type = MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY;
    (*jsonArrayData)->data.arrayVal = LE_SLS_LIST_INIT;

cleanup:
    return res;
}

int mangoh_bridge_json_setResponseCommand(mangoh_bridge_json_data_t* jsonRspData, const char* cmd)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonRspData);
    LE_ASSERT(cmd);

    mangoh_bridge_json_data_t* jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonData->type = MANGOH_BRIDGE_JSON_DATA_TYPE_STRING;
    jsonData->len = strlen(cmd) + 1;
    jsonData->data.strVal = calloc(1, jsonData->len);
    if (!jsonData->data.strVal)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonData->data.strVal, cmd);

    mangoh_bridge_json_array_obj_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_obj_item_t));
    if (!jsonItemData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonItemData->attribute = malloc(strlen(MANGOH_BRIDGE_JSON_MESSAGE_RESPONSE) + 1);
    if (!jsonItemData->attribute)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonItemData->attribute, MANGOH_BRIDGE_JSON_MESSAGE_RESPONSE);

    jsonItemData->item = jsonData;
    jsonItemData->link = LE_SLS_LINK_INIT;
    le_sls_Queue(&jsonRspData->data.objVal, &jsonItemData->link);

cleanup:
    return res;
}

int mangoh_bridge_json_setKey(mangoh_bridge_json_data_t* jsonRspData, const char* key)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonRspData);
    LE_ASSERT(key);

    mangoh_bridge_json_data_t* jsonData = calloc(1, sizeof(mangoh_bridge_json_data_t));
    if (!jsonData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonData->type = MANGOH_BRIDGE_JSON_DATA_TYPE_STRING;
    jsonData->len = strlen(key) + 1;
    jsonData->data.strVal = calloc(1, jsonData->len);
    if (!jsonData->data.strVal)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonData->data.strVal, key);

    mangoh_bridge_json_array_obj_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_obj_item_t));
    if (!jsonItemData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonItemData->attribute = malloc(strlen(MANGOH_BRIDGE_JSON_MESSAGE_KEY) + 1);
    if (!jsonItemData->attribute)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonItemData->attribute, MANGOH_BRIDGE_JSON_MESSAGE_KEY);

    jsonItemData->item = jsonData;
    jsonItemData->link = LE_SLS_LINK_INIT;
    le_sls_Queue(&jsonRspData->data.objVal, &jsonItemData->link);

cleanup:
    return res;
}

int mangoh_bridge_json_setValue(mangoh_bridge_json_data_t* jsonRspData, const mangoh_bridge_json_data_t* value)
{
    int32_t res = LE_OK;

    LE_ASSERT(jsonRspData);
    LE_ASSERT(value);

    mangoh_bridge_json_array_obj_item_t* jsonItemData = calloc(1, sizeof(mangoh_bridge_json_array_obj_item_t));
    if (!jsonItemData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    jsonItemData->attribute = malloc(strlen(MANGOH_BRIDGE_JSON_MESSAGE_VALUE) + 1);
    if (!jsonItemData->attribute)
    {
        LE_ERROR("ERROR malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }
    strcpy(jsonItemData->attribute, MANGOH_BRIDGE_JSON_MESSAGE_VALUE);

    jsonItemData->item = (mangoh_bridge_json_data_t*)value;
    jsonItemData->link = LE_SLS_LINK_INIT;
    le_sls_Queue(&jsonRspData->data.objVal, &jsonItemData->link);

cleanup:
    return res;
}

int mangoh_bridge_json_addObject(mangoh_bridge_json_data_t* jsonRspData, const mangoh_bridge_json_data_t* jsonItemData)
{
    int32_t res = LE_OK;

    mangoh_bridge_json_array_item_t* jsonArrayItem = calloc(1, sizeof(mangoh_bridge_json_array_item_t));
    if (!jsonItemData)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    mangoh_bridge_json_data_t* jsonDataCopy = NULL;
    res = mangoh_bridge_json_copyObject(&jsonDataCopy, jsonItemData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_copyObject() failed(%d)", res);
        goto cleanup;
    }

    jsonArrayItem->item = jsonDataCopy;
    jsonArrayItem->link = LE_SLS_LINK_INIT;
    le_sls_Queue(&jsonRspData->data.arrayVal, &jsonArrayItem->link);

cleanup:
    return res;
}

int mangoh_bridge_json_read(const uint8_t* const buff, uint32_t* len, mangoh_bridge_json_data_t** jsonData)
{
    int res = LE_OK;

    LE_ASSERT(buff);
    LE_ASSERT(len && *len);
    LE_ASSERT(jsonData && !*jsonData);

    const uint8_t* reversePtr = &buff[*len - 1];
    res = mangoh_bridge_json_skipWhitespace(&reversePtr, len, false);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_skipWhitespace() failed(%d)", res);
        goto cleanup;
    }

    if (!*len || buff[*len - 1] != '}')
    {
        LE_DEBUG("Rx partial JSON object('%c') bytes(%u)", buff[*len - 1], *len);
        res = LE_UNDERFLOW;
        goto cleanup;
    }

    const uint8_t* ptr = (uint8_t*)buff;
    res = mangoh_bridge_json_readData(&ptr, len, jsonData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_readData() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    if (res && *jsonData)
    {
        int32_t err = mangoh_bridge_json_destroy(jsonData);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroy() failed(%d)", err);
        }
    }

    return res;
}

int mangoh_bridge_json_write(const mangoh_bridge_json_data_t* jsonData, uint8_t** buff, uint32_t* len)
{
    int res = LE_OK;

    LE_ASSERT(jsonData);
    LE_ASSERT(buff);
    LE_ASSERT(len);

    uint32_t allocLen = MANGOH_BRIDGE_JSON_BUFFER_ALLOC_LEN;
    *buff = malloc(MANGOH_BRIDGE_JSON_BUFFER_ALLOC_LEN);
    if (!*buff)
    {
        LE_ERROR("malloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    uint8_t* ptr = *buff;
    res = mangoh_bridge_json_writeData(jsonData, buff, &allocLen, &ptr, len);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_json_writeData() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

int mangoh_bridge_json_destroy(mangoh_bridge_json_data_t** jsonData)
{
    int res = LE_OK;

    LE_ASSERT(jsonData && *jsonData);

    switch ((*jsonData)->type)
    {
    case MANGOH_BRIDGE_JSON_DATA_TYPE_NULL:
    case MANGOH_BRIDGE_JSON_DATA_TYPE_BOOLEAN:
    case MANGOH_BRIDGE_JSON_DATA_TYPE_INT:
    case MANGOH_BRIDGE_JSON_DATA_TYPE_FLOAT:
    case MANGOH_BRIDGE_JSON_DATA_TYPE_UNICODE:
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_STRING:
        mangoh_bridge_json_destroyData(jsonData);
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_ARRAY:
        res = mangoh_bridge_json_destroyArray(jsonData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroyArray() failed(%d)", res);
            goto cleanup;
        }
        break;

    case MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT:
        res = mangoh_bridge_json_destroyObject(jsonData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_json_destroyObject() failed(%d)", res);
            goto cleanup;
        }
        break;

    default:
        LE_ERROR("ERROR invalid JSON data type(%d)", (*jsonData)->type);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    free(*jsonData);
    *jsonData = NULL;

cleanup:
    return res;
}
