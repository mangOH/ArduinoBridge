/*
 * @file bridge.h
 *
 * Arduino bridge module.
 *
 * This module is the main module for executing the Arduino Yun bridge protocol.  The module provides functions for
 * bridge initialization and destroying as well functions to register command processors, processing loop runners, and
 * reset command functions.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#include "legato.h"
#include "airVantage.h"
#include "packet.h"
#include "fileIO.h"
#include "console.h"
#include "mailbox.h"
#include "processes.h"
#include "sockets.h"

#ifndef SWI_MANGOH_BRIDGE_INCLUDE_GUARD
#define SWI_MANGOH_BRIDGE_INCLUDE_GUARD

#define SWI_MANGOH_BRIDGE_READ_WAIT_MICROSEC        50000
#define SWI_MANGOH_BRIDGE_NUMBER_OF_COMMANDS        256
#define SWI_MANGOH_BRIDGE_SERIAL_PORT_FN_MAX_LEN    32
#define SWI_MANGOH_BRIDGE_FD_MONITOR_NAME           "BridgeFdMonitor"
#define SWI_MANGOH_BRIDGE_SERIAL_PORT_FN            "/dev/ttyUSB0"
#define SWI_MANGOH_BRIDGE_SERIAL_FD_INVALID         -1

#define SWI_MANGOH_BRIDGE_RESULT_OK                 0
#define SWI_MANGOH_BRIDGE_RESULT_FAILED             1

typedef int (*swi_mangoh_bridge_cmd_proc_func_t)(void*, const unsigned char*, uint32_t);
typedef int (*swi_mangoh_bridge_runner_func_t)(void*);
typedef int (*swi_mangoh_bridge_reset_func_t)(void*);

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge command processor info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_cmd_proc_t
{
    swi_mangoh_bridge_cmd_proc_func_t            fcn;                                                   ///< Bridge sub-module command processor function
    void*                                        module;                                                ///< Bridge sub-module
} swi_mangoh_bridge_cmd_proc_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge runner info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_runner_t
{
    swi_mangoh_bridge_runner_func_t              fcn;                                                  ///< Bridge sub-module runner function
    void*                                        module;                                               ///< Bridge sub-module

} swi_mangoh_bridge_runner_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge runner info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_runner_item_t
{
    swi_mangoh_bridge_runner_t                   info;                                                ///< Runner Info
    le_sls_Link_t                                link;                                                ///< Linked list link

} swi_mangoh_bridge_runner_item_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge reset info
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_reset_t
{
    swi_mangoh_bridge_reset_func_t               fcn;                                                 ///< Bridge sub-module reset function
    void*                                        module;                                              ///< Bridge sub-module

} swi_mangoh_bridge_reset_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge reset linked list item
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_reset_item_t
{
    swi_mangoh_bridge_reset_t                    info;                                                ///< Reset info data
    le_sls_Link_t                                link;                                                ///< Linked list link
} swi_mangoh_bridge_reset_item_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge sub-modules
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_modules_t
{
    swi_mangoh_bridge_fileio_t                   fileio;                                              ///< Bridge file I/O module
    swi_mangoh_bridge_console_t                  console;                                             ///< Bridge console module
    swi_mangoh_bridge_mailbox_t                  mailbox;                                             ///< Bridge mailbox module
    swi_mangoh_bridge_processes_t                processes;                                           ///< Bridge processes module
    swi_mangoh_bridge_sockets_t                  sockets;                                             ///< Bridge sockets module
    swi_mangoh_bridge_air_vantage_t              airVantage;                                          ///< Bridge custom Air Vantage module
} swi_mangoh_bridge_modules_t;

//------------------------------------------------------------------------------------------------------------------
/**
 * Bridge module
 */
//------------------------------------------------------------------------------------------------------------------
typedef struct _swi_mangoh_bridge_t
{
    swi_mangoh_bridge_cmd_proc_t                 cmdHdlrs[SWI_MANGOH_BRIDGE_NUMBER_OF_COMMANDS];      ///< List of command processors
    swi_mangoh_bridge_packet_t                   packet;                                              ///< Bridge packet
    swi_mangoh_bridge_modules_t                  modules;                                             ///< Bridge sub-modules
    fd_set                                       readfds;                                             ///< Read fd set
    le_sls_List_t                                runnerList;                                          ///< Bridge functions run in each processing loop
    le_sls_List_t                                resetList;                                           ///< Bridge functions run when a reset is received
    le_fdMonitor_Ref_t                           fdMonitor;                                           ///< UART Bridge serial file monitor
    int                                          serialFd;                                            ///< UART Bridge serial file descriptor
    bool                                         closed;                                              ///< Bridge closed flag
} swi_mangoh_bridge_t;

int swi_mangoh_bridge_registerCommandProcessor(swi_mangoh_bridge_t*, uint8_t, void*, swi_mangoh_bridge_cmd_proc_func_t);
int swi_mangoh_bridge_registerRunner(swi_mangoh_bridge_t*, void*, swi_mangoh_bridge_runner_func_t);
int swi_mangoh_bridge_registerReset(swi_mangoh_bridge_t*, void*, swi_mangoh_bridge_reset_func_t);

int swi_mangoh_bridge_sendResult(swi_mangoh_bridge_t*, uint32_t);
int swi_mangoh_bridge_sendAck(swi_mangoh_bridge_t*);
int swi_mangoh_bridge_sendNack(swi_mangoh_bridge_t*);

swi_mangoh_bridge_air_vantage_t* swi_mangoh_bridge_getAirVantageModule(void);
le_log_TraceRef_t swi_mangoh_bridge_getTraceRef(void);

int swi_mangoh_bridge_destroy(swi_mangoh_bridge_t*);

#endif
