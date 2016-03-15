/*
 * @file swi_mangoh_bridge_mailbox.h
 *
 * Arduino bridge mailbox sub-module.
 *
 * Contains functions and data structures for executing the mailbox portion of the Arduino
 * bridge protocol.  Callback functions are provided to support optional functionality with
 * Air Vantage.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "legato.h"
#include "swi_mangoh_bridge_tcpServer.h"
#include "swi_mangoh_bridge_tcpClient.h"
#include "swi_mangoh_bridge_json.h"

#ifndef SWI_MANGOH_BRIDGE_MAILBOX_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_MAILBOX_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_MAILBOX_SEND                            'M'
#define SWI_MANGOH_BRIDGE_MAILBOX_SEND_JSON                       'J'
#define SWI_MANGOH_BRIDGE_MAILBOX_RECV                            'm'
#define SWI_MANGOH_BRIDGE_MAILBOX_AVAILABLE                       'n'
#define SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_PUT                   'D'
#define SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_GET                   'd'

#define SWI_MANGOH_BRIDGE_MAILBOX_SERVER_IP_ADDR                  "127.0.0.1"
#define SWI_MANGOH_BRIDGE_MAILBOX_JSON_SERVER_PORT                "5700"
#define SWI_MANGOH_BRIDGE_MAILBOX_SERVER_BACKLOG                  5
#define SWI_MANGOH_BRIDGE_MAILBOX_RX_BUFF_SIZE                    0x4000

#define SWI_MANGOH_BRIDGE_MAILBOX_GET_WILDCARD                    "*"
#define SWI_MANGOH_BRIDGE_MAILBOX_RAW_COMMAND                     "raw"
#define SWI_MANGOH_BRIDGE_MAILBOX_GET_COMMAND                     "get"
#define SWI_MANGOH_BRIDGE_MAILBOX_PUT_COMMAND                     "put"
#define SWI_MANGOH_BRIDGE_MAILBOX_DELETE_COMMAND                  "delete"

#define SWI_MANGOH_BRIDGE_MAILBOX_SEPARATOR                        0xFE
#define SWI_MANGOH_BRIDGE_MAILBOX_DATA_STORE_SIZE                  31
#define SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_KEY_IDX                0
#define SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_VALUE_IDX              1
#define SWI_MANGOH_BRIDGE_MAILBOX_DATASTORE_PARAMS                 2

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Mailbox requests
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_mailbox_send_req_t
{
    uint8_t                                                data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_mailbox_send_req_t;

typedef struct _swi_mangoh_bridge_mailbox_json_send_req_t
{
    uint8_t                                                data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_mailbox_json_send_req_t;

typedef struct _swi_mangoh_bridge_mailbox_datastore_put_req_t
{
    uint8_t                                                data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_mailbox_datastore_put_req_t;

typedef struct _swi_mangoh_bridge_mailbox_datastore_get_req_t
{
    uint8_t                                                key[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_mailbox_datastore_get_req_t;

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Mailbox responses
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_mailbox_recv_rsp_t
{
    uint8_t                                                data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_mailbox_recv_rsp_t;

typedef struct _swi_mangoh_bridge_mailbox_available_rsp_t
{
    uint16_t                                               len;
} __attribute__((packed)) swi_mangoh_bridge_mailbox_available_rsp_t;

typedef struct _swi_mangoh_bridge_mailbox_datastore_put_rsp_t
{
    uint8_t                                                result;
} __attribute__((packed)) swi_mangoh_bridge_mailbox_datastore_put_rsp_t;

typedef struct _swi_mangoh_bridge_mailbox_datastore_get_rsp_t
{
    uint8_t                                                data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_mailbox_datastore_get_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Mailbox module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_mailbox_t
{
    int8_t                                               rxBuffer[SWI_MANGOH_BRIDGE_MAILBOX_RX_BUFF_SIZE];      ///< Receive buffer
    swi_mangoh_bridge_tcp_server_t                       server;                                                ///< Server module
    swi_mangoh_bridge_tcp_client_t                       clients;                                               ///< Clients
    le_hashmap_Ref_t                                     database;                                              ///< Datastore data
    void*                                                bridge;                                                ///< Bridge module
    uint8_t*                                             jsonMsg;                                               ///< JSON message
    uint32_t                                             rxBuffLen;                                             ///< Number of bytes in Rx buffer
    uint32_t                                             jsonMsgLen;                                            ///< JSON message length
} swi_mangoh_bridge_mailbox_t;

int swi_mangoh_bridge_mailbox_init(swi_mangoh_bridge_mailbox_t*, void*);
int swi_mangoh_bridge_mailbox_destroy(swi_mangoh_bridge_mailbox_t*);

#endif
