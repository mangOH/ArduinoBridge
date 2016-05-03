/*
 * @file processes.h
 *
 * Arduino bridge processes sub-module.
 *
 * Contains functions and data structures for executing the processes portion of the Arduino
 * bridge protocol.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "packet.h"

#ifndef SWI_MANGOH_BRIDGE_PROCESSES_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_PROCESSES_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_PROCESSES_RUN                      'R'
#define SWI_MANGOH_BRIDGE_PROCESSES_RUNNING                  'r'
#define SWI_MANGOH_BRIDGE_PROCESSES_WAIT                     'W'
#define SWI_MANGOH_BRIDGE_PROCESSES_CLEAN_UP                 'w'
#define SWI_MANGOH_BRIDGE_PROCESSES_READ_OUTPUT              'O'
#define SWI_MANGOH_BRIDGE_PROCESSES_AVAILABLE_OUTPUT         'o'
#define SWI_MANGOH_BRIDGE_PROCESSES_WRITE_INPUT              'I'

#define SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID              -1
#define SWI_MANGOH_BRIDGE_PROCESSES_INVALID_FILE             -1
#define SWI_MANGOH_BRIDGE_PROCESSES_INVALID_ID               -1
#define SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS                  256
#define SWI_MANGOH_BRIDGE_PROCESSES_CMD_LINE_MAX_LEN         128
#define SWI_MANGOH_BRIDGE_PROCESSES_CMD_LINE_MAX_ARGS        64
#define SWI_MANGOH_BRIDGE_PROCESSES_WRITE_INPUT_BUFF_LEN    (SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE - sizeof(int8_t))
#define SWI_MANGOH_BRIDGE_PROCESSES_SEPARATOR                0xFE

typedef struct _swi_mangoh_bridge_process_run_req_t
{
    int8_t                                    cmd[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_process_run_req_t;

typedef struct _swi_mangoh_bridge_process_running_req_t
{
    int8_t                                    id;
} __attribute__((packed)) swi_mangoh_bridge_process_running_req_t;

typedef struct _swi_mangoh_bridge_process_wait_req_t
{
    int8_t                                    id;
} __attribute__((packed)) swi_mangoh_bridge_process_wait_req_t;

typedef struct _swi_mangoh_bridge_process_cleanup_req_t
{
    int8_t                                    id;
} __attribute__((packed)) swi_mangoh_bridge_process_cleanup_req_t;

typedef struct _swi_mangoh_bridge_process_available_output_req_t
{
    int8_t                                    id;
} __attribute__((packed)) swi_mangoh_bridge_process_available_output_req_t;

typedef struct _swi_mangoh_bridge_process_read_output_req_t
{
    int8_t                                    id;
    uint8_t                                    len;
} __attribute__((packed)) swi_mangoh_bridge_process_read_output_req_t;

typedef struct _swi_mangoh_bridge_process_write_input_req_t
{
    int8_t                                    id;
    int8_t                                    data[SWI_MANGOH_BRIDGE_PROCESSES_WRITE_INPUT_BUFF_LEN];
} __attribute__((packed)) swi_mangoh_bridge_process_write_input_req_t;

typedef struct _swi_mangoh_bridge_process_run_rsp_t
{
    int8_t                                    result;
    int8_t                                    id;
} __attribute__((packed)) swi_mangoh_bridge_process_run_rsp_t;

typedef struct _swi_mangoh_bridge_process_running_rsp_t
{
    int8_t                                    result;
} __attribute__((packed)) swi_mangoh_bridge_process_running_rsp_t;

typedef struct _swi_mangoh_bridge_process_wait_rsp_t
{
    int16_t                                    exitCode;
} __attribute__((packed)) swi_mangoh_bridge_process_wait_rsp_t;

typedef struct _swi_mangoh_bridge_process_read_output_rsp_t
{
    int8_t                                    data[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];
} __attribute__((packed)) swi_mangoh_bridge_process_read_output_rsp_t;

typedef struct _swi_mangoh_bridge_process_avail_output_rsp_t
{
    int8_t                                    len;
} __attribute__((packed)) swi_mangoh_bridge_process_avail_output_rsp_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Process info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_process_t
{
    uint8_t                                 outputBuff[SWI_MANGOH_BRIDGE_PACKET_DATA_SIZE];        ///< Output buffer
    pid_t                                   pid;                                                   ///< Process ID
    int32_t                                 status;                                                ///< Process exit status
    int32_t                                 infp;                                                  ///< Process stdin
    int32_t                                 outfp;                                                 ///< Process stdout
    uint32_t                                outputBuffLen;                                         ///< Number of bytes in output buffer
} swi_mangoh_bridge_process_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Processes module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_processes_t
{
    swi_mangoh_bridge_process_t              list[SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS];             ///< List of processes
    uint8_t                                  nextId;                                                ///< Next internal process ID
    void*                                    bridge;                                                ///< Bridge module
} swi_mangoh_bridge_processes_t;

int swi_mangoh_bridge_processes_init(swi_mangoh_bridge_processes_t*, void*);
int swi_mangoh_bridge_processes_destroy(swi_mangoh_bridge_processes_t*);

#endif
