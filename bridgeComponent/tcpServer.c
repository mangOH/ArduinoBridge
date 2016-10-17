/**
 * @file
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "legato.h"
#include "tcpServer.h"

int mangoh_bridge_tcp_server_acceptNewConnections(mangoh_bridge_tcp_server_t* tcpServer, mangoh_bridge_tcp_client_t* tcpClients)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpServer);
    LE_ASSERT(tcpClients);

    FD_ZERO(&tcpServer->readfds);
    FD_SET(tcpServer->sockFd, &tcpServer->readfds);

    struct timeval tv  = {0};
    res = select(tcpServer->sockFd + 1, &tcpServer->readfds, NULL, NULL, &tv);
    if (res < 0)
    {
        LE_ERROR("ERROR select() failed(%d/%d)", res, errno);
        res = LE_COMM_ERROR;
        goto cleanup;
    }

    if ((res > 0) && FD_ISSET(tcpServer->sockFd, &tcpServer->readfds))
    {
        struct sockaddr_in clientAddr = {0};
        uint32_t clientAddrSize = 0;
        int32_t newFd = accept(tcpServer->sockFd, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (newFd < 0)
        {
            LE_ERROR("ERROR accept() failed(%d/%d)", newFd, errno);
            res = LE_COMM_ERROR;
            goto cleanup;
        }

        char clientIPStr[INET_ADDRSTRLEN] = {0};
        LE_INFO("connection -> '%s'", inet_ntop(AF_INET, &clientAddr.sin_addr, clientIPStr, INET_ADDRSTRLEN));

        res = fcntl(newFd, F_SETFL, O_NONBLOCK);
        if (res < 0)
        {
            LE_ERROR("ERROR fcntl() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        if (tcpClients->info[tcpClients->nextId].sockFd == MANGOH_BRIDGE_TCP_SERVER_SOCKET_INVALID)
        {
            LE_ASSERT(!tcpClients->info[tcpClients->nextId].sendBuffer && !tcpClients->info[tcpClients->nextId].rxBuffer);

            LE_DEBUG("client -> socket[%u](%d)", tcpClients->nextId, newFd);
            tcpClients->info[tcpClients->nextId].sockFd = newFd;

            tcpClients->info[tcpClients->nextId].sendBuffLen = 0;
            tcpClients->info[tcpClients->nextId].sendBuffer = calloc(1, MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN);
            if (!tcpClients->info[tcpClients->nextId].sendBuffer)
            {
                LE_ERROR("ERROR calloc() failed");
                res = LE_NO_MEMORY;
                goto cleanup;
            }

            tcpClients->info[tcpClients->nextId].recvBuffLen = 0;
            tcpClients->info[tcpClients->nextId].rxBuffer = calloc(1, MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN);
            if (!tcpClients->info[tcpClients->nextId].rxBuffer)
            {
                LE_ERROR("ERROR calloc() failed");
                res = LE_NO_MEMORY;
                goto cleanup;
            }

            mangoh_bridge_tcp_client_setNextId(tcpClients);
        }
        else
        {
            LE_ERROR("ERROR socket(%u) not closed", tcpClients->info[tcpClients->nextId].sockFd);

            res = close(newFd);
            if (res < 0)
            {
                LE_ERROR("ERROR socket(%d) close() failed(%d/%d)", newFd, res, errno);
                res = LE_COMM_ERROR;
                goto cleanup;
            }

            res = LE_BAD_PARAMETER;
            goto cleanup;
        }
    }

    res = LE_OK;

cleanup:
    return res;
}

int mangoh_bridge_tcp_server_start(mangoh_bridge_tcp_server_t* tcpServer, const char* serverIp, const char* service, uint32_t backlog)
{
    int32_t res = LE_OK;

    LE_ASSERT(tcpServer);

    while (1)
    {
        struct addrinfo hints = {0};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        struct addrinfo* servinfo = NULL;
        res = getaddrinfo(serverIp, service, &hints, &servinfo);
        if (res < 0)
        {
            LE_ERROR("ERROR getaddrinfo() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        // loop through all the results and bind to the first we can
        struct addrinfo* p = NULL;
        for (p = servinfo; p != NULL; p = p->ai_next)
        {
            tcpServer->sockFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (tcpServer->sockFd < 0)
            {
                LE_ERROR("ERROR socket() failed(%d/%d)", tcpServer->sockFd, errno);
                continue;
            }

            uint32_t reuseAddr = 1;
            res = setsockopt(tcpServer->sockFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));
            if (res < 0)
            {
                LE_ERROR("ERROR setsockopt() failed(%d/%d)", res, errno);
                res = LE_FAULT;
                goto cleanup;
            }

            res = bind(tcpServer->sockFd, p->ai_addr, p->ai_addrlen);
            if (res < 0)
            {
                LE_ERROR("ERROR bind() failed(%d/%d)", res, errno);

                res = close(tcpServer->sockFd);
                if (res < 0)
                {
                    LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
                    res = LE_COMM_ERROR;
                    goto cleanup;
                }

                tcpServer->sockFd = MANGOH_BRIDGE_TCP_SERVER_SOCKET_INVALID;
                continue;
            }

            break;
        }

        if (tcpServer->sockFd == MANGOH_BRIDGE_TCP_SERVER_SOCKET_INVALID)
        {
            LE_ERROR("ERROR bind '%s:%s' failed", serverIp, service);
            sleep(MANGOH_BRIDGE_TCP_SERVER_RETRY_BIND_DELAY_SECS);
            continue;
        }

        res = fcntl(tcpServer->sockFd, F_SETFL, O_NONBLOCK);
        if (res < 0)
        {
            LE_ERROR("ERROR fcntl() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        res = listen(tcpServer->sockFd, backlog);
        if (res < 0)
        {
            LE_ERROR("ERROR listen() failed(%d/%d)", res, errno);
            res = LE_COMM_ERROR;
            goto cleanup;
        }

        LE_DEBUG("server started('%s:%s')", serverIp, service);
        break;
    }

cleanup:
    return res;
}

int mangoh_bridge_tcp_server_stop(mangoh_bridge_tcp_server_t* tcpServer)
{
    int32_t res = LE_OK;

    if (tcpServer->sockFd > 0)
    {
        res = close(tcpServer->sockFd);
        if (res < 0)
        {
            LE_ERROR("ERROR close() socket(%d) failed(%d/%d)", tcpServer->sockFd, res, errno);
            res = LE_COMM_ERROR;
            goto cleanup;
        }

        tcpServer->sockFd = MANGOH_BRIDGE_TCP_SERVER_SOCKET_INVALID;
    }

    res = LE_OK;

cleanup:
    return res;
}
