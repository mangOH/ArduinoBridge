/*
 * @file mangoh_bridge_mailbox.h
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
#include "tcpServer.h"
#include "tcpClient.h"
#include "json.h"

#ifndef MANGOH_BRIDGE_MAILBOX_INCLUDE_GUARD
#define MANGOH_BRIDGE_MAILBOX_INCLUDE_GUARD

#define MANGOH_BRIDGE_MAILBOX_SEND                            'M'
#define MANGOH_BRIDGE_MAILBOX_SEND_JSON                       'J'
#define MANGOH_BRIDGE_MAILBOX_RECV                            'm'
#define MANGOH_BRIDGE_MAILBOX_AVAILABLE                       'n'
#define MANGOH_BRIDGE_MAILBOX_DATASTORE_PUT                   'D'
#define MANGOH_BRIDGE_MAILBOX_DATASTORE_GET                   'd'

#define MANGOH_BRIDGE_MAILBOX_SERVER_IP_ADDR                  "127.0.0.1"
#define MANGOH_BRIDGE_MAILBOX_JSON_SERVER_PORT                "5700"
#define MANGOH_BRIDGE_MAILBOX_SERVER_BACKLOG                  5
#define MANGOH_BRIDGE_MAILBOX_RX_BUFF_SIZE                    0x4000

#define MANGOH_BRIDGE_MAILBOX_GET_WILDCARD                    "*"
#define MANGOH_BRIDGE_MAILBOX_RAW_COMMAND                     "raw"
#define MANGOH_BRIDGE_MAILBOX_GET_COMMAND                     "get"
#define MANGOH_BRIDGE_MAILBOX_PUT_COMMAND                     "put"
#define MANGOH_BRIDGE_MAILBOX_DELETE_COMMAND                  "delete"

#define MANGOH_BRIDGE_MAILBOX_SEPARATOR                        0xFE
#define MANGOH_BRIDGE_MAILBOX_DATA_STORE_SIZE                  31
#define MANGOH_BRIDGE_MAILBOX_DATASTORE_KEY_IDX                0
#define MANGOH_BRIDGE_MAILBOX_DATASTORE_VALUE_IDX              1
#define MANGOH_BRIDGE_MAILBOX_DATASTORE_PARAMS                 2

//--------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Mailbox requests
 */
//--------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_mailbox_send_req_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_mailbox_send_req_t;

typedef struct _mangoh_bridge_mailbox_json_send_req_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_mailbox_json_send_req_t;

typedef struct _mangoh_bridge_mailbox_datastore_put_req_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_mailbox_datastore_put_req_t;

typedef struct _mangoh_bridge_mailbox_datastore_get_req_t
{
    uint8_t key[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_mailbox_datastore_get_req_t;

//--------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Mailbox responses
 */
//--------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_mailbox_recv_rsp_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_mailbox_recv_rsp_t;

typedef struct _mangoh_bridge_mailbox_available_rsp_t
{
    uint16_t len;
} __attribute__((packed)) mangoh_bridge_mailbox_available_rsp_t;

typedef struct _mangoh_bridge_mailbox_datastore_put_rsp_t
{
    uint8_t result;
} __attribute__((packed)) mangoh_bridge_mailbox_datastore_put_rsp_t;

typedef struct _mangoh_bridge_mailbox_datastore_get_rsp_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_mailbox_datastore_get_rsp_t;

//--------------------------------------------------------------------------------------------------
/**
 * Mailbox module
 */
//--------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_mailbox_t
{
    int8_t                     rxBuffer[MANGOH_BRIDGE_MAILBOX_RX_BUFF_SIZE]; ///< Receive buffer
    mangoh_bridge_tcp_server_t server;                                       ///< Server module
    mangoh_bridge_tcp_client_t clients;                                      ///< Clients
    le_hashmap_Ref_t           database;                                     ///< Datastore data
    void*                      bridge;                                       ///< Bridge module
    uint8_t*                   jsonMsg;                                      ///< JSON message
    uint32_t                   rxBuffLen;                                    ///< Number of bytes in Rx buffer
    uint32_t                   jsonMsgLen;                                   ///< JSON message length
} mangoh_bridge_mailbox_t;

int mangoh_bridge_mailbox_init(mangoh_bridge_mailbox_t*, void*);
int mangoh_bridge_mailbox_destroy(mangoh_bridge_mailbox_t*);

#endif
