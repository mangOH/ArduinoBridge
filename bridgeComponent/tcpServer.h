/*
 * @file mangoh_bridge_tcpServer.h
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

#ifndef MANGOH_BRIDGE_TCP_SERVER_INCLUDE_GUARD
#define MANGOH_BRIDGE_TCP_SERVER_INCLUDE_GUARD

#define MANGOH_BRIDGE_TCP_SERVER_SOCKET_INVALID                   -1
#define MANGOH_BRIDGE_TCP_SERVER_RETRY_BIND_DELAY_SECS            5

//------------------------------------------------------------------------------------------------------------------
/**
 * TCP server module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_tcp_server_t
{
    fd_set  readfds; ///< Read descriptor set
    int32_t sockFd;  ///< Server socket descriptor
} mangoh_bridge_tcp_server_t;

int mangoh_bridge_tcp_server_acceptNewConnections(mangoh_bridge_tcp_server_t*, mangoh_bridge_tcp_client_t*);

int mangoh_bridge_tcp_server_start(mangoh_bridge_tcp_server_t*, const char*, const char*, uint32_t);
int mangoh_bridge_tcp_server_stop(mangoh_bridge_tcp_server_t*);

#endif
