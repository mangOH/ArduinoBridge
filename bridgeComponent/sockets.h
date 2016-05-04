/*
 * @file mangoh_bridge_sockets.h
 *
 * Arduino bridge sockets sub-module.
 *
 * Contains functions and data structures for executing the console portion of the Arduino
 * bridge protocol.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#ifndef MANGOH_BRIDGE_SOCKETS_INCLUDE_GUARD
#define MANGOH_BRIDGE_SOCKETS_INCLUDE_GUARD

#define MANGOH_BRIDGE_SOCKETS_LISTEN                     'N'
#define MANGOH_BRIDGE_SOCKETS_ACCEPT                     'k'
#define MANGOH_BRIDGE_SOCKETS_READ                       'K'
#define MANGOH_BRIDGE_SOCKETS_WRITE                      'l'
#define MANGOH_BRIDGE_SOCKETS_CONNECTED                  'L'
#define MANGOH_BRIDGE_SOCKETS_CLOSE                      'j'
#define MANGOH_BRIDGE_SOCKETS_CONNECTING                 'c'
#define MANGOH_BRIDGE_SOCKETS_CONNECT                    'C'
#define MANGOH_BRIDGE_SOCKETS_WRITE_TO_ALL               'b'

#define MANGOH_BRIDGE_SOCKETS_INVALID                    -1
#define MANGOH_BRIDGE_SOCKETS_SERVER_BACKLOG             5
#define MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS                10
#define MANGOH_BRIDGE_SOCKETS_PORT_STRING_LEN            32
#define MANGOH_BRIDGE_SOCKETS_SERVER_IP_LEN              (MANGOH_BRIDGE_PACKET_DATA_SIZE - sizeof(uint16_t))
#define MANGOH_BRIDGE_SOCKETS_DATA_LEN                   (MANGOH_BRIDGE_PACKET_DATA_SIZE - sizeof(uint8_t))
#define MANGOH_BRIDGE_SOCKETS_BUFF_LEN                   8192

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Sockets requests
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_sockets_listen_req_t
{
    uint16_t port;
    uint8_t  serverIP[MANGOH_BRIDGE_SOCKETS_SERVER_IP_LEN];
} __attribute__((packed)) mangoh_bridge_sockets_listen_req_t;

typedef struct _mangoh_bridge_sockets_connect_req_t
{
    uint16_t port;
    uint8_t  serverIP[MANGOH_BRIDGE_SOCKETS_SERVER_IP_LEN];
} __attribute__((packed)) mangoh_bridge_sockets_connect_req_t;

typedef struct _mangoh_bridge_sockets_read_req_t
{
    uint8_t id;
    uint8_t len;
} __attribute__((packed)) mangoh_bridge_sockets_read_req_t;

typedef struct _mangoh_bridge_sockets_write_req_t
{
    uint8_t id;
    uint8_t data[MANGOH_BRIDGE_SOCKETS_DATA_LEN];
} __attribute__((packed)) mangoh_bridge_sockets_write_req_t;

typedef struct _mangoh_bridge_sockets_write_all_req_t
{
    uint8_t data[MANGOH_BRIDGE_SOCKETS_DATA_LEN];
} __attribute__((packed)) mangoh_bridge_sockets_write_all_req_t;

typedef struct _mangoh_bridge_sockets_connected_req_t
{
    uint8_t id;
} __attribute__((packed)) mangoh_bridge_sockets_connected_req_t;

typedef struct _mangoh_bridge_sockets_connecting_req_t
{
    uint8_t id;
} __attribute__((packed)) mangoh_bridge_sockets_connecting_req_t;

typedef struct _mangoh_bridge_sockets_close_req_t
{
    uint8_t id;
} __attribute__((packed)) mangoh_bridge_sockets_close_req_t;

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Sockets responses
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_sockets_listen_rsp_t
{
    int8_t result;
} __attribute__((packed)) mangoh_bridge_sockets_listen_rsp_t;

typedef struct _mangoh_bridge_sockets_accept_rsp_t
{
    int8_t result;
} __attribute__((packed)) mangoh_bridge_sockets_accept_rsp_t;

typedef struct _mangoh_bridge_sockets_connect_rsp_t
{
    int8_t id;
} __attribute__((packed)) mangoh_bridge_sockets_connect_rsp_t;

typedef struct _mangoh_bridge_sockets_connected_rsp_t
{
    int8_t result;
} __attribute__((packed)) mangoh_bridge_sockets_connected_rsp_t;

typedef struct _mangoh_bridge_sockets_connecting_rsp_t
{
    int8_t result;
} __attribute__((packed)) mangoh_bridge_sockets_connecting_rsp_t;

typedef struct _mangoh_bridge_sockets_read_rsp_t
{
    uint8_t data[MANGOH_BRIDGE_SOCKETS_DATA_LEN];
} __attribute__((packed)) mangoh_bridge_sockets_read_rsp_t;

typedef struct _mangoh_bridge_sockets_buff_t
{
    uint8_t  data[MANGOH_BRIDGE_SOCKETS_BUFF_LEN];
    uint16_t len;
} mangoh_bridge_sockets_buff_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Sockets server
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_sockets_server_t
{
    fd_set   readfds; ///< Read fd set
    int32_t  sockFd;  ///< Socket fd
    uint16_t port;    ///< Server port
} mangoh_bridge_sockets_server_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Sockets client info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_sockets_client_info_t
{
    mangoh_bridge_sockets_buff_t rxBuff;       ///< Receive buffer
    mangoh_bridge_sockets_buff_t txBuff;       ///< Send buffer
    int32_t                      sockFd;       ///< Socket fd
    uint8_t                      connected:1;  ///< Connected flag
    uint8_t                      connecting:1; ///< Connecting flag
    uint8_t                      reserved:6;
} mangoh_bridge_sockets_client_info_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Sockets client list
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_sockets_client_t
{
    mangoh_bridge_sockets_client_info_t info[MANGOH_BRIDGE_SOCKETS_MAX_CLIENTS]; ///< List of client sockets
    fd_set                              readfds;                                 ///< Read fd set
    fd_set                              writefds;                                ///< Write fd set
    fd_set                              errfds;                                  ///< Error fd set
    int32_t                             maxSockFd;                               ///< Maximum socket fd
    uint8_t                             nextId;                                  ///< Next client ID
} mangoh_bridge_sockets_client_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Sockets module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_sockets_t
{
    mangoh_bridge_sockets_client_t clients; ///< Clients info
    mangoh_bridge_sockets_server_t server;  ///< Server info
    void*                          bridge;  ///< Bridge module
} mangoh_bridge_sockets_t;

int mangoh_bridge_sockets_init(mangoh_bridge_sockets_t*, void*);
int mangoh_bridge_sockets_destroy(mangoh_bridge_sockets_t*);

#endif

