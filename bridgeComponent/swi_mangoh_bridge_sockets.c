#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "legato.h"
#include "swi_mangoh_bridge.h"
#include "swi_mangoh_bridge_sockets.h"

static int swi_mangoh_bridge_sockets_listen(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_accept(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_read(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_write(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_connected(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_close(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_connecting(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_connect(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_sockets_writeToAll(void*, const unsigned char*, uint32_t);

static int swi_mangoh_bridge_sockets_closeServer(swi_mangoh_bridge_sockets_server_t*);
static int swi_mangoh_bridge_sockets_closeClient(swi_mangoh_bridge_sockets_client_info_t*);
static int swi_mangoh_bridge_sockets_checkConnections(swi_mangoh_bridge_sockets_client_info_t*);
static int swi_mangoh_bridge_sockets_checkClients(uint16_t, swi_mangoh_bridge_sockets_client_t*);

static int swi_mangoh_bridge_sockets_runner(void*);
static int swi_mangoh_bridge_sockets_reset(void*);

static int swi_mangoh_bridge_sockets_listen(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_listen_req_t* const req = (swi_mangoh_bridge_sockets_listen_req_t*)data;
    sockets->server.port = ntohs(req->port);

    LE_DEBUG("---> LISTEN('%s:%u')", req->serverIP, sockets->server.port);

    if (sockets->server.sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        res = swi_mangoh_bridge_sockets_closeServer(&sockets->server);
        if (res)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeServer() failed(%d)", res);
            goto cleanup;
        }
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* servinfo = NULL;
    char portStr[SWI_MANGOH_BRIDGE_SOCKETS_PORT_STRING_LEN] = {0};
    snprintf(portStr, SWI_MANGOH_BRIDGE_SOCKETS_PORT_STRING_LEN, "%u", sockets->server.port);
    res = getaddrinfo((const char*)req->serverIP, portStr, &hints, &servinfo);
    if (res < 0)
    {
        if (res == EAI_AGAIN)
        {
            LE_WARN("WARNING getaddrinfo() name server temporary failure indication");
            res = LE_COMM_ERROR;
        }
        else
        {
            LE_ERROR("ERROR getaddrinfo() failed(%d/%d)", res, errno);
            res = LE_BAD_PARAMETER;
        }

        goto cleanup;
    }

    struct addrinfo* p = NULL;
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        char addrstr[SWI_MANGOH_BRIDGE_SOCKETS_SERVER_IP_LEN] = {0};
        void* ptr = NULL;

        switch (p->ai_family)
        {
        case AF_INET:
            ptr = &((struct sockaddr_in*)p->ai_addr)->sin_addr;
            break;

        case AF_INET6:
            ptr = &((struct sockaddr_in6*)p->ai_addr)->sin6_addr;
            break;
        }

        inet_ntop(p->ai_family, ptr, addrstr, sizeof(addrstr));
        LE_DEBUG("IPv%u address: '%s' ('%s')", p->ai_family == PF_INET6 ? 6 : 4, addrstr, p->ai_canonname);

        sockets->server.sockFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockets->server.sockFd < 0)
        {
            LE_ERROR("ERROR sockets() failed(%d/%d)", sockets->server.sockFd, errno);
            res = LE_FAULT;
            continue;
        }

        uint32_t reuseAddr = 1;
        res = setsockopt(sockets->server.sockFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));
        if (res < 0)
        {
            LE_ERROR("ERROR setsockopt() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        res = bind(sockets->server.sockFd, p->ai_addr, p->ai_addrlen);
        if (res < 0)
        {
            LE_ERROR("ERROR bind() failed(%d/%d)", res, errno);

            res = swi_mangoh_bridge_sockets_closeServer(&sockets->server);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeServer() failed(%d)", res);
                goto cleanup;
            }

            continue;
        }

        break;
    }

    if (p == NULL)
    {
        LE_ERROR("ERROR bind('%s:%u') failed", req->serverIP, sockets->server.port);
        res = LE_FAULT;
        goto cleanup;
    }

    res = fcntl(sockets->server.sockFd, F_SETFL, O_NONBLOCK);
    if (res < 0)
    {
        LE_ERROR("ERROR fcntl() failed(%d/%d)", res, errno);
        res = LE_FAULT;
        goto cleanup;
    }

    res = listen(sockets->server.sockFd, SWI_MANGOH_BRIDGE_SOCKETS_SERVER_BACKLOG);
    if (res < 0)
    {
        LE_ERROR("ERROR listen() failed(%d/%d)", res, errno);
        res = LE_COMM_ERROR;
        goto cleanup;
    }

    swi_mangoh_bridge_sockets_listen_rsp_t* const rsp = (swi_mangoh_bridge_sockets_listen_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;
    rsp->result = true;
    LE_DEBUG("result(%d)", rsp->result);
    res = swi_mangoh_bridge_sendResult(sockets->bridge, sizeof(swi_mangoh_bridge_sockets_listen_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    if (res)
    {
        int32_t err = swi_mangoh_bridge_sockets_closeServer(&sockets->server);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeServer() failed(%d)", err);
        }

        swi_mangoh_bridge_sockets_listen_rsp_t* const rsp = (swi_mangoh_bridge_sockets_listen_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;
        rsp->result = false;
        LE_DEBUG("result(%d)", rsp->result);
        err = swi_mangoh_bridge_sendResult(sockets->bridge, sizeof(swi_mangoh_bridge_sockets_listen_rsp_t));
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", err);
        }
    }

    return res;
}

static int swi_mangoh_bridge_sockets_accept(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    LE_DEBUG("---> ACCEPT");

    if (sockets->server.sockFd == SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        LE_ERROR("ERROR server socket closed");
        res = -EINVAL;
        goto cleanup;
    }

    swi_mangoh_bridge_sockets_accept_rsp_t* const rsp = (swi_mangoh_bridge_sockets_accept_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;

    FD_ZERO(&sockets->server.readfds);
    FD_SET(sockets->server.sockFd, &sockets->server.readfds);

    struct timeval tv  = {0};
    res = select(sockets->server.sockFd + 1, &sockets->server.readfds, NULL, NULL, &tv);
    if (res < 0)
    {
        LE_ERROR("ERROR select() failed(%d/%d)", errno, res);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    if (FD_ISSET(sockets->server.sockFd, &sockets->server.readfds))
    {
        struct sockaddr_in clientAddr = {0};
        uint32_t clientAddrSize = 0;
        int32_t newFd = accept(sockets->server.sockFd, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (newFd < 0)
        {
            LE_ERROR("ERROR accept() socket(%d) failed(%d/%d)", sockets->server.sockFd, newFd, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        char clientIPStr[INET_ADDRSTRLEN] = {0};
        LE_INFO("connection -> '%s'", inet_ntop(AF_INET, &clientAddr.sin_addr, clientIPStr, INET_ADDRSTRLEN));

        res = fcntl(newFd, F_SETFL, O_NONBLOCK);
        if (res < 0)
        {
            LE_ERROR("ERROR fcntl() socket(%d) failed(%d/%d)", newFd, res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        if (sockets->clients.info[sockets->clients.nextId].sockFd == SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
        {
            LE_INFO("add sockets[%u](%d)", sockets->clients.nextId, newFd);
            sockets->clients.info[sockets->clients.nextId].sockFd = newFd;
            sockets->clients.info[sockets->clients.nextId].connected = true;
            rsp->result = sockets->clients.nextId;
            sockets->clients.nextId = (sockets->clients.nextId + 1) % SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS;
        }
        else
        {
            LE_ERROR("ERROR socket[%u](%d) not closed", sockets->clients.nextId, sockets->clients.info[sockets->clients.nextId].sockFd);

            res = close(newFd);
            if (res < 0)
            {
                LE_ERROR("ERROR close() socket(%d) failed(%d/%d)", newFd, res, errno);
                res = LE_IO_ERROR;
                goto cleanup;
            }

            res = LE_BAD_PARAMETER;
            goto cleanup;
        }

        FD_CLR(sockets->server.sockFd, &sockets->server.readfds);

        LE_DEBUG("result(%d)", rsp->result);
        res = swi_mangoh_bridge_sendResult(sockets->bridge, sizeof(swi_mangoh_bridge_sockets_accept_rsp_t));
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        res = swi_mangoh_bridge_sendResult(sockets->bridge, 0);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }

cleanup:
    if (res)
    {
        int32_t err = swi_mangoh_bridge_sendResult(sockets->bridge, 0);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", err);
        }
    }

    return res;
}

static int swi_mangoh_bridge_sockets_read(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_read_req_t* const req = (swi_mangoh_bridge_sockets_read_req_t*)data;
    const uint8_t id = req->id;
    const uint8_t len = req->len;
    LE_DEBUG("---> READ(%u) length(%u)", id, len);

    if (id >= SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS)
    {
        LE_ERROR("ERROR invalid socket id(%d)", id);
        res = -EINVAL;
        goto cleanup;
    }

    swi_mangoh_bridge_sockets_read_rsp_t* const rsp = (swi_mangoh_bridge_sockets_read_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;

    uint32_t bytesRead = 0;
    LE_DEBUG("socket[%u](%d) Rx buffer length(%u)", id, sockets->clients.info[id].sockFd, sockets->clients.info[id].rxBuff.len);
    if (sockets->clients.info[id].rxBuff.len)
    {
        if (sockets->clients.info[id].rxBuff.len < len)
        {
            memcpy(rsp->data, sockets->clients.info[id].rxBuff.data, sockets->clients.info[id].rxBuff.len);
            bytesRead = sockets->clients.info[id].rxBuff.len;
            sockets->clients.info[id].rxBuff.len = 0;
        }
        else
        {
            memcpy(rsp->data, sockets->clients.info[id].rxBuff.data, len);
            memmove(sockets->clients.info[id].rxBuff.data, &sockets->clients.info[id].rxBuff.data[len], sockets->clients.info[id].rxBuff.len - len);
            sockets->clients.info[id].rxBuff.len -=  len;
            bytesRead = len;
        }
    }

    LE_DEBUG("result(%d)", bytesRead);
    res = swi_mangoh_bridge_sendResult(sockets->bridge, bytesRead);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_write(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_write_req_t* const req = (swi_mangoh_bridge_sockets_write_req_t*)data;
    const uint8_t len = size - sizeof(req->id);
    LE_DEBUG("---> WRITE(%u) len(%u)", req->id, len);

    if (req->id >= SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS)
    {
        LE_ERROR("ERROR invalid socket id(%d)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    if (sockets->clients.info[req->id].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        if (sockets->clients.info[req->id].txBuff.len + len < SWI_MANGOH_BRIDGE_SOCKETS_BUFF_LEN)
        {
            memcpy(&sockets->clients.info[req->id].txBuff.data[sockets->clients.info[req->id].txBuff.len], req->data, len);
            sockets->clients.info[req->id].txBuff.len += len;
            LE_DEBUG("socket[%u](%d) Tx buffer length(%u)", req->id, sockets->clients.info[req->id].sockFd, sockets->clients.info[req->id].txBuff.len);
        }
        else
        {
            LE_ERROR("ERROR socket[%u](%d) Tx buffer overflow", req->id, sockets->clients.info[req->id].sockFd);
            res = LE_OVERFLOW;
            goto cleanup;
        }
    }
    else
    {
        LE_ERROR("ERROR socket[%u](%d) not connected", req->id, sockets->clients.info[req->id].sockFd);
        res = LE_CLOSED;
        goto cleanup;
    }

    res = swi_mangoh_bridge_sendResult(sockets->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_connected(void* param, const unsigned char* data, uint32_t size)
{
    const swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_connected_req_t* const req = (swi_mangoh_bridge_sockets_connected_req_t*)data;
    LE_DEBUG("---> CONNECTED(%d)", req->id);

    if (req->id >= SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS)
    {
        LE_ERROR("ERROR invalid socket id(%d)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    swi_mangoh_bridge_sockets_connected_rsp_t* const rsp = (swi_mangoh_bridge_sockets_connected_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;
    rsp->result = ((sockets->clients.info[req->id].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) && sockets->clients.info[req->id].connected) ? true:false;
    LE_DEBUG("socket(%d) connected(%u)", sockets->clients.info[req->id].sockFd, sockets->clients.info[req->id].connected);

    LE_DEBUG("result(%d)", rsp->result);
    res = swi_mangoh_bridge_sendResult(sockets->bridge, sizeof(swi_mangoh_bridge_sockets_connected_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_close(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_close_req_t* const req = (swi_mangoh_bridge_sockets_close_req_t*)data;
    LE_DEBUG("---> CLOSE(%u)", req->id);

    if (req->id >= SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS)
    {
        LE_ERROR("ERROR invalid socket id(%d)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    if (sockets->clients.info[req->id].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        res = swi_mangoh_bridge_sockets_closeClient(&sockets->clients.info[req->id]);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", res);
            goto cleanup;
        }
    }

    res = swi_mangoh_bridge_sendResult(sockets->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_connecting(void* param, const unsigned char* data, uint32_t size)
{
    const swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_connecting_req_t* const req = (swi_mangoh_bridge_sockets_connecting_req_t*)data;
    LE_DEBUG("---> CONNECTING(%d)", req->id);

    if (req->id >= SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS)
    {
        LE_ERROR("ERROR invalid socket id(%d)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    swi_mangoh_bridge_sockets_connecting_rsp_t* const rsp = (swi_mangoh_bridge_sockets_connecting_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;
    rsp->result = ((sockets->clients.info[req->id].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) && sockets->clients.info[req->id].connecting) ? true:false;
    LE_DEBUG("socket(%d) connecting(%u)", sockets->clients.info[req->id].sockFd, sockets->clients.info[req->id].connecting);

    LE_DEBUG("result(%d)", rsp->result);
    res = swi_mangoh_bridge_sendResult(sockets->bridge, sizeof(swi_mangoh_bridge_sockets_connecting_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_connect(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_connect_req_t* const req = (swi_mangoh_bridge_sockets_connect_req_t*)data;
    const uint16_t port = ntohs(req->port);
    LE_DEBUG("---> CONNECT('%s:%u')", req->serverIP, port);

    swi_mangoh_bridge_sockets_connect_rsp_t* const rsp = (swi_mangoh_bridge_sockets_connect_rsp_t*)((swi_mangoh_bridge_t*)sockets->bridge)->packet.msg.data;

    if (sockets->clients.info[sockets->clients.nextId].sockFd == SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        sockets->clients.info[sockets->clients.nextId].sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets->clients.info[sockets->clients.nextId].sockFd < 0)
        {
            LE_ERROR("ERROR socket() failed(%d/%d)", res, errno);
            res = sockets->clients.info[sockets->clients.nextId].sockFd;
            goto cleanup;
        }

        res = fcntl(sockets->clients.info[sockets->clients.nextId].sockFd, F_SETFL, O_NONBLOCK);
        if (res < 0)
        {
            LE_ERROR("ERROR fcntl() socket(%d) failed(%d/%d)",
                    sockets->clients.info[sockets->clients.nextId].sockFd, res, errno);
            goto cleanup;
        }

        struct addrinfo hints = {0};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        struct addrinfo* servinfo = NULL;
        char portStr[SWI_MANGOH_BRIDGE_SOCKETS_PORT_STRING_LEN] = {0};
        snprintf(portStr, SWI_MANGOH_BRIDGE_SOCKETS_PORT_STRING_LEN, "%u", port);
        res = getaddrinfo((const char*)req->serverIP, portStr, &hints, &servinfo);
        if (res)
        {
            if (res == EAI_AGAIN)
            {
                LE_WARN("WARNING getaddrinfo() name server temporary failure indication");
                res = LE_COMM_ERROR;
            }
            else
            {
                LE_ERROR("ERROR getaddrinfo() failed(%d)", res);
                res = LE_FAULT;
            }

            goto cleanup;
        }

        struct addrinfo* p = NULL;
        for (p = servinfo; p != NULL; p = p->ai_next)
        {
            char addrstr[SWI_MANGOH_BRIDGE_SOCKETS_SERVER_IP_LEN] = {0};
            void* ptr = NULL;

            switch (p->ai_family)
            {
            case AF_INET:
                ptr = &((struct sockaddr_in*)p->ai_addr)->sin_addr;
                break;

            case AF_INET6:
                ptr = &((struct sockaddr_in6*)p->ai_addr)->sin6_addr;
                break;
            }

            inet_ntop(p->ai_family, ptr, addrstr, sizeof(addrstr));
            LE_DEBUG("IPv%u address: '%s' ('%s')", p->ai_family == PF_INET6 ? 6 : 4, addrstr, p->ai_canonname);

            res = connect(sockets->clients.info[sockets->clients.nextId].sockFd, p->ai_addr, p->ai_addrlen);
            if ((res < 0) && (errno != EINPROGRESS))
            {
                LE_WARN("WARNING connect() socket(%d) failed(%d/%d)",
                        sockets->clients.info[sockets->clients.nextId].sockFd, res, errno);
                continue;
            }

            break;
        }

        if (p == NULL)
        {
            LE_ERROR("ERROR connect('%s:%u') socket(%d) failed",
                    req->serverIP, req->port, sockets->clients.info[sockets->clients.nextId].sockFd);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        LE_DEBUG("socket[%u](%d) connecting...", sockets->clients.nextId, sockets->clients.info[sockets->clients.nextId].sockFd);
        sockets->clients.info[sockets->clients.nextId].connecting = true;
        rsp->id = sockets->clients.nextId;
        sockets->clients.nextId = (sockets->clients.nextId + 1) % SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS;

        LE_DEBUG("result(%d)", rsp->id);
        res = swi_mangoh_bridge_sendResult(sockets->bridge, sizeof(swi_mangoh_bridge_sockets_connect_rsp_t));
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        LE_ERROR("socket(%u) not closed", sockets->clients.nextId);
        res = -EINVAL;
        goto cleanup;
    }

cleanup:
    if (res)
    {
        int32_t err = swi_mangoh_bridge_sendResult(sockets->bridge, 0);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", err);
        }

        err = swi_mangoh_bridge_sockets_closeClient(&sockets->clients.info[sockets->clients.nextId]);
        if (err != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", err);
        }
    }

    return res;
}

static int swi_mangoh_bridge_sockets_writeToAll(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(data);

    const swi_mangoh_bridge_sockets_write_all_req_t* const req = (swi_mangoh_bridge_sockets_write_all_req_t*)data;
    LE_DEBUG("---> WRITE TO ALL");

    uint8_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        if ((sockets->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) && sockets->clients.info[idx].connected)
        {
            if (sockets->clients.info[idx].txBuff.len + size < SWI_MANGOH_BRIDGE_SOCKETS_BUFF_LEN)
            {
                memcpy(&sockets->clients.info[idx].txBuff.data[sockets->clients.info[idx].txBuff.len], req->data, size);
                sockets->clients.info[idx].txBuff.len += size;
            }
            else
            {
                LE_ERROR("ERROR sockets[%u](%d) Tx buffer overflow", idx, sockets->clients.info[idx].sockFd);
                res = LE_OVERFLOW;
                goto cleanup;
            }
        }
    }

    res = swi_mangoh_bridge_sendResult(sockets->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_closeServer(swi_mangoh_bridge_sockets_server_t* server)
{
    int32_t res = LE_OK;

    LE_ASSERT(server);

    if (server->sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        LE_DEBUG("close server socket(%d)", server->sockFd);
        res = close(server->sockFd);
        if (res < 0)
        {
            LE_ERROR("ERROR close() socket(%d) failed(%d/%d)", server->sockFd, res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        server->sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_closeClient(swi_mangoh_bridge_sockets_client_info_t* clientInfo)
{
    int32_t res = LE_OK;

    LE_ASSERT(clientInfo);

    clientInfo->rxBuff.len = 0;
    clientInfo->txBuff.len = 0;
    clientInfo->connected = false;
    clientInfo->connecting = false;

    if (clientInfo->sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        LE_DEBUG("close socket(%d)", clientInfo->sockFd);
        res = close(clientInfo->sockFd);
        if (res < 0)
        {
            LE_ERROR("ERROR close() socket(%d) failed(%d/%d)", clientInfo->sockFd, res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }
    }

    clientInfo->sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_checkConnections(swi_mangoh_bridge_sockets_client_info_t* clientInfo)
{
    int32_t res = LE_OK;

    LE_ASSERT(clientInfo);

    int32_t val = 0;
    socklen_t len = sizeof(val);
    res = getsockopt(clientInfo->sockFd, SOL_SOCKET, SO_ERROR, &val, &len);
    if (res < 0)
    {
        LE_ERROR("ERROR getsockopt() socket(%d) failed(%d/%d)", clientInfo->sockFd, res, errno);

        res = swi_mangoh_bridge_sockets_closeClient(clientInfo);
        if (res)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", res);
            goto cleanup;
        }

        goto cleanup;
    }

    if (val)
    {
        res = swi_mangoh_bridge_sockets_closeClient(clientInfo);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", res);
            goto cleanup;
        }

        goto cleanup;
    }

    LE_DEBUG("socket(%d) connected", clientInfo->sockFd);
    clientInfo->connecting = false;
    clientInfo->connected = true;

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_checkClients(uint16_t idx, swi_mangoh_bridge_sockets_client_t* clients)
{
    int32_t res = LE_OK;

    LE_ASSERT(clients);

    if (FD_ISSET(clients->info[idx].sockFd, &clients->errfds))
    {
        res = swi_mangoh_bridge_sockets_closeClient(&clients->info[idx]);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", res);
            goto cleanup;
        }

        goto cleanup;
    }

    if (FD_ISSET(clients->info[idx].sockFd, &clients->readfds))
    {
        uint8_t data[SWI_MANGOH_BRIDGE_SOCKETS_BUFF_LEN] = {0};
        ssize_t bytesRx = recv(clients->info[idx].sockFd, data, SWI_MANGOH_BRIDGE_SOCKETS_BUFF_LEN, 0);
        LE_DEBUG("socket[%u](%d) recv(%d)", idx, clients->info[idx].sockFd, bytesRx);
        if (bytesRx < 0)
        {
            LE_ERROR("ERROR recv() socket(%d) failed(%d/%d)", clients->info[idx].sockFd, bytesRx, errno);

            res = swi_mangoh_bridge_sockets_closeClient(&clients->info[idx]);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", res);
                goto cleanup;
            }

            res = bytesRx;
            goto cleanup;
        }
        else if (bytesRx > 0)
        {
            if (clients->info[idx].rxBuff.len + bytesRx > SWI_MANGOH_BRIDGE_SOCKETS_BUFF_LEN)
            {
                LE_ERROR("ERROR socket[%u](%d) Rx buffer overflow", idx, clients->info[idx].sockFd);
                res = LE_OVERFLOW;
                goto cleanup;
            }

            memcpy(&clients->info[idx].rxBuff.data[clients->info[idx].rxBuff.len], data, bytesRx);
            clients->info[idx].rxBuff.len += bytesRx;
            LE_DEBUG("socket[%u](%d) Rx buffer length(%u)", idx, clients->info[idx].sockFd, clients->info[idx].rxBuff.len);
        }
    }

    if (FD_ISSET(clients->info[idx].sockFd, &clients->writefds))
    {
        LE_DEBUG("socket[%u](%d) Tx buffer length(%u)", idx, clients->info[idx].sockFd, clients->info[idx].txBuff.len);
        if (clients->info[idx].txBuff.len)
        {
            ssize_t bytesTx = send(clients->info[idx].sockFd, clients->info[idx].txBuff.data, clients->info[idx].txBuff.len, 0);
            LE_DEBUG("socket[%u](%d) send(%d)", idx, clients->info[idx].sockFd, bytesTx);
            if (bytesTx < 0)
            {
                LE_ERROR("ERROR socket[%u](%d) send() failed(%d/%d)", idx, clients->info[idx].sockFd, bytesTx, errno);

                res = swi_mangoh_bridge_sockets_closeClient(&clients->info[idx]);
                if (res != LE_OK)
                {
                    LE_ERROR("ERROR swi_mangoh_bridge_sockets_closeClient() failed(%d)", res);
                    goto cleanup;
                }

                res = LE_IO_ERROR;
                goto cleanup;
            }

            memmove(clients->info[idx].txBuff.data, &clients->info[idx].txBuff.data[bytesTx], clients->info[idx].txBuff.len - bytesTx);
            clients->info[idx].txBuff.len -= bytesTx;
            LE_DEBUG("socket[%u](%d) Tx buffer length(%u)", idx, clients->info[idx].sockFd, clients->info[idx].txBuff.len);
        }
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_runner(void* param)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);

    FD_ZERO(&sockets->clients.writefds);
    FD_ZERO(&sockets->clients.errfds);
    sockets->clients.maxSockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;

    uint8_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        if ((sockets->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) && sockets->clients.info[idx].connecting)
        {
            FD_SET(sockets->clients.info[idx].sockFd, &sockets->clients.writefds);
            FD_SET(sockets->clients.info[idx].sockFd, &sockets->clients.errfds);
            sockets->clients.maxSockFd = (sockets->clients.info[idx].sockFd > sockets->clients.maxSockFd) ?
                    sockets->clients.info[idx].sockFd:sockets->clients.maxSockFd;
        }
    }

    if (sockets->clients.maxSockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        struct timeval tv  = {0};
        res = select(sockets->clients.maxSockFd + 1, NULL, &sockets->clients.writefds, &sockets->clients.errfds, &tv);
        if (res < 0)
        {
            LE_ERROR("ERROR select() failed(%d/%d)", res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
        {
            if ((sockets->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) &&
                (FD_ISSET(sockets->clients.info[idx].sockFd, &sockets->clients.writefds)) &&
                sockets->clients.info[idx].connecting)
            {
                res = swi_mangoh_bridge_sockets_checkConnections(&sockets->clients.info[idx]);
                if (res != LE_OK)
                {
                    LE_ERROR("ERROR swi_mangoh_bridge_sockets_checkConnections() failed(%d)", res);
                    goto cleanup;
                }
            }
        }
    }

    FD_ZERO(&sockets->clients.readfds);
    FD_ZERO(&sockets->clients.writefds);
    FD_ZERO(&sockets->clients.errfds);
    sockets->clients.maxSockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;

    for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        if ((sockets->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) && sockets->clients.info[idx].connected)
        {
            FD_SET(sockets->clients.info[idx].sockFd, &sockets->clients.readfds);
            FD_SET(sockets->clients.info[idx].sockFd, &sockets->clients.writefds);
            FD_SET(sockets->clients.info[idx].sockFd, &sockets->clients.errfds);
            sockets->clients.maxSockFd = (sockets->clients.info[idx].sockFd > sockets->clients.maxSockFd) ?
                    sockets->clients.info[idx].sockFd:sockets->clients.maxSockFd;
        }
    }

    if (sockets->clients.maxSockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        struct timeval tv  = {0};
        res = select(sockets->clients.maxSockFd + 1, &sockets->clients.readfds, &sockets->clients.writefds, &sockets->clients.errfds, &tv);
        if (res < 0)
        {
            LE_ERROR("ERROR select() failed(%d/%d)", res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
        {
            if ((sockets->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID) && (sockets->clients.info[idx].connected))
            {
                res = swi_mangoh_bridge_sockets_checkClients(idx, &sockets->clients);
                if (res != LE_OK)
                {
                    LE_ERROR("ERROR swi_mangoh_bridge_sockets_checkClients() failed(%d)", res);
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_sockets_reset(void* param)
{
    swi_mangoh_bridge_sockets_t* sockets = (swi_mangoh_bridge_sockets_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(sockets);

    uint8_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        if (sockets->clients.info[idx].sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
        {
            LE_DEBUG("close socket[%u](%d)", idx, sockets->clients.info[idx].sockFd);
            res = close(sockets->clients.info[idx].sockFd);
            if (res < 0)
            {
                LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
                res = LE_IO_ERROR;
                goto cleanup;
            }

            sockets->clients.info[idx].sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;
            sockets->clients.info[idx].rxBuff.len = 0;
            sockets->clients.info[idx].txBuff.len = 0;
            sockets->clients.info[idx].connecting = false;
            sockets->clients.info[idx].connected = false;
        }
    }

    if (sockets->server.sockFd != SWI_MANGOH_BRIDGE_SOCKETS_INVALID)
    {
        LE_DEBUG("close server socket(%d)", sockets->server.sockFd);
        res = close(sockets->server.sockFd);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        sockets->server.sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;
    }

cleanup:
    return res;
}

int swi_mangoh_bridge_sockets_init(swi_mangoh_bridge_sockets_t* sockets, void* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(sockets);
    LE_ASSERT(bridge);

    LE_DEBUG("init");

    sockets->bridge = bridge;
    sockets->server.sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;

    uint32_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS; idx++)
    {
        sockets->clients.info[idx].sockFd = SWI_MANGOH_BRIDGE_SOCKETS_INVALID;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_LISTEN, sockets, swi_mangoh_bridge_sockets_listen);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_ACCEPT, sockets, swi_mangoh_bridge_sockets_accept);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_READ, sockets, swi_mangoh_bridge_sockets_read);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_WRITE, sockets, swi_mangoh_bridge_sockets_write);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_CONNECTED, sockets, swi_mangoh_bridge_sockets_connected);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_CLOSE, sockets, swi_mangoh_bridge_sockets_close);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_CONNECTING, sockets, swi_mangoh_bridge_sockets_connecting);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_CONNECT, sockets, swi_mangoh_bridge_sockets_connect);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(sockets->bridge, SWI_MANGOH_BRIDGE_SOCKETS_WRITE_TO_ALL, sockets, swi_mangoh_bridge_sockets_writeToAll);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerRunner(sockets->bridge, sockets, swi_mangoh_bridge_sockets_runner);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerRunner() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerReset(sockets->bridge, sockets, swi_mangoh_bridge_sockets_reset);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerReset() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    LE_DEBUG("init completed(%d)", res);
    return res;
}

int swi_mangoh_bridge_sockets_destroy(swi_mangoh_bridge_sockets_t* sockets)
{
    int32_t res = LE_OK;

    LE_ASSERT(sockets);

    res = swi_mangoh_bridge_sockets_reset(sockets);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sockets_reset() failed(%d)", res);
    }

    return LE_OK;
}
