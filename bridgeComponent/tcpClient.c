/**
 * @file
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "packet.h"
#include "tcpClient.h"

static int mangoh_bridge_tcp_client_close(mangoh_bridge_tcp_client_info_t*);
static int mangoh_bridge_tcp_client_broadcast(mangoh_bridge_tcp_client_t*, uint32_t);
static int mangoh_bridge_tcp_client_readFromSockets(mangoh_bridge_tcp_client_t*);
static int mangoh_bridge_tcp_client_writeToSockets(mangoh_bridge_tcp_client_t*);
static int mangoh_bridge_tcp_client_dropStarvingClients(mangoh_bridge_tcp_client_t*);

static int mangoh_bridge_tcp_client_close(mangoh_bridge_tcp_client_info_t* tcpClientInfo)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClientInfo);

    res = close(tcpClientInfo->sockFd);
    if (res < 0)
    {
        LE_ERROR("ERROR socket(%d) close() failed(%d/%d)", tcpClientInfo->sockFd, res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    if (tcpClientInfo->sendBuffer)
    {
        free(tcpClientInfo->sendBuffer);
        tcpClientInfo->sendBuffer = NULL;
    }

    if (tcpClientInfo->rxBuffer)
    {
        free(tcpClientInfo->rxBuffer);
        tcpClientInfo->rxBuffer = NULL;
    }

    tcpClientInfo->sendBuffLen = 0;
    tcpClientInfo->sockFd = MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID;
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_tcp_client_broadcast(mangoh_bridge_tcp_client_t* tcpClients, uint32_t from)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClients);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if ((idx != from) && (tcpClients->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID))
        {
            LE_DEBUG("socket[%u](%d)", idx, tcpClients->info[idx].sockFd);
            LE_ASSERT(tcpClients->info[idx].sendBuffer != NULL);

            if (tcpClients->info[idx].sendBuffLen + tcpClients->info[from].sendBuffLen > MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN)
            {
                LE_ERROR("ERROR client(%u) send buffer overflow", idx);
                res = LE_OVERFLOW;
                goto cleanup;
            }

            memcpy(tcpClients->info[idx].sendBuffer + tcpClients->info[idx].sendBuffLen, tcpClients->info[from].rxBuffer, tcpClients->info[from].recvBuffLen);
            tcpClients->info[idx].sendBuffLen += tcpClients->info[from].recvBuffLen;
            LE_DEBUG("socket[%u](%d) send(%u)", idx, tcpClients->info[idx].sockFd, tcpClients->info[idx].sendBuffLen);
        }
    }

cleanup:
    return res;
}

static int mangoh_bridge_tcp_client_readFromSockets(mangoh_bridge_tcp_client_t* tcpClients)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClients);

    tcpClients->maxSockFd = MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID;
    FD_ZERO(&tcpClients->readfds);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if (tcpClients->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
        {
            FD_SET(tcpClients->info[idx].sockFd, &tcpClients->readfds);
            tcpClients->maxSockFd = (tcpClients->info[idx].sockFd > tcpClients->maxSockFd) ? tcpClients->info[idx].sockFd:tcpClients->maxSockFd;
        }
    }

    if (tcpClients->maxSockFd == MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
    {
        goto cleanup;
    }

    struct timeval tv  = {0};
    res = select(tcpClients->maxSockFd + 1, &tcpClients->readfds, NULL, NULL, &tv);
    if (res < 0)
    {
        LE_ERROR("ERROR select() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    if (res > 0)
    {
        for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
        {
            if ((tcpClients->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID) &&
                (FD_ISSET(tcpClients->info[idx].sockFd, &tcpClients->readfds)))
            {
                LE_ASSERT(tcpClients->info[idx].rxBuffer != NULL);

                int32_t bytesRead = recv(tcpClients->info[idx].sockFd, tcpClients->info[idx].rxBuffer + tcpClients->info[idx].recvBuffLen,
                                         MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN - tcpClients->info[idx].recvBuffLen, 0);
                if (bytesRead < 0)
                {
                    LE_ERROR("ERROR socket[%u](%d) recv() failed(%d/%d)", idx, tcpClients->info[idx].sockFd, bytesRead, errno);

                    res = mangoh_bridge_tcp_client_close(&tcpClients->info[idx]);
                    if (res != LE_OK)
                    {
                        LE_ERROR("ERROR mangoh_bridge_tcp_client_close() failed(%d)", res);
                        goto cleanup;
                    }

                    res = LE_IO_ERROR;
                    goto cleanup;
                }
                else if ((bytesRead == 0) && (MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN - tcpClients->info[idx].recvBuffLen > 0))
                {
                    LE_INFO("socket[%u](%d) closed", idx, tcpClients->info[idx].sockFd);

                    res = mangoh_bridge_tcp_client_close(&tcpClients->info[idx]);
                    if (res != LE_OK)
                    {
                            LE_ERROR("ERROR mangoh_bridge_tcp_client_close() failed(%d)", res);
                            goto cleanup;
                    }
                }
                else
                {
                    LE_DEBUG("socket[%u](%d) read(%u)", idx, tcpClients->info[idx].sockFd, bytesRead);
                    tcpClients->info[idx].recvBuffLen += bytesRead;
                    LE_DEBUG("Rx buffer(%u)", tcpClients->info[idx].recvBuffLen);

                    if (tcpClients->broadcast)
                    {
                        res = mangoh_bridge_tcp_client_broadcast(tcpClients, idx);
                        if (res)
                        {
                            LE_ERROR("ERROR mangoh_bridge_tcp_client_broadcast() failed(%d)", res);
                            goto cleanup;
                        }
                    }
                }

                FD_CLR(tcpClients->info[idx].sockFd, &tcpClients->readfds);
            }
        }
    }

    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_tcp_client_writeToSockets(mangoh_bridge_tcp_client_t* tcpClient)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClient);

    FD_ZERO(&tcpClient->writefds);
    tcpClient->maxSockFd = MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID;

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if (tcpClient->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
        {
            FD_SET(tcpClient->info[idx].sockFd, &tcpClient->writefds);
            tcpClient->maxSockFd = (tcpClient->info[idx].sockFd > tcpClient->maxSockFd) ? tcpClient->info[idx].sockFd:tcpClient->maxSockFd;
        }
    }

    if (tcpClient->maxSockFd == MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
    {
        goto cleanup;
    }

    struct timeval tv  = {0};
    res = select(tcpClient->maxSockFd + 1, NULL, &tcpClient->writefds, NULL, &tv);
    if (res < 0)
    {
        LE_ERROR("ERROR select() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    if (res > 0)
    {
        for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
        {
            if ((tcpClient->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID) &&
                (FD_ISSET(tcpClient->info[idx].sockFd, &tcpClient->writefds)))
            {
                LE_ASSERT(tcpClient->info[idx].sendBuffer != NULL);

                if (tcpClient->info[idx].sendBuffLen > 0)
                {
                    LE_DEBUG("socket[%u](%d) send(%u)", idx, tcpClient->info[idx].sockFd, tcpClient->info[idx].sendBuffLen);
                    int32_t bytesSent = send(tcpClient->info[idx].sockFd, tcpClient->info[idx].sendBuffer, tcpClient->info[idx].sendBuffLen, 0);
                    if (bytesSent < 0)
                    {
                        LE_WARN("WARNING socket[%u](%d) send() failed(%d/%d)", idx, tcpClient->info[idx].sockFd, bytesSent, errno);

                        res = mangoh_bridge_tcp_client_close(&tcpClient->info[idx]);
                        if (res != LE_OK)
                        {
                            LE_ERROR("ERROR mangoh_bridge_tcp_client_close() failed(%d)", res);
                            goto cleanup;
                        }
                    }
                    else
                    {
                        LE_DEBUG("socket[%u](%d) sent(%u)", idx, tcpClient->info[idx].sockFd, bytesSent);
                        memmove(tcpClient->info[idx].sendBuffer, tcpClient->info[idx].sendBuffer + bytesSent, tcpClient->info[idx].sendBuffLen - bytesSent);
                        memset(tcpClient->info[idx].sendBuffer + tcpClient->info[idx].sendBuffLen - bytesSent, 0, bytesSent);
                        tcpClient->info[idx].sendBuffLen -= bytesSent;
                    }
                }

                FD_CLR(tcpClient->info[idx].sockFd, &tcpClient->writefds);
            }
        }
    }

    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_tcp_client_dropStarvingClients(mangoh_bridge_tcp_client_t* tcpClient)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClient);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if ((tcpClient->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID) &&
            (tcpClient->info[idx].sendBuffLen == MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN))
        {
            LE_DEBUG("close socket[%u](%d)", idx, tcpClient->info[idx].sockFd);
            res = mangoh_bridge_tcp_client_close(&tcpClient->info[idx]);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_tcp_client_close() failed(%d)", res);
                goto cleanup;
            }
        }
    }

cleanup:
    return res;
}

int mangoh_bridge_tcp_client_getReceivedData(mangoh_bridge_tcp_client_t* tcpClient, int8_t* buff, uint32_t* len, uint32_t maxLen)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClient);
    LE_ASSERT(buff);
    LE_ASSERT(len);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if (tcpClient->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
        {
            if (*len + tcpClient->info[idx].recvBuffLen > maxLen)
            {
                LE_WARN("buffer overflow");
                break;
            }

            if (tcpClient->info[idx].recvBuffLen > 0)
            {
                memcpy(&buff[*len], tcpClient->info[idx].rxBuffer, tcpClient->info[idx].recvBuffLen);
                *len += tcpClient->info[idx].recvBuffLen;
                tcpClient->info[idx].recvBuffLen = 0;
            }
        }
    }

    return res;
}

int mangoh_bridge_tcp_client_write(mangoh_bridge_tcp_client_t* tcpClient, const uint8_t* buff, uint32_t len)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClient);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if (tcpClient->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
        {
            LE_ASSERT(tcpClient->info[idx].sendBuffer != NULL);

            if (len + tcpClient->info[idx].sendBuffLen > MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN)
            {
                LE_ERROR("ERROR socket[%u](%d) send buffer overflow", idx, tcpClient->info[idx].sockFd);
                res = LE_OVERFLOW;
                goto cleanup;
            }

            memcpy(tcpClient->info[idx].sendBuffer + tcpClient->info[idx].sendBuffLen, buff, len);
            tcpClient->info[idx].sendBuffLen += len;
            LE_DEBUG("socket[%u](%d) send buffer length(%u)", idx, tcpClient->info[idx].sockFd, tcpClient->info[idx].sendBuffLen);
        }
    }

cleanup:
    return res;
}

void mangoh_bridge_tcp_client_connected(const mangoh_bridge_tcp_client_t* tcpClient, int8_t* result)
{
    LE_ASSERT(tcpClient);
    LE_ASSERT(result);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        if (tcpClient->info[idx].sockFd != MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID)
        {
            LE_ASSERT(tcpClient->info[idx].sendBuffer);
            LE_ASSERT(tcpClient->info[idx].rxBuffer);

            LE_DEBUG("found connected client(%u)", idx);
            *result = true;
            break;
        }
    }

    if (idx == MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS)
    {
        *result = false;
    }
}

int mangoh_bridge_tcp_client_run(mangoh_bridge_tcp_client_t* tcpClient)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClient);

    res = mangoh_bridge_tcp_client_readFromSockets(tcpClient);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_client_readFromSockets() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_tcp_client_writeToSockets(tcpClient);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_client_writeToSockets() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_tcp_client_dropStarvingClients(tcpClient);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_tcp_client_dropStarvingClients() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

void mangoh_bridge_tcp_client_setNextId(mangoh_bridge_tcp_client_t* tcpClients)
{
    LE_ASSERT(tcpClients);
    tcpClients->nextId = (tcpClients->nextId + 1) % MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS;
};

void mangoh_bridge_tcp_client_init(mangoh_bridge_tcp_client_t* tcpClient, bool broadcast)
{
    LE_ASSERT(tcpClient);

    tcpClient->broadcast = broadcast;

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        tcpClient->info[idx].sockFd = MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID;
    }
}

int mangoh_bridge_tcp_client_destroy(mangoh_bridge_tcp_client_t* tcpClient)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpClient);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS; idx++)
    {
        res = mangoh_bridge_tcp_client_close(&tcpClient->info[idx]);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_tcp_client_close() failed(%d)", res);
            goto cleanup;
        }
    }

cleanup:
    return res;
}

