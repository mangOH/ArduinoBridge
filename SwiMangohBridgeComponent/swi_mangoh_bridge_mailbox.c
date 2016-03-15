#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "legato.h"
#include "swi_mangoh_bridge_tcpClient.h"
#include "swi_mangoh_bridge_tcpServer.h"
#include "swi_mangoh_bridge.h"
#include "swi_mangoh_bridge_json.h"
#include "swi_mangoh_bridge_mailbox.h"

static int swi_mangoh_bridge_mailbox_send(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_mailbox_send_json(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_mailbox_recv(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_mailbox_available(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_mailbox_datastorePut(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_mailbox_datastoreGet(void*, const unsigned char*, uint32_t);

static int swi_mangoh_bridge_mailbox_processRawCommand(swi_mangoh_bridge_mailbox_t*, const swi_mangoh_bridge_json_data_t*);
static int swi_mangoh_bridge_mailbox_processGetCommand(swi_mangoh_bridge_mailbox_t*, const swi_mangoh_bridge_json_data_t*);
static int swi_mangoh_bridge_mailbox_processPutCommand(swi_mangoh_bridge_mailbox_t*, const swi_mangoh_bridge_json_data_t*);
static int swi_mangoh_bridge_mailbox_processDeleteCommand(swi_mangoh_bridge_mailbox_t*, const swi_mangoh_bridge_json_data_t*);
static int swi_mangoh_bridge_mailbox_processCommands(swi_mangoh_bridge_mailbox_t*);

static int swi_mangoh_bridge_mailbox_runner(void*);
static int swi_mangoh_bridge_mailbox_reset(void*);

static int swi_mangoh_bridge_mailbox_send(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    swi_mangoh_bridge_json_data_t* jsonReqData = NULL;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(data);

    const swi_mangoh_bridge_mailbox_send_req_t* const req = (swi_mangoh_bridge_mailbox_send_req_t*)data;
    LE_DEBUG("---> SEND");

    mailbox->jsonMsgLen = size;
    mailbox->jsonMsg = calloc(1, mailbox->jsonMsgLen);
    if (!mailbox->jsonMsg)
    {
        LE_ERROR("ERROR calloc() failed(");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_createObject(&jsonReqData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_createObject() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setRequestCommand(jsonReqData, SWI_MANGOH_BRIDGE_MAILBOX_RAW_COMMAND);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setRequestCommand() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setData(jsonReqData, req->data, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setData() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_write(jsonReqData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_client_write(&mailbox->clients, mailbox->jsonMsg, mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_client_write() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("result(%d)", res);
    res = swi_mangoh_bridge_sendResult(mailbox->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    if (jsonReqData)
    {
        int32_t err = swi_mangoh_bridge_json_destroy(&jsonReqData);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", err);
            res = res ? res:err;
        }
    }

    mailbox->jsonMsgLen = 0;
    if (mailbox->jsonMsg)
    {
        free(mailbox->jsonMsg);
        mailbox->jsonMsg = NULL;
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_send_json(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    swi_mangoh_bridge_json_data_t* jsonReqData = NULL;
    uint32_t len = size;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(data);

    const swi_mangoh_bridge_mailbox_json_send_req_t* const req = (swi_mangoh_bridge_mailbox_json_send_req_t*)data;
    LE_DEBUG("---> SEND JSON");

    res = swi_mangoh_bridge_json_read(req->data, &len, &jsonReqData);
    if (res)
    {
        res = swi_mangoh_bridge_json_createObject(&jsonReqData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_createObject() failed(%d)", res);
            goto cleanup;
        }

        res = swi_mangoh_bridge_json_setRequestCommand(jsonReqData, SWI_MANGOH_BRIDGE_MAILBOX_RAW_COMMAND);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_setRequestCommand() failed(%d)", res);
            goto cleanup;
        }

        res = swi_mangoh_bridge_json_setData(jsonReqData, req->data, size);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_setData() failed(%d)", res);
            goto cleanup;
        }
    }

    res = swi_mangoh_bridge_json_write(jsonReqData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_client_write(&mailbox->clients, mailbox->jsonMsg, mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_client_write() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("result(%d)", res);
    res = swi_mangoh_bridge_sendResult(mailbox->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    if (jsonReqData)
    {
        int32_t err = swi_mangoh_bridge_json_destroy(&jsonReqData);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", err);
            res = res ? res:err;
        }
    }

    mailbox->jsonMsgLen = 0;
    if (mailbox->jsonMsg)
    {
        free(mailbox->jsonMsg);
        mailbox->jsonMsg = NULL;
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_recv(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(data);

    LE_DEBUG("---> RECV");

    if (mailbox->rxBuffLen)
    {
        swi_mangoh_bridge_mailbox_recv_rsp_t* const rsp = (swi_mangoh_bridge_mailbox_recv_rsp_t*)((swi_mangoh_bridge_t*)mailbox->bridge)->packet.msg.data;
        uint8_t rdLen = (sizeof(rsp->data) > mailbox->rxBuffLen) ? mailbox->rxBuffLen:sizeof(rsp->data);

        memcpy(rsp->data, (const char*)mailbox->rxBuffer, rdLen);
        memmove(mailbox->rxBuffer, &mailbox->rxBuffer[rdLen], mailbox->rxBuffLen - rdLen);
        memset(&mailbox->rxBuffer[mailbox->rxBuffLen - rdLen], 0, rdLen);
        mailbox->rxBuffLen -= rdLen;

        LE_DEBUG("result(%u)", rdLen);
        res = swi_mangoh_bridge_sendResult(mailbox->bridge, rdLen);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        res = swi_mangoh_bridge_sendResult(mailbox->bridge, 0);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_mailbox_available(void* param, const unsigned char* data, uint32_t size)
{
    const swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(data);

    LE_DEBUG("---> AVAILABLE");

    swi_mangoh_bridge_mailbox_available_rsp_t* const rsp = (swi_mangoh_bridge_mailbox_available_rsp_t*)((swi_mangoh_bridge_t*)mailbox->bridge)->packet.msg.data;

    rsp->len = htons(mailbox->rxBuffLen);
    LE_DEBUG("result(%u)", mailbox->rxBuffLen);
    res = swi_mangoh_bridge_sendResult(mailbox->bridge, sizeof(swi_mangoh_bridge_mailbox_available_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_mailbox_datastorePut(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(data);

    const swi_mangoh_bridge_mailbox_datastore_put_req_t* const req = (swi_mangoh_bridge_mailbox_datastore_put_req_t*)data;
    LE_DEBUG("---> DATASTORE PUT");

    const char search[2] = { SWI_MANGOH_BRIDGE_MAILBOX_SEPARATOR, 0 };
    char* params[SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_PARAMS] = {0};
    char* token = strtok((char*)req->data, search);
    uint16_t idx = 0;

    while (token)
    {
        LE_DEBUG("arg('%s')", token);
        params[idx++] = token;
        token = strtok(NULL, search);
    }

    swi_mangoh_bridge_mailbox_datastore_put_rsp_t* const rsp = (swi_mangoh_bridge_mailbox_datastore_put_rsp_t*)((swi_mangoh_bridge_t*)mailbox->bridge)->packet.msg.data;
    if (idx == SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_PARAMS)
    {
        swi_mangoh_bridge_json_data_t* jsonData = NULL;
        uint32_t len = strlen(params[SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_VALUE_IDX]);

        res = swi_mangoh_bridge_json_read((const uint8_t*)params[SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_VALUE_IDX], &len, &jsonData);
        if (res != LE_OK)
        {
            LE_WARN("WARNING invalid request");
            rsp->result = false;
        }
        else
        {
            if (le_hashmap_Put(mailbox->database, params[SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_KEY_IDX], jsonData))
            {
                LE_WARN("WARNING object replaced");
            }

            rsp->result = true;
        }
    }
    else
    {
        LE_WARN("WARNING invalid request");
        rsp->result = false;
    }

    LE_DEBUG("result(%d)", rsp->result);
    res = swi_mangoh_bridge_sendResult(mailbox->bridge, sizeof(swi_mangoh_bridge_mailbox_datastore_put_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_mailbox_datastoreGet(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(data);

    const swi_mangoh_bridge_mailbox_datastore_get_req_t* const req = (swi_mangoh_bridge_mailbox_datastore_get_req_t*)data;
    LE_DEBUG("---> DATASTORE GET('%s')", req->key);

    swi_mangoh_bridge_mailbox_datastore_get_rsp_t* const rsp = (swi_mangoh_bridge_mailbox_datastore_get_rsp_t*)((swi_mangoh_bridge_t*)mailbox->bridge)->packet.msg.data;
    swi_mangoh_bridge_json_data_t* jsonData = NULL;

    jsonData = le_hashmap_Get(mailbox->database, req->key);
    if (jsonData)
    {
        LE_ASSERT(!mailbox->jsonMsg);
        res = swi_mangoh_bridge_json_write(jsonData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
            goto cleanup;
        }

        memcpy(rsp->data, mailbox->jsonMsg, mailbox->jsonMsgLen);
        LE_DEBUG("result(%u)", mailbox->jsonMsgLen);
        res = swi_mangoh_bridge_sendResult(mailbox->bridge, mailbox->jsonMsgLen);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        res = swi_mangoh_bridge_sendResult(mailbox->bridge, 0);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_mailbox_processRawCommand(swi_mangoh_bridge_mailbox_t* mailbox, const swi_mangoh_bridge_json_data_t* jsonReqData)
{
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(jsonReqData);

    swi_mangoh_bridge_json_data_t* jsonData = NULL;
    res = swi_mangoh_bridge_json_getData(jsonReqData, &jsonData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_getData() failed(%d)", res);
        goto cleanup;
    }
    else if (!jsonData)
    {
        LE_ERROR("ERROR invalid JSON RAW request");
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    LE_ASSERT(!mailbox->jsonMsgLen && !mailbox->jsonMsg);
    res = swi_mangoh_bridge_json_write(jsonData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    LE_DEBUG("RAW response(%u)", mailbox->jsonMsgLen);
    if (mailbox->rxBuffLen + mailbox->jsonMsgLen > sizeof(mailbox->rxBuffer))
    {
        LE_ERROR("ERROR mailbox Rx buffer overflow");
        res = LE_OVERFLOW;
        goto cleanup;
    }

    memcpy(&mailbox->rxBuffer[mailbox->rxBuffLen], mailbox->jsonMsg, mailbox->jsonMsgLen);
    mailbox->rxBuffLen += mailbox->jsonMsgLen;
    LE_DEBUG("Rx buffer(%u)", mailbox->rxBuffLen);

cleanup:
    if (mailbox->jsonMsg)
    {
        free(mailbox->jsonMsg);
        mailbox->jsonMsg = NULL;
        mailbox->jsonMsgLen = 0;
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_processGetCommand(swi_mangoh_bridge_mailbox_t* mailbox, const swi_mangoh_bridge_json_data_t* jsonReqData)
{
    swi_mangoh_bridge_json_data_t* jsonRspData = NULL;
    int32_t res = LE_OK;
    int32_t err = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(jsonReqData);

    char* key = NULL;
    res = swi_mangoh_bridge_json_getKey(jsonReqData, &key);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_getKey() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_createObject(&jsonRspData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_createObject() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setResponseCommand(jsonRspData, SWI_MANGOH_BRIDGE_MAILBOX_GET_COMMAND);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setResponseCommand() failed(%d)", res);
        goto cleanup;
    }

    if (key)
    {
        LE_DEBUG("GET('%s')", key);
        res = swi_mangoh_bridge_json_setKey(jsonRspData, key);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_setKey() failed(%d)", res);
            goto cleanup;
        }

        swi_mangoh_bridge_json_data_t* jsonDataCopy = NULL;
        swi_mangoh_bridge_json_data_t* value = NULL;

        value = le_hashmap_Get(mailbox->database, key);
        if (!value)
        {
            LE_WARN("WARNING JSON object('%s') not found", key);
        }
        else
        {
            res = swi_mangoh_bridge_json_copyObject(&jsonDataCopy, value);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR swi_mangoh_bridge_json_copyObject() failed(%d)", res);
                goto cleanup;
            }

            res = swi_mangoh_bridge_json_setValue(jsonRspData, jsonDataCopy);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR swi_mangoh_bridge_json_setValue() failed(%d)", res);
                goto cleanup;
            }
        }
    }
    else
    {
        swi_mangoh_bridge_json_data_t* jsonArrayData = NULL;
        res = swi_mangoh_bridge_json_createArray(&jsonArrayData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_createArray() failed(%d)", res);
            goto cleanup;
        }

        le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(mailbox->database);
        res =  le_hashmap_NextNode(iter);
        while (res == LE_OK)
        {
            const char* key = (char*)le_hashmap_GetKey(iter);
            if (!key)
            {
                LE_ERROR("ERROR le_hashmap_GetKey() failed");
                res = LE_NOT_FOUND;
                goto cleanup;
            }

            LE_DEBUG("key('%s')", key);

            const swi_mangoh_bridge_json_data_t* value = (swi_mangoh_bridge_json_data_t*)le_hashmap_GetValue(iter);
            if (!value)
            {
                LE_ERROR("ERROR le_hashmap_GetValue() failed");
                res = LE_NOT_FOUND;
                goto cleanup;
            }

            res = swi_mangoh_bridge_json_addObject(jsonArrayData, value);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR swi_mangoh_bridge_json_addObject() failed(%d)", res);
                goto cleanup;
            }

            res = le_hashmap_NextNode(iter);
        }

        res = swi_mangoh_bridge_json_setValue(jsonRspData, jsonArrayData);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_setValue() failed(%d)", res);
            goto cleanup;
        }
    }

    LE_ASSERT(!mailbox->jsonMsgLen);
    LE_ASSERT(!mailbox->jsonMsg);

    res = swi_mangoh_bridge_json_write(jsonRspData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_client_write(&mailbox->clients, mailbox->jsonMsg, mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_client_write() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    err = swi_mangoh_bridge_json_destroy(&jsonRspData);
    if (err != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", err);
        res = res ? res:err;
    }

    if (mailbox->jsonMsg)
    {
        free(mailbox->jsonMsg);
        mailbox->jsonMsg = NULL;
        mailbox->jsonMsgLen = 0;
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_processPutCommand(swi_mangoh_bridge_mailbox_t* mailbox, const swi_mangoh_bridge_json_data_t* jsonReqData)
{
    swi_mangoh_bridge_json_data_t* jsonRspData = NULL;
    int32_t res = LE_OK;
    int32_t err = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(jsonReqData);

    char* key = NULL;
    res = swi_mangoh_bridge_json_getKey(jsonReqData, &key);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_getKey() failed(%d)", res);
        goto cleanup;
    }
    else if (!key)
    {
        LE_ERROR("ERROR invalid JSON PUT request");
        res = LE_NOT_FOUND;
        goto cleanup;
    }

    LE_DEBUG("PUT('%s')", key);

    swi_mangoh_bridge_json_data_t* value = NULL;
    res = swi_mangoh_bridge_json_getValue(jsonReqData, &value);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_getValue() failed(%d)", res);
        goto cleanup;
    }
    else if (!value)
    {
        LE_ERROR("ERROR invalid JSON PUT request");
        res = LE_NOT_FOUND;
        goto cleanup;
    }

    if (le_hashmap_Put(mailbox->database, key, value))
    {
        LE_DEBUG("REPLACED('%s')", key);
    }
    else
    {
        LE_DEBUG("ADDED('%s')", key);
    }

    res = swi_mangoh_bridge_json_createObject(&jsonRspData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_createObject() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setResponseCommand(jsonRspData, SWI_MANGOH_BRIDGE_MAILBOX_PUT_COMMAND);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setResponseCommand() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setKey(jsonRspData, key);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setKey() failed(%d)", res);
        goto cleanup;
    }

    swi_mangoh_bridge_json_data_t* putValue = NULL;
    putValue = le_hashmap_Get(mailbox->database, key);
    if (!putValue)
    {
        LE_ERROR("ERROR JSON object('%s') not found", key);
        res = LE_NOT_FOUND;
        goto cleanup;
    }

    swi_mangoh_bridge_json_data_t* jsonDataCopy = NULL;
    res = swi_mangoh_bridge_json_copyObject(&jsonDataCopy, putValue);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_copyObject() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setValue(jsonRspData, jsonDataCopy);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setValue() failed(%d)", res);
        goto cleanup;
    }

    LE_ASSERT(!mailbox->jsonMsgLen);
    LE_ASSERT(!mailbox->jsonMsg);

    res = swi_mangoh_bridge_json_write(jsonRspData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_client_write(&mailbox->clients, mailbox->jsonMsg, mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_client_write() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    err = swi_mangoh_bridge_json_destroy(&jsonRspData);
    if (err != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", err);
        res = res ? res:err;
    }

    if (mailbox->jsonMsg)
    {
        free(mailbox->jsonMsg);
        mailbox->jsonMsg = NULL;
        mailbox->jsonMsgLen = 0;
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_processDeleteCommand(swi_mangoh_bridge_mailbox_t* mailbox, const swi_mangoh_bridge_json_data_t* jsonReqData)
{
    swi_mangoh_bridge_json_data_t* jsonRspData = NULL;
    int32_t res = LE_OK;
    int32_t err = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(jsonReqData);

    char* key = NULL;
    res = swi_mangoh_bridge_json_getKey(jsonReqData, &key);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_getKey() failed(%d)", res);
        goto cleanup;
    }
    else if (!key)
    {
        LE_ERROR("ERROR invalid JSON DELETE request");
        res = LE_NOT_FOUND;
        goto cleanup;
    }

    LE_DEBUG("DELETE('%s')", key);

    res = swi_mangoh_bridge_json_createObject(&jsonRspData);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_createObject() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setResponseCommand(jsonRspData, SWI_MANGOH_BRIDGE_MAILBOX_DELETE_COMMAND);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setResponseCommand() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_json_setKey(jsonRspData, key);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_setKey() failed(%d)", res);
        goto cleanup;
    }

    swi_mangoh_bridge_json_data_t* value = NULL;
    value = le_hashmap_Get(mailbox->database, key);
    if (value)
    {
        if (!le_hashmap_Remove(mailbox->database, key))
        {
            LE_ERROR("ERROR le_hashmap_Remove() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        res = swi_mangoh_bridge_json_setValue(jsonRspData, value);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_setValue() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        LE_DEBUG("key('%s') not found", key);
    }

    res = swi_mangoh_bridge_json_write(jsonRspData, &mailbox->jsonMsg, &mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_write() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_client_write(&mailbox->clients, mailbox->jsonMsg, mailbox->jsonMsgLen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_client_write() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    err = swi_mangoh_bridge_json_destroy(&jsonRspData);
    if (err != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", err);
        res = res ? res:err;
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_processCommands(swi_mangoh_bridge_mailbox_t* mailbox)
{
    swi_mangoh_bridge_json_data_t* jsonReqData = NULL;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);

    uint32_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if (mailbox->clients.info[idx].recvBuffLen > 0)
        {
            uint32_t len = mailbox->clients.info[idx].recvBuffLen;
            LE_DEBUG("Rx data(%u)", len);

            jsonReqData = NULL;
            res = swi_mangoh_bridge_json_read((const uint8_t*)mailbox->clients.info[idx].rxBuffer, &len, &jsonReqData);
            if (res == LE_OK)
            {
                LE_ASSERT(jsonReqData && (jsonReqData->type == SWI_MANGOH_BRIDGE_JSON_DATA_TYPE_OBJECT));

                memmove(mailbox->clients.info[idx].rxBuffer, mailbox->clients.info[idx].rxBuffer + mailbox->clients.info[idx].recvBuffLen + len, len);
                mailbox->clients.info[idx].recvBuffLen = len;

                char* command = NULL;
                res = swi_mangoh_bridge_json_getCommand(jsonReqData, &command);
                if (res != LE_OK)
                {
                    LE_ERROR("swi_mangoh_bridge_json_getCommand() failed(%d)", res);
                    mailbox->clients.info[idx].recvBuffLen = 0;
                    memset(mailbox->clients.info[idx].rxBuffer, 0, SWI_MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN);
                    continue;
                }

                LE_ASSERT(command);
                if (!strcmp(command, SWI_MANGOH_BRIDGE_MAILBOX_RAW_COMMAND))
                {
                    LE_DEBUG("--> RAW");
                    res = swi_mangoh_bridge_mailbox_processRawCommand(mailbox, jsonReqData);
                    if (res != LE_OK)
                    {
                        LE_ERROR("ERROR swi_mangoh_bridge_mailbox_processRawCommand() failed(%d)", res);
                        goto cleanup;
                    }
                }
                else if (!strcmp(command, SWI_MANGOH_BRIDGE_MAILBOX_GET_COMMAND))
                {
                    LE_DEBUG("--> GET");
                    res = swi_mangoh_bridge_mailbox_processGetCommand(mailbox, jsonReqData);
                    if (res != LE_OK)
                    {
                        LE_ERROR("ERROR swi_mangoh_bridge_mailbox_processGetCommand() failed(%d)", res);
                        goto cleanup;
                    }
                }
                else if (!strcmp(command, SWI_MANGOH_BRIDGE_MAILBOX_PUT_COMMAND))
                {
                    LE_DEBUG("--> PUT");
                    res = swi_mangoh_bridge_mailbox_processPutCommand(mailbox, jsonReqData);
                    if (res != LE_OK)
                    {
                        LE_ERROR("ERROR swi_mangoh_bridge_mailbox_processPutCommand() failed(%d)", res);
                        goto cleanup;
                    }
                }
                else if (!strcmp(command, SWI_MANGOH_BRIDGE_MAILBOX_DELETE_COMMAND))
                {
                    LE_DEBUG("--> DELETE");
                    res = swi_mangoh_bridge_mailbox_processDeleteCommand(mailbox, jsonReqData);
                    if (res != LE_OK)
                    {
                        LE_ERROR("ERROR swi_mangoh_bridge_mailbox_processDeleteCommand() failed(%d)", res);
                        goto cleanup;
                    }
                }
                else
                {
                    LE_ERROR("ERROR invalid command('%s')", command);

                    mailbox->clients.info[idx].recvBuffLen = 0;
                    memset(mailbox->clients.info[idx].rxBuffer, 0, SWI_MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN);

                    res = LE_BAD_PARAMETER;
                    goto cleanup;
                }

                res = swi_mangoh_bridge_json_destroy(&jsonReqData);
                if (res != LE_OK)
                {
                    LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", res);
                    goto cleanup;
                }

                LE_ASSERT(!jsonReqData);
            }
            else if (mailbox->clients.info[idx].recvBuffLen == SWI_MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN)
            {
                LE_ERROR("ERROR JSON invalid object size(> %u)", SWI_MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN);
                mailbox->clients.info[idx].recvBuffLen = 0;
                memset(mailbox->clients.info[idx].rxBuffer, 0, SWI_MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN);
                goto cleanup;
            }
            else if (res == LE_UNDERFLOW)
            {
                res = LE_OK;
            }
        }
    }

cleanup:
    if (jsonReqData)
    {
        int32_t err = swi_mangoh_bridge_json_destroy(&jsonReqData);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_json_destroy() failed(%d)", err);
        }
    }

    return res;
}

static int swi_mangoh_bridge_mailbox_runner(void* param)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);

    res = swi_mangoh_bridge_tcp_server_acceptNewConnections(&mailbox->server, &mailbox->clients);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_server_acceptNewConnections() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_client_run(&mailbox->clients);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_client_run() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_mailbox_processCommands(mailbox);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_mailbox_processCommands() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_mailbox_reset(void* param)
{
    swi_mangoh_bridge_mailbox_t* mailbox = (swi_mangoh_bridge_mailbox_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);

    uint8_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        if (mailbox->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
        {
            LE_DEBUG("close socket[%u](%d)", idx, mailbox->clients.info[idx].sockFd);
            res = close(mailbox->clients.info[idx].sockFd);
            if (res < 0)
            {
                LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
                res = LE_IO_ERROR;
                goto cleanup;
            }

            mailbox->clients.info[idx].sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;
            mailbox->clients.info[idx].recvBuffLen = 0;
            mailbox->clients.info[idx].sendBuffLen = 0;
        }
    }

    mailbox->clients.nextId = 0;
    mailbox->clients.broadcast = 0;
    mailbox->clients.maxSockFd = 0;

cleanup:
    return res;
}

int swi_mangoh_bridge_mailbox_init(swi_mangoh_bridge_mailbox_t* mailbox, void* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(mailbox);
    LE_ASSERT(bridge);

    LE_DEBUG("init");

    mailbox->bridge = bridge;
    mailbox->database = le_hashmap_Create("Bridge Mbox", SWI_MANGOH_BRIDGE_MAILBOX_DATA_STORE_SIZE, le_hashmap_HashString, le_hashmap_EqualsString);

    swi_mangoh_bridge_tcp_client_init(&mailbox->clients, false);

    res = swi_mangoh_bridge_tcp_server_start(&mailbox->server, SWI_MANGOH_BRIDGE_MAILBOX_SERVER_IP_ADDR, SWI_MANGOH_BRIDGE_MAILBOX_JSON_SERVER_PORT, SWI_MANGOH_BRIDGE_MAILBOX_SERVER_BACKLOG);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_server_start() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(mailbox->bridge, SWI_MANGOH_BRIDGE_MAILBOX_SEND, mailbox, swi_mangoh_bridge_mailbox_send);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(mailbox->bridge, SWI_MANGOH_BRIDGE_MAILBOX_SEND_JSON, mailbox, swi_mangoh_bridge_mailbox_send_json);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(mailbox->bridge, SWI_MANGOH_BRIDGE_MAILBOX_RECV, mailbox, swi_mangoh_bridge_mailbox_recv);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(mailbox->bridge, SWI_MANGOH_BRIDGE_MAILBOX_AVAILABLE, mailbox, swi_mangoh_bridge_mailbox_available);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(mailbox->bridge, SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_PUT, mailbox, swi_mangoh_bridge_mailbox_datastorePut);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(mailbox->bridge, SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_GET, mailbox, swi_mangoh_bridge_mailbox_datastoreGet);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerRunner(mailbox->bridge, mailbox, swi_mangoh_bridge_mailbox_runner);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerRunner() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerReset(mailbox->bridge, mailbox, swi_mangoh_bridge_mailbox_reset);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerReset() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    LE_DEBUG("init completed(%d)", res);
    return res;
}

int swi_mangoh_bridge_mailbox_destroy(swi_mangoh_bridge_mailbox_t* mailbox)
{
    int32_t res = LE_OK;

    res = swi_mangoh_bridge_tcp_client_destroy(&mailbox->clients);
    if (res < 0)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_server_stop() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_tcp_server_stop(&mailbox->server);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_tcp_server_stop() failed(%d)", res);
    }

cleanup:
    return res;
}




