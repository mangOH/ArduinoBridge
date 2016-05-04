/*
 * @file mangoh_bridge_packet.h
 *
 * Arduino bridge packet module.
 *
 * This module contains data structures used in the Arduino Yun bridge protocol as well as helper functions for
 * debugging packets, checking CRCs and initializing responses.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "legato.h"

#ifndef MANGOH_BRIDGE_PACKET_INCLUDE_GUARD
#define MANGOH_BRIDGE_PACKET_INCLUDE_GUARD

#define MANGOH_BRIDGE_PACKET_CMD_SIZE             sizeof(uint8_t)
#define MANGOH_BRIDGE_PACKET_DATA_SIZE            128

#define MANGOH_BRIDGE_PACKET_CRC_RESET            0xFFFF
#define MANGOH_BRIDGE_PACKET_START                0xFF
#define MANGOH_BRIDGE_PACKET_ACK                  0x00
#define MANGOH_BRIDGE_PACKET_NACK                 0xFF

#define MANGOH_BRIDGE_PACKET_VERSION              {'1','0','0'}
#define MANGOH_BRIDGE_PACKET_VERSION_SIZE         3
#define MANGOH_BRIDGE_PACKET_RESET                {'X','X'}
#define MANGOH_BRIDGE_PACKET_RESET_SIZE           2
#define MANGOH_BRIDGE_PACKET_CLOSE                {'X','X','X','X','X'}
#define MANGOH_BRIDGE_PACKET_CLOSE_SIZE           5

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge packet data
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_packet_data_t
{
    uint8_t cmd;
    uint8_t buffer[MANGOH_BRIDGE_PACKET_DATA_SIZE - MANGOH_BRIDGE_PACKET_CMD_SIZE];
} __attribute__((packed)) mangoh_bridge_packet_data_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge packet message
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_packet_msg_t
{
    uint8_t  start;                                ///< Packet start marker
    uint8_t  idx;                                  ///< Packet index
    uint16_t len;                                  ///< Packet length
    uint8_t  data[MANGOH_BRIDGE_PACKET_DATA_SIZE]; ///< Packet data
    uint16_t crc;                                  ///< Rx 16-bit CRC
} mangoh_bridge_packet_msg_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Brdige packet
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _mangoh_bridge_packet_t
{
    mangoh_bridge_packet_msg_t msg;                                        ///< Bridge packet
    unsigned char              version[MANGOH_BRIDGE_PACKET_VERSION_SIZE]; ///< Bridge version
    unsigned char              reset[MANGOH_BRIDGE_PACKET_RESET_SIZE];     ///< Reset packet
    unsigned char              close[MANGOH_BRIDGE_PACKET_CLOSE_SIZE];     ///< Close packet
    unsigned short             crc;                                        ///< Calculated 16-bit CRC
} mangoh_bridge_packet_t;

unsigned short mangoh_bridge_packet_crcUpdate(unsigned short, const unsigned char*, unsigned int);
void mangoh_bridge_packet_initResponse(mangoh_bridge_packet_t*, uint32_t);
void mangoh_bridge_packet_dumpBuffer(const unsigned char*, unsigned int);

#endif
