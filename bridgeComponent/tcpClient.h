/*
 * @file mangoh_bridge_tcpClient.h
 *
 * Arduino bridge TCP client sub-module.
 *
 * This module contains data structures and functions used to handling a list TCP connected clients.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#ifndef MANGOH_BRIDGE_TCP_CLIENT_INCLUDE_GUARD
#define MANGOH_BRIDGE_TCP_CLIENT_INCLUDE_GUARD

#define MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID                 -1
#define MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS                    10
#define MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN                0x4000
#define MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN                0x4000

//------------------------------------------------------------------------------------------------------------------
/**
 * TCP client info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_tcp_client_info_t
{
    int8_t*  sendBuffer;  ///< Send buffer
    int8_t*  rxBuffer;    ///< Receive buffer
    uint32_t sendBuffLen; ///< Number of bytes in send buffer
    uint32_t recvBuffLen; ///< Number of bytes in receive buffer
    int32_t  sockFd;      ///< Socket descriptor
} mangoh_bridge_tcp_client_info_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * TCP client module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_tcp_client_t
{
    mangoh_bridge_tcp_client_info_t info[MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS]; ///< List of clients
    fd_set                          readfds;                                    ///< Read fd set
    fd_set                          writefds;                                   ///< Write fd set
    int32_t                         maxSockFd;                                  ///< fd set maximum
    uint32_t                        nextId;                                     ///< Next client ID
    bool                            broadcast;                                  ///< Broadcast to all clients flag
} mangoh_bridge_tcp_client_t;

int mangoh_bridge_tcp_client_write(mangoh_bridge_tcp_client_t*, const uint8_t*, uint32_t);
void mangoh_bridge_tcp_client_connected(const mangoh_bridge_tcp_client_t*, int8_t*);
int mangoh_bridge_tcp_client_getReceivedData(mangoh_bridge_tcp_client_t*, int8_t*, uint32_t*, uint32_t);

void mangoh_bridge_tcp_client_setNextId(mangoh_bridge_tcp_client_t*);

int mangoh_bridge_tcp_client_run(mangoh_bridge_tcp_client_t*);
void mangoh_bridge_tcp_client_init(mangoh_bridge_tcp_client_t*, bool);
int mangoh_bridge_tcp_client_destroy(mangoh_bridge_tcp_client_t*);

#endif
