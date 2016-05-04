#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "legato.h"
#include "bridge.h"
#include "tcpClient.h"
#include "tcpServer.h"
#include "console.h"

static int mangoh_bridge_console_write(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_console_read(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_console_connected(void*, const unsigned char*, uint32_t);

static int mangoh_bridge_console_runner(void*);
static int mangoh_bridge_console_reset(void*);

static int mangoh_bridge_console_write(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_console_t* console = (mangoh_bridge_console_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(console);
    LE_ASSERT(data);

    const mangoh_bridge_write_read_req_t* const req = (mangoh_bridge_write_read_req_t*)data;
    LE_DEBUG("---> WRITE '%s' size(%u)", req->data, size);

    res = mangoh_bridge_tcp_client_write(&console->clients, req->data, size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_client_write() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_sendResult(console->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_console_read(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_console_t* console = (mangoh_bridge_console_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(console);
    LE_ASSERT(data);

    const mangoh_bridge_console_read_req_t* const req = (mangoh_bridge_console_read_req_t*)data;
    LE_DEBUG("---> READ length(%u)", req->len);

    res = mangoh_bridge_tcp_client_getReceivedData(&console->clients, &console->rxBuffer[console->rxBuffLen], &console->rxBuffLen, sizeof(console->rxBuffer));
    if (res)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_client_getReceivedData() failed(%d)", res);
        goto cleanup;
    }

    if (console->rxBuffLen)
    {
        uint8_t rdLen = (req->len > console->rxBuffLen) ? console->rxBuffLen:req->len;

        mangoh_bridge_console_read_rsp_t* const rsp = (mangoh_bridge_console_read_rsp_t*)((mangoh_bridge_t*)console->bridge)->packet.msg.data;
        memcpy(rsp->data, (const char*)console->rxBuffer, rdLen);
        memmove(console->rxBuffer, &console->rxBuffer[rdLen], console->rxBuffLen - rdLen);
        memset(&console->rxBuffer[console->rxBuffLen - rdLen], 0, rdLen);
        console->rxBuffLen -= rdLen;

        LE_INFO("result(%d) '%s'", rdLen, rsp->data);
        res = mangoh_bridge_sendResult(console->bridge, rdLen);
        if (res)
        {
            LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        res = mangoh_bridge_sendResult(console->bridge, 0);
        if (res)
        {
            LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }

cleanup:
    return res;
}

static int mangoh_bridge_console_connected(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_console_t* console = (mangoh_bridge_console_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(console);

    LE_DEBUG("---> CONNECTED");

    mangoh_bridge_console_connected_rsp_t* const rsp = (mangoh_bridge_console_connected_rsp_t*)((mangoh_bridge_t*)console->bridge)->packet.msg.data;
    mangoh_bridge_tcp_client_connected(&console->clients, &rsp->result);

    LE_DEBUG("result(%d)", rsp->result);
    res = mangoh_bridge_sendResult(console->bridge, sizeof(mangoh_bridge_console_connected_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_console_runner(void* param)
{
    mangoh_bridge_console_t* console = (mangoh_bridge_console_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(console);

    res = mangoh_bridge_tcp_server_acceptNewConnections(&console->server, &console->clients);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_server_acceptNewConnections() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_tcp_client_run(&console->clients);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_client_run() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_console_reset(void* param)
{
    mangoh_bridge_console_t* console = (mangoh_bridge_console_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(console);

    uint8_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        if (console->clients.info[idx].sockFd != MANGOH_BRIDGE_SOCKETS_INVALID)
        {
            LE_DEBUG("close socket[%u](%d)", idx, console->clients.info[idx].sockFd);
            res = close(console->clients.info[idx].sockFd);
            if (res < 0)
            {
                LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
                res = LE_IO_ERROR;
                goto cleanup;
            }

            console->clients.info[idx].sockFd = MANGOH_BRIDGE_SOCKETS_INVALID;
            console->clients.info[idx].recvBuffLen = 0;
            console->clients.info[idx].sendBuffLen = 0;
        }
    }

    console->clients.nextId = 0;
    console->clients.broadcast = 0;
    console->clients.maxSockFd = 0;

cleanup:
    return res;
}

int mangoh_bridge_console_init(mangoh_bridge_console_t* console, void* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(console);

    LE_DEBUG("init");

    console->bridge = bridge;

    mangoh_bridge_tcp_client_init(&console->clients, true);

    res = mangoh_bridge_tcp_server_start(&console->server, MANGOH_BRIDGE_CONSOLE_SERVER_IP_ADDR,
            MANGOH_BRIDGE_CONSOLE_SERVER_PORT, MANGOH_BRIDGE_CONSOLE_SERVER_BACKLOG);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_server_start() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(console->bridge, MANGOH_BRIDGE_CONSOLE_WRITE, console, mangoh_bridge_console_write);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(console->bridge, MANGOH_BRIDGE_CONSOLE_READ, console, mangoh_bridge_console_read);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(console->bridge, MANGOH_BRIDGE_CONSOLE_CONNECTED, console, mangoh_bridge_console_connected);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerRunner(console->bridge, console, mangoh_bridge_console_runner);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerRunner() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerReset(console->bridge, console, mangoh_bridge_console_reset);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerReset() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    LE_DEBUG("init completed(%d)", res);
    return res;
}

int mangoh_bridge_console_destroy(mangoh_bridge_console_t* console)
{
    int32_t res = LE_OK;

    res = mangoh_bridge_tcp_client_destroy(&console->clients);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_server_stop() failed(%d)", res);
        goto cleanup;
    }

    res =  mangoh_bridge_tcp_server_stop(&console->server);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_server_stop() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}
