/*
 * @file fileIO.h
 *
 * Arduino bridge file I/O sub-module.
 *
 * This module contains functions and data structures for executing the file I/O portion of the Arduino
 * bridge protocol.  Callback functions are provided to support optional functionality with
 * Air Vantage.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */#include "packet.h"

#ifndef SWI_MANGOH_BRIDGE_FILEIO_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_FILEIO_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_FILEIO_OPEN                'F'
#define SWI_MANGOH_BRIDGE_FILEIO_CLOSE               'f'
#define SWI_MANGOH_BRIDGE_FILEIO_WRITE               'g'
#define SWI_MANGOH_BRIDGE_FILEIO_READ                'G'
#define SWI_MANGOH_BRIDGE_FILEIO_IS_DIRECTORY        'i'
#define SWI_MANGOH_BRIDGE_FILEIO_SEEK                's'
#define SWI_MANGOH_BRIDGE_FILEIO_POSITION            'S'
#define SWI_MANGOH_BRIDGE_FILEIO_SIZE                't'

#define SWI_MANGOH_BRIDGE_FILEIO_LIST_SIZE            256
#define SWI_MANGOH_BRIDGE_FILEIO_INVALID_FD           -1
#define SWI_MANGOH_BRIDGE_FILEIO_MAX_FILENAME_LEN     (SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE - sizeof(int8_t))
#define SWI_MANGOH_BRIDGE_FILEIO_WRITE_BUFF_SIZE      (SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE - sizeof(int8_t))

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge File I/O requests
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_fileio_open_req_t
{
    int8_t                                    mode;
    int8_t                                    filename[SWI_MANGOH_BRIDGE_FILEIO_MAX_FILENAME_LEN];
} __attribute__((packed)) swi_mangoh_bridge_fileio_open_req_t;

typedef struct _swi_mangoh_bridge_fileio_write_req_t
{
    int8_t                                    fd;
    uint8_t                                    buffer[SWI_MANGOH_BRIDGE_FILEIO_WRITE_BUFF_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_fileio_write_req_t;

typedef struct _swi_mangoh_bridge_fileio_is_dir_req_t
{
    int8_t                                    filename[SWI_MANGOH_BRIDGE_FILEIO_MAX_FILENAME_LEN];
} swi_mangoh_bridge_fileio_is_dir_req_t;

typedef struct _swi_mangoh_bridge_fileio_seek_req_t
{
    int8_t                                    fd;
    uint8_t                                    pos;
} __attribute__((packed)) swi_mangoh_bridge_fileio_seek_req_t;

typedef struct _swi_mangoh_bridge_fileio_position_req_t
{
    int8_t                                    fd;
} __attribute__((packed)) swi_mangoh_bridge_fileio_position_req_t;

typedef struct _swi_mangoh_bridge_fileio_size_req_t
{
    int8_t                                    fd;
} __attribute__((packed)) swi_mangoh_bridge_fileio_size_req_t;

typedef struct _swi_mangoh_bridge_fileio_read_req_t
{
    int8_t                                    fd;
    uint8_t                                    size;
} __attribute__((packed)) swi_mangoh_bridge_fileio_read_req_t;

typedef struct _swi_mangoh_bridge_fileio_close_req_t
{
    int8_t                                    fd;
} __attribute__((packed)) swi_mangoh_bridge_fileio_close_req_t;

//------------------------------------------------------------------------------------------------------------------
/*
 * Arduino Bridge File I/O responses
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_fileio_open_rsp_t
{
    int8_t                                    result;
    int8_t                                    fd;
} __attribute__((packed)) swi_mangoh_bridge_fileio_open_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_read_rsp_t
{
    uint8_t                                    len;
    uint8_t                                    data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_fileio_read_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_pos_rsp_t
{
    int8_t                                    result;
    uint32_t                                pos;
} __attribute__((packed)) swi_mangoh_bridge_fileio_pos_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_size_rsp_t
{
    int8_t                                    result;
    uint32_t                                size;
} __attribute__((packed)) swi_mangoh_bridge_fileio_size_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_write_rsp_t
{
    int8_t                                    result;
} __attribute__((packed)) swi_mangoh_bridge_fileio_write_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_close_rsp_t
{
    int8_t                                    result;
} __attribute__((packed)) swi_mangoh_bridge_fileio_close_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_seek_rsp_t
{
    int8_t                                    result;
} __attribute__((packed)) swi_mangoh_bridge_fileio_seek_rsp_t;

typedef struct _swi_mangoh_bridge_fileio_is_dir_rsp_t
{
    int8_t                                    result;
} __attribute__((packed)) swi_mangoh_bridge_fileio_is_dir_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * File I/O module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_fileio_t
{
    int                                      fdList[SWI_MANGOH_BRIDGE_FILEIO_LIST_SIZE];            ///< File descriptor list
    void*                                    bridge;                                                ///< Bridge module
    uint8_t                                  nextId;                                                ///< Next file ID
} swi_mangoh_bridge_fileio_t;

int swi_mangoh_bridge_fileio_init(swi_mangoh_bridge_fileio_t*, void*);
int swi_mangoh_bridge_fileio_destroy(swi_mangoh_bridge_fileio_t*);

#endif
