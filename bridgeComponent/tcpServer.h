/*
 * @file tcpServer.h
 *
 * Arduino bridge TCP server module.
 *
 * This module contains data structures and methods used to implement a TCP server.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "tcpClient.h"

#ifndef SWI_MANGOH_BRIDGE_TCP_SERVER_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_TCP_SERVER_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_TCP_SERVER_SOCKET_INVALID                   -1
#define SWI_MANGOH_BRIDGE_TCP_SERVER_RETRY_BIND_DELAY_SECS            5

//------------------------------------------------------------------------------------------------------------------
/**
 * TCP server module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_tcp_server_t
{
    fd_set                                     readfds;                ///< Read descriptor set
    int32_t                                    sockFd;                 ///< Server socket descriptor
} swi_mangoh_bridge_tcp_server_t;

int swi_mangoh_bridge_tcp_server_acceptNewConnections(swi_mangoh_bridge_tcp_server_t*, swi_mangoh_bridge_tcp_client_t*);

int swi_mangoh_bridge_tcp_server_start(swi_mangoh_bridge_tcp_server_t*, const char*, const char*, uint32_t);
int swi_mangoh_bridge_tcp_server_stop(swi_mangoh_bridge_tcp_server_t*);

#endif
