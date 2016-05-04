/*
 * @file mangoh_bridge_air_vantage.h
 *
 * Arduino bridge Air Vantage sub-module.
 *
 * This module contains functions and data structures for executing the custom Air Vantage portion of the Arduino
 * bridge protocol.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "legato.h"
#include "interfaces.h"
#include "packet.h"

#ifndef MANGOH_BRIDGE_AIR_VANTAGE_INCLUDE_GUARD
#define MANGOH_BRIDGE_AIR_VANTAGE_INCLUDE_GUARD

#define MANGOH_BRIDGE_AIR_VANTAGE_DATA_UPDATE_MAP_NAME                   "BrdigeDataHndlrs"
#define MANGOH_BRIDGE_AIR_VANTAGE_DATA_UPDATE_MAP_SIZE                   15

#define MANGOH_BRIDGE_AIR_VANTAGE_SESSION_START                          '0'
#define MANGOH_BRIDGE_AIR_VANTAGE_SESSION_END                            '1'
#define MANGOH_BRIDGE_AIR_VANTAGE_SUBSCRIBE                              '2'
#define MANGOH_BRIDGE_AIR_VANTAGE_PUSH_BOOLEAN                           '3'
#define MANGOH_BRIDGE_AIR_VANTAGE_PUSH_INTEGER                           '4'
#define MANGOH_BRIDGE_AIR_VANTAGE_PUSH_FLOAT                             '5'
#define MANGOH_BRIDGE_AIR_VANTAGE_PUSH_STRING                            '6'
#define MANGOH_BRIDGE_AIR_VANTAGE_AVAILABLE                              '7'
#define MANGOH_BRIDGE_AIR_VANTAGE_RECV                                   '8'

#define MANGOH_BRIDGE_AIR_VANTAGE_TIMESTAMP_LEN                          64
#define MANGOH_BRIDGE_AIR_VANTAGE_VALUE_MAX_LEN                          32
#define MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN                     32
#define MANGOH_BRIDGE_AIR_VANTAGE_RX_BUFF_SIZE                           0x4000
#define MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN                            256

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Air Vantage responses
 */
//------------------------------------------------------------------------------------------------------------------

typedef struct _mangoh_bridge_air_vantage_avail_rsp_t
{
    uint16_t result;
} mangoh_bridge_air_vantage_avail_rsp_t;

typedef struct _mangoh_bridge_air_vantage_recv_rsp_t
{
    uint8_t data[MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) mangoh_bridge_air_vantage_recv_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Air Vantage module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_air_vantage_t
{
    int8_t           rxBuffer[MANGOH_BRIDGE_AIR_VANTAGE_RX_BUFF_SIZE]; ///< Receive buffer
    le_hashmap_Ref_t dataUpdateHandlers;                               ///< Workflow Manager data update callback functions
    void*            bridge;                                           ///< Bridge module
    uint32_t         rxBuffLen;                                        ///< Number of bytes in Rx buffer
} mangoh_bridge_air_vantage_t;

int mangoh_bridge_air_vantage_init(mangoh_bridge_air_vantage_t*, void*);
int mangoh_bridge_air_vantage_destroy(mangoh_bridge_air_vantage_t*);

#endif
