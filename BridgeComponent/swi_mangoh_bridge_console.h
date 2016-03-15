/*
 * @file swi_mangoh_bridge_console.h
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
#include "swi_mangoh_bridge_packet.h"
#include "swi_mangoh_bridge_tcpClient.h"
#include "swi_mangoh_bridge_tcpServer.h"

#ifndef SWI_MANGOH_BRIDGE_CONSOLE_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_CONSOLE_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_CONSOLE_WRITE                           'P'
#define SWI_MANGOH_BRIDGE_CONSOLE_READ                            'p'
#define SWI_MANGOH_BRIDGE_CONSOLE_CONNECTED                       'a'

#define SWI_MANGOH_BRIDGE_CONSOLE_SERVER_IP_ADDR                  "127.0.0.1"
#define SWI_MANGOH_BRIDGE_CONSOLE_SERVER_PORT                     "6571"
#define SWI_MANGOH_BRIDGE_CONSOLE_SERVER_BACKLOG                  1
#define SWI_MANGOH_BRIDGE_CONSOLE_RX_BUFF_SIZE                    1024

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Console requests
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_console_write_req_t
{
    uint8_t                                    data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_write_read_req_t;

typedef struct _swi_mangoh_bridge_console_rea_req_t
{
    uint8_t                                    len;
} __attribute__((packed)) swi_mangoh_bridge_console_read_req_t;

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Console responses
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_console_read_rsp_t
{
    uint8_t                                    data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_console_read_rsp_t;

typedef struct _swi_mangoh_bridge_console_connected_rsp_t
{
    int8_t                                    result;
} __attribute__((packed)) swi_mangoh_bridge_console_connected_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Console module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_console_t
{
    swi_mangoh_bridge_tcp_server_t            server;                                              ///< Server
    swi_mangoh_bridge_tcp_client_t            clients;                                             ///< Clients
    int8_t                                    rxBuffer[SWI_MANGOH_BRIDGE_CONSOLE_RX_BUFF_SIZE];    ///< Receive data buffer
    void*                                     bridge;                                              ///< Bridge module
    uint32_t                                  rxBuffLen;                                           ///< Current bytes in Rx buffer
} swi_mangoh_bridge_console_t;

int swi_mangoh_bridge_console_run(swi_mangoh_bridge_console_t*);
int swi_mangoh_bridge_console_init(swi_mangoh_bridge_console_t*, void*);
int swi_mangoh_bridge_console_destroy(swi_mangoh_bridge_console_t*);

#endif
