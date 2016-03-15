/*
 * @file swi_mangoh_bridge_air_vantage.h
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
#include "swi_mangoh_bridge_packet.h"

#ifndef SWI_MANGOH_BRIDGE_AIR_VANTAGE_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_DATA_UPDATE_MAP_NAME                   "BrdigeDataHndlrs"
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_DATA_UPDATE_MAP_SIZE                   15

#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_SESSION_START                          '0'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_SESSION_END                            '1'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_SUBSCRIBE                              '2'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_PUSH_BOOLEAN                           '3'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_PUSH_INTEGER                           '4'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_PUSH_FLOAT                             '5'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_PUSH_STRING                            '6'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_AVAILABLE                              '7'
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_RECV                                   '8'

#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_TIMESTAMP_LEN                          64
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_VALUE_MAX_LEN                          32
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_FIELD_NAME_MAX_LEN                     32
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_RX_BUFF_SIZE                           0x4000
#define SWI_MANGOH_BRIDGE_AIR_VANTAGE_MAX_MSG_LEN                            256

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge Air Vantage responses
 */
//------------------------------------------------------------------------------------------------------------------

typedef struct _swi_mangoh_bridge_air_vantage_avail_rsp_t
{
    uint16_t                                            result;
} swi_mangoh_bridge_air_vantage_avail_rsp_t;

typedef struct _swi_mangoh_bridge_air_vantage_recv_rsp_t
{
    uint8_t                                            data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_air_vantage_recv_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Air Vantage module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_air_vantage_t
{
    int8_t                                             rxBuffer[SWI_MANGOH_BRIDGE_AIR_VANTAGE_RX_BUFF_SIZE];            ///< Receive buffer
    le_hashmap_Ref_t                                   dataUpdateHandlers;                                              ///< Workflow Manager data update callback functions
    void*                                              bridge;                                                          ///< Bridge module
    uint32_t                                           rxBuffLen;                                                       ///< Number of bytes in Rx buffer
} swi_mangoh_bridge_air_vantage_t;

int swi_mangoh_bridge_air_vantage_init(swi_mangoh_bridge_air_vantage_t*, void*);
int swi_mangoh_bridge_air_vantage_destroy(swi_mangoh_bridge_air_vantage_t*);

#endif
