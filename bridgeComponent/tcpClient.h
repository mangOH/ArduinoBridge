/*
 * @file tcpClient.h
 *
 * Arduino bridge TCP client sub-module.
 *
 * This module contains data structures and functions used to handling a list TCP connected clients.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#ifndef SWI_MANGOH_BRIDGE_TCP_CLIENT_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_TCP_CLIENT_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_TCP_CLIENT_SOCKET_INVALID                 -1
#define SWI_MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS                    10
#define SWI_MANGOH_BRIDGE_TCP_CLIENT_SEND_BUFFER_LEN                0x4000
#define SWI_MANGOH_BRIDGE_TCP_CLIENT_RECV_BUFFER_LEN                0x4000

//------------------------------------------------------------------------------------------------------------------
/**
 * TCP client info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_tcp_client_info_t
{
    int8_t*                                 sendBuffer;                                           ///< Send buffer
    int8_t*                                 rxBuffer;                                             ///< Receive buffer
    uint32_t                                sendBuffLen;                                          ///< Number of bytes in send buffer
    uint32_t                                recvBuffLen;                                          ///< Number of bytes in receive buffer
    int32_t                                 sockFd;                                               ///< Socket descriptor
} swi_mangoh_bridge_tcp_client_info_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * TCP client module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_tcp_client_t
{
    swi_mangoh_bridge_tcp_client_info_t     info[SWI_MANGOH_BRIDGE_TCP_CLIENT_MAX_CLIENTS];       ///< List of clients
    fd_set                                  readfds;                                              ///< Read fd set
    fd_set                                  writefds;                                             ///< Write fd set
    int32_t                                 maxSockFd;                                            ///< fd set maximum
    uint32_t                                nextId;                                               ///< Next client ID
    bool                                    broadcast;                                            ///< Broadcast to all clients flag
} swi_mangoh_bridge_tcp_client_t;

int swi_mangoh_bridge_tcp_client_write(swi_mangoh_bridge_tcp_client_t*, const uint8_t*, uint32_t);
void swi_mangoh_bridge_tcp_client_connected(const swi_mangoh_bridge_tcp_client_t*, int8_t*);
int swi_mangoh_bridge_tcp_client_getReceivedData(swi_mangoh_bridge_tcp_client_t*, int8_t*, uint32_t*, uint32_t);

void swi_mangoh_bridge_tcp_client_setNextId(swi_mangoh_bridge_tcp_client_t*);

int swi_mangoh_bridge_tcp_client_run(swi_mangoh_bridge_tcp_client_t*);
void swi_mangoh_bridge_tcp_client_init(swi_mangoh_bridge_tcp_client_t*, bool);
int swi_mangoh_bridge_tcp_client_destroy(swi_mangoh_bridge_tcp_client_t*);

#endif
