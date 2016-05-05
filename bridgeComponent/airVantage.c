#include <arpa/inet.h>
#include "legato.h"
#include "bridge.h"
#include "airVantage.h"

static int mangoh_bridge_air_vantage_sessionStart(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_sessionEnd(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_subscribe(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_pushBoolean(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_pushInteger(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_pushFloat(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_pushString(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_available(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_air_vantage_recv(void*, const unsigned char*, uint32_t);

static void mangoh_bridge_air_vantage_removedataUpdateHdlrs(mangoh_bridge_air_vantage_t*);
static void mangoh_bridge_air_vantage_dataUpdateHdlr(dataRouter_DataType_t, const char*, void*);

static void mangoh_bridge_air_vantage_removedataUpdateHdlrs(mangoh_bridge_air_vantage_t* airVantage)
{
  LE_ASSERT(airVantage);

  if (!le_hashmap_isEmpty(airVantage->dataUpdateHandlers))
  {
    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(airVantage->dataUpdateHandlers);

    int32_t res = le_hashmap_NextNode(iter);
    while (res == LE_OK)
    {
      dataRouter_DataUpdateHandlerRef_t dataUpdateHandlerRef = (dataRouter_DataUpdateHandlerRef_t)le_hashmap_GetValue(iter);
      if (dataUpdateHandlerRef)
      {
        dataRouter_RemoveDataUpdateHandler(dataUpdateHandlerRef);
      }

      res = le_hashmap_NextNode(iter);
    }

    le_hashmap_RemoveAll(airVantage->dataUpdateHandlers);
  }
}

static int mangoh_bridge_air_vantage_sessionStart(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> SESSION START");
    uint8_t* ptr = (uint8_t*)data;

    uint8_t pushAv = *ptr;
    ptr += sizeof(pushAv);
    LE_DEBUG("AV push(%u)", pushAv);

    uint8_t storage = *ptr;
    ptr += sizeof(storage);
    LE_DEBUG("storage(%u)", storage);

    uint8_t len = 0;
    memcpy(&len, ptr, sizeof(len));
    LE_DEBUG("len(%u)", len);
    ptr += sizeof(len);

    char url[MANGOH_BRIDGE_AIR_VANTAGE_VALUE_MAX_LEN] = {0};
    memcpy(url, ptr, len);
    LE_DEBUG("url('%s')", url);
    ptr += len;

    char pw[MANGOH_BRIDGE_AIR_VANTAGE_VALUE_MAX_LEN] = {0};
    memcpy(pw, ptr, size - len - sizeof(len) - sizeof(pushAv) - sizeof(storage));
    LE_DEBUG("pw('%s')", pw);

    dataRouter_SessionStart(url, pw, pushAv, storage);

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_sessionEnd(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> SESSSION END");

    mangoh_bridge_air_vantage_removedataUpdateHdlrs(airVantage);
    dataRouter_SessionEnd();

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_subscribe(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> SUBSCRIBE");
    uint8_t* ptr = (uint8_t*)data;

    uint8_t len = 0;
    memcpy(&len, ptr, sizeof(len));
    LE_DEBUG("len(%u)", len);
    ptr += sizeof(len);

    char fieldName[MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN] = {0};
    memcpy(fieldName, ptr, len);
    LE_DEBUG("field('%s')", fieldName);

    dataRouter_DataUpdateHandlerRef_t dataUpdateHandlerRef = le_hashmap_Get(airVantage->dataUpdateHandlers, fieldName);
    if (!dataUpdateHandlerRef)
    {
        LE_DEBUG("add data update handler('%s')", fieldName);
        dataUpdateHandlerRef = dataRouter_AddDataUpdateHandler(fieldName, mangoh_bridge_air_vantage_dataUpdateHdlr, airVantage);
        if (!dataUpdateHandlerRef)
        {
            LE_ERROR("ERROR dataRouter_AddDataUpdateHandler() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        if (le_hashmap_Put(airVantage->dataUpdateHandlers, fieldName, dataUpdateHandlerRef))
        {
            LE_ERROR("ERROR le_hashmap_Put() failed");
            res = LE_FAULT;
            goto cleanup;
        }
    }

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_pushBoolean(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> PUSH BOOLEAN");
    uint8_t* ptr = (uint8_t*)data;

    uint8_t len = 0;
    memcpy(&len, ptr, sizeof(len));
    LE_DEBUG("len(%u)", len);
    ptr += sizeof(len);

    char fieldName[MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN] = {0};
    memcpy(fieldName, ptr, len);
    LE_DEBUG("field('%s')", fieldName);
    ptr += len;

    uint8_t val = 0;
    memcpy(&val, ptr, sizeof(val));
    LE_DEBUG("value('%s')", val ? "true":"false");

    dataRouter_WriteBoolean(fieldName, val, time(NULL));

    dataRouter_DataUpdateHandlerRef_t dataUpdateHandlerRef = le_hashmap_Get(airVantage->dataUpdateHandlers, fieldName);
    if (!dataUpdateHandlerRef)
    {
        LE_DEBUG("add data update handler('%s')", fieldName);
        dataUpdateHandlerRef = dataRouter_AddDataUpdateHandler(fieldName, mangoh_bridge_air_vantage_dataUpdateHdlr, airVantage);
        if (!dataUpdateHandlerRef)
        {
            LE_ERROR("ERROR dataRouter_AddDataUpdateHandler() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        if (le_hashmap_Put(airVantage->dataUpdateHandlers, fieldName, dataUpdateHandlerRef))
        {
            LE_ERROR("ERROR le_hashmap_Put() failed");
            res = LE_FAULT;
            goto cleanup;
        }
    }

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_pushInteger(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> PUSH INTEGER");
    uint8_t* ptr = (uint8_t*)data;

    uint8_t len = 0;
    memcpy(&len, ptr, sizeof(len));
    LE_DEBUG("len(%u)", len);
    ptr += sizeof(len);

    char fieldName[MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN] = {0};
    memcpy(fieldName, ptr, len);
    LE_DEBUG("field('%s')", fieldName);
    ptr += len;

    int32_t val = 0;
    memcpy(&val, ptr, sizeof(val));
    LE_DEBUG("value(%d)", val);

    dataRouter_WriteInteger(fieldName, val, time(NULL));

    dataRouter_DataUpdateHandlerRef_t dataUpdateHandlerRef = le_hashmap_Get(airVantage->dataUpdateHandlers, fieldName);
    if (!dataUpdateHandlerRef)
    {
        LE_DEBUG("add data update handler('%s')", fieldName);
        dataUpdateHandlerRef = dataRouter_AddDataUpdateHandler(fieldName, mangoh_bridge_air_vantage_dataUpdateHdlr, airVantage);
        if (!dataUpdateHandlerRef)
        {
            LE_ERROR("ERROR dataRouter_AddDataUpdateHandler() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        if (le_hashmap_Put(airVantage->dataUpdateHandlers, fieldName, dataUpdateHandlerRef))
        {
            LE_ERROR("ERROR le_hashmap_Put() failed");
            res = LE_FAULT;
            goto cleanup;
        }
    }

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_pushFloat(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    double fVal = 0;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> PUSH FLOAT");
    uint8_t* ptr = (uint8_t*)data;

    uint8_t len = 0;
    memcpy(&len, ptr, sizeof(len));
    LE_DEBUG("len(%u)", len);
    ptr += sizeof(len);

    char fieldName[MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN] = {0};
    memcpy(fieldName, ptr, len);
    LE_DEBUG("field('%s')", fieldName);
    ptr += len;

    int8_t precision = 0;
    memcpy(&precision, ptr, sizeof(precision));
    LE_DEBUG("precision(%u)", precision);
    ptr += sizeof(precision);

    int32_t val = 0;
    memcpy(&val, ptr, sizeof(val));
    LE_DEBUG("value(%d)", val);
    fVal = val;
    while (precision-- > 0) fVal /= 10.0;

    LE_DEBUG("write(%f)", fVal);
    dataRouter_WriteFloat(fieldName, fVal, time(NULL));

    dataRouter_DataUpdateHandlerRef_t dataUpdateHandlerRef = le_hashmap_Get(airVantage->dataUpdateHandlers, fieldName);
    if (!dataUpdateHandlerRef)
    {
        LE_DEBUG("add data update handler('%s')", fieldName);
        dataUpdateHandlerRef = dataRouter_AddDataUpdateHandler(fieldName, mangoh_bridge_air_vantage_dataUpdateHdlr, airVantage);
        if (!dataUpdateHandlerRef)
        {
            LE_ERROR("ERROR dataRouter_AddDataUpdateHandler() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        if (le_hashmap_Put(airVantage->dataUpdateHandlers, fieldName, dataUpdateHandlerRef))
        {
            LE_ERROR("ERROR le_hashmap_Put() failed");
            res = LE_FAULT;
            goto cleanup;
        }
    }

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_pushString(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> PUSH STRING");
    uint8_t* ptr = (uint8_t*)data;

    uint8_t len = 0;
    memcpy(&len, ptr, sizeof(len));
    LE_DEBUG("len(%u)", len);
    ptr += sizeof(len);

    char fieldName[MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN] = {0};
    memcpy(fieldName, ptr, len);
    LE_DEBUG("field('%s')", fieldName);
    ptr += len;

    char val[MANGOH_BRIDGE_AIR_VANTAGE_VALUE_MAX_LEN] = {0};
    memcpy(val, ptr, size - len - sizeof(len));
    LE_DEBUG("value('%s')", val);

    dataRouter_WriteString(fieldName, val, time(NULL));

    dataRouter_DataUpdateHandlerRef_t dataUpdateHandlerRef = le_hashmap_Get(airVantage->dataUpdateHandlers, fieldName);
    if (!dataUpdateHandlerRef)
    {
        LE_DEBUG("add data update handler('%s')", fieldName);
        dataUpdateHandlerRef = dataRouter_AddDataUpdateHandler(fieldName, mangoh_bridge_air_vantage_dataUpdateHdlr, airVantage);
        if (!dataUpdateHandlerRef)
        {
            LE_ERROR("ERROR dataRouter_AddDataUpdateHandler() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        if (le_hashmap_Put(airVantage->dataUpdateHandlers, fieldName, dataUpdateHandlerRef))
        {
            LE_ERROR("ERROR le_hashmap_Put() failed");
            res = LE_FAULT;
            goto cleanup;
        }
    }

    res = mangoh_bridge_sendResult(airVantage->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_available(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;
    le_log_TraceRef_t traceRef = mangoh_bridge_getTraceRef();

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_TRACE(traceRef, "---> AVAIL");

    mangoh_bridge_air_vantage_avail_rsp_t* const rsp = (mangoh_bridge_air_vantage_avail_rsp_t*)((mangoh_bridge_t*)airVantage->bridge)->packet.msg.data;

    LE_TRACE(traceRef, "Rx buffer length(%u)", airVantage->rxBuffLen);
    rsp->result = htons(airVantage->rxBuffLen);
    LE_TRACE(traceRef, "result(%d)", rsp->result);
    res = mangoh_bridge_sendResult(airVantage->bridge, sizeof(rsp->result));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_air_vantage_recv(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);
    LE_ASSERT(data);

    LE_DEBUG("---> RECV");

    if (airVantage->rxBuffLen)
    {
        mangoh_bridge_air_vantage_recv_rsp_t* const rsp = (mangoh_bridge_air_vantage_recv_rsp_t*)((mangoh_bridge_t*)airVantage->bridge)->packet.msg.data;
        uint8_t rdLen = (sizeof(rsp->data) > airVantage->rxBuffLen) ? airVantage->rxBuffLen:sizeof(rsp->data);

        memcpy(rsp->data, (const char*)airVantage->rxBuffer, rdLen);
        memmove(airVantage->rxBuffer, &airVantage->rxBuffer[rdLen], airVantage->rxBuffLen - rdLen);
        memset(&airVantage->rxBuffer[airVantage->rxBuffLen - rdLen], 0, rdLen);
        airVantage->rxBuffLen -= rdLen;

        LE_DEBUG("result(%u)", rdLen);
        res = mangoh_bridge_sendResult(airVantage->bridge, rdLen);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        res = mangoh_bridge_sendResult(airVantage->bridge, 0);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }

cleanup:
    return res;
}

static void mangoh_bridge_air_vantage_dataUpdateHdlr(dataRouter_DataType_t type, const char* key, void* context)
{
    char msg[MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN] = {0};
    uint32_t timestamp = 0;
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)context;
    uint16_t len = 0;

    LE_ASSERT(context);
    LE_ASSERT(key);

    LE_DEBUG("--> key('%s')", key);

    switch (type)
    {
    case DATAROUTER_BOOLEAN:
    {
      bool value = false;
      dataRouter_ReadBoolean(key, &value, &timestamp);

      time_t ts = timestamp;
      snprintf(msg, MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN, "|%s|%d|%s", key, value, ctime(&ts));
      break;
    }
    case DATAROUTER_INTEGER:
    {
      int32_t value = 0;
      dataRouter_ReadInteger(key, &value, &timestamp);

      time_t ts = timestamp;
      snprintf(msg, MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN, "|%s|%d|%s", key, value, ctime(&ts));
      break;
    }
    case DATAROUTER_FLOAT:
    {
      double value = 0;
      dataRouter_ReadFloat(key, &value, &timestamp);

      time_t ts = timestamp;
      snprintf(msg, MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN, "|%s|%f|%s", key, value, ctime(&ts));
      break;
    }
    case DATAROUTER_STRING:
    {
      char value[MANGOH_BRIDGE_AIR_VANTAGE_VALUE_MAX_LEN] = {0};
      dataRouter_ReadString(key, value, sizeof(value), &timestamp);

      time_t ts = timestamp;
      snprintf(msg, MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN, "|%s|%s|%s", key, value, ctime(&ts));
      break;
    }
    }

    LE_DEBUG("'%s'", msg);
    len = strlen(msg);
    if (len + airVantage->rxBuffLen > sizeof(airVantage->rxBuffer))
    {
        LE_ERROR("ERROR Rx buffer overflow(%u + %u > %u)", len, airVantage->rxBuffLen, sizeof(airVantage->rxBuffer));
        goto cleanup;
    }

    strncpy((char*)&airVantage->rxBuffer[airVantage->rxBuffLen], msg, sizeof(airVantage->rxBuffer) - airVantage->rxBuffLen);
    airVantage->rxBuffLen += len;
    LE_DEBUG("Rx buffer('%s') length(%u)", airVantage->rxBuffer, airVantage->rxBuffLen);

cleanup:
    return;
}

static int mangoh_bridge_air_vantage_reset(void* param)
{
    mangoh_bridge_air_vantage_t* airVantage = (mangoh_bridge_air_vantage_t*)param;

    LE_ASSERT(airVantage);

    mangoh_bridge_air_vantage_removedataUpdateHdlrs(airVantage);
    dataRouter_SessionEnd();
    return LE_OK;
}

int mangoh_bridge_air_vantage_init(mangoh_bridge_air_vantage_t* airVantage, void* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(airVantage);

    LE_DEBUG("init");

    airVantage->bridge = bridge;
    airVantage->dataUpdateHandlers = le_hashmap_Create(MANGOH_BRIDGE_AIR_VANTAGE_DATA_UPDATE_MAP_NAME, MANGOH_BRIDGE_AIR_VANTAGE_DATA_UPDATE_MAP_SIZE,
        le_hashmap_HashString, le_hashmap_EqualsString);

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_SESSION_START, airVantage, mangoh_bridge_air_vantage_sessionStart);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_SESSION_END, airVantage, mangoh_bridge_air_vantage_sessionEnd);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_SUBSCRIBE, airVantage, mangoh_bridge_air_vantage_subscribe);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_PUSH_BOOLEAN, airVantage, mangoh_bridge_air_vantage_pushBoolean);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_PUSH_INTEGER, airVantage, mangoh_bridge_air_vantage_pushInteger);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_PUSH_FLOAT, airVantage, mangoh_bridge_air_vantage_pushFloat);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_PUSH_STRING, airVantage, mangoh_bridge_air_vantage_pushString);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_AVAILABLE, airVantage, mangoh_bridge_air_vantage_available);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(airVantage->bridge, MANGOH_BRIDGE_AIR_VANTAGE_RECV, airVantage, mangoh_bridge_air_vantage_recv);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerReset(airVantage->bridge, airVantage, mangoh_bridge_air_vantage_reset);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerReset() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    LE_DEBUG("init completed(%d)", res);
    return res;
}

int mangoh_bridge_air_vantage_destroy(mangoh_bridge_air_vantage_t* airVantage)
{
    mangoh_bridge_air_vantage_removedataUpdateHdlrs(airVantage);
    dataRouter_SessionEnd();
    return LE_OK;
}
