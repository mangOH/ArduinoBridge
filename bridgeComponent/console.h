/*
 * @file mangoh_bridge_console.h
 *
 * Arduino bridge console sub-module.
 *
 * This module contains functions and data structures for executing the console portion of the Arduino
 * bridge protocol.  Callback functions are provided to support optional functionality with
 * Air Vantage.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "legato.h"
#include "packet.h"
#include "tcpClient.h"
#include "tcpServer.h"

#ifndef MANGOH_BRIDGE_CONSOLE_INCLUDE_GUARD
#define MANGOH_BRIDGE_CONSOLE_INCLUDE_GUARD

#define MANGOH_BRIDGE_CONSOLE_WRITE                           'P'
#define MANGOH_BRIDGE_CONSOLE_READ                            'p'
#define MANGOH_BRIDGE_CONSOLE_CONNECTED                       'a'

#define MANGOH_BRIDGE_CONSOLE_SERVER_IP_ADDR                  "127.0.0.1"
#define MANGOH_BRIDGE_CONSOLE_SERVER_PORT                     "6571"
#define MANGOH_BRIDGE_CONSOLE_SERVER_BACKLOG                  1
#define MANGOH_BRIDGE_CONSOLE_RX_BUFF_SIZE                    1024

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Console requests
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_console_write_req_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_write_read_req_t;

typedef struct _mangoh_bridge_console_rea_req_t
{
    uint8_t len;
} __attribute__((packed)) mangoh_bridge_console_read_req_t;

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Console responses
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_console_read_rsp_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_console_read_rsp_t;

typedef struct _mangoh_bridge_console_connected_rsp_t
{
    int8_t result;
} __attribute__((packed)) mangoh_bridge_console_connected_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Console module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_console_t
{
    mangoh_bridge_tcp_server_t server;                                       ///< Server
    mangoh_bridge_tcp_client_t clients;                                      ///< Clients
    int8_t                     rxBuffer[MANGOH_BRIDGE_CONSOLE_RX_BUFF_SIZE]; ///< Receive data buffer
    void*                      bridge;                                       ///< Bridge module
    uint32_t                   rxBuffLen;                                    ///< Current bytes in Rx buffer
} mangoh_bridge_console_t;

int mangoh_bridge_console_run(mangoh_bridge_console_t*);
int mangoh_bridge_console_init(mangoh_bridge_console_t*, void*);
int mangoh_bridge_console_destroy(mangoh_bridge_console_t*);

#endif
