/**
 * @file
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include <errno.h>
#include <termios.h>
#include <arpa/inet.h>
#include "interfaces.h"
#include "bridge.h"

static int mangoh_bridge_read(mangoh_bridge_t*, unsigned char*, unsigned int);
static int mangoh_bridge_write(const mangoh_bridge_t*, const unsigned char*, unsigned int);

static int mangoh_bridge_process_msg_idx(mangoh_bridge_t*);
static int mangoh_bridge_process_payload_len(mangoh_bridge_t*);
static int mangoh_bridge_process_crc(mangoh_bridge_t*);
static int mangoh_bridge_process_cmd(mangoh_bridge_t*);
static int mangoh_bridge_close(mangoh_bridge_t*);
static int mangoh_bridge_reset(mangoh_bridge_t*);
static int mangoh_bridge_process_payload(mangoh_bridge_t*);
static int mangoh_bridge_process_msg(mangoh_bridge_t*);

static int mangoh_bridge_excute_runners(mangoh_bridge_t*);
static int mangoh_bridge_excute_resets(mangoh_bridge_t*);
static void mangoh_bridge_eventHandler(int, short);

static void mangoh_bridge_SigTermEventHandler(int);
static int mangoh_bridge_start(mangoh_bridge_t*);
static int mangoh_bridge_stop(mangoh_bridge_t*);
static int mangoh_bridge_init(mangoh_bridge_t*);

static mangoh_bridge_t bridge;

static le_log_TraceRef_t BridgeTraceRef;

static int mangoh_bridge_read(mangoh_bridge_t* bridge, unsigned char* data, unsigned int len)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);
    LE_ASSERT(data);

    FD_ZERO(&bridge->readfds);
    FD_SET(bridge->serialFd, &bridge->readfds);

    struct timeval tv  = { .tv_sec = 0, .tv_usec = MANGOH_BRIDGE_READ_WAIT_MICROSEC };
    res = select(bridge->serialFd + 1, &bridge->readfds, NULL, NULL, &tv);
    if (res < 0)
    {
        LE_ERROR("ERROR select() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    if ((res > 0) && FD_ISSET(bridge->serialFd, &bridge->readfds))
    {
        unsigned char* ptr = (unsigned char*)data;
        unsigned int count = len;

        while (count > 0)
        {
            ssize_t bytesRead = read(bridge->serialFd, ptr, count);
            if (bytesRead > 0)
            {
                count -= bytesRead;
                ptr += bytesRead;
            }
            else
            {
                LE_ERROR("ERROR read() fd(%d) failed(%zd/%d)", bridge->serialFd, bytesRead, errno);
                sleep(1);

                res = mangoh_bridge_stop(bridge);
                if (res)
                {
                    LE_ERROR("ERROR mangoh_bridge_stop() failed(%d/%d)", res, errno);
                    goto cleanup;
                }

                res = mangoh_bridge_start(bridge);
                if (res)
                {
                    LE_ERROR("ERROR mangoh_bridge_start() failed(%d/%d)", res, errno);
                    goto cleanup;
                }

                res = LE_IO_ERROR;
                goto cleanup;
            }
        }

        LE_TRACE(BridgeTraceRef, "received(%u)", len);
        if(LE_IS_TRACE_ENABLED(BridgeTraceRef))
        {
            mangoh_bridge_packet_dumpBuffer(data, len);
        }
        res = len;
    }

cleanup:
    return res;
}

static int mangoh_bridge_write(const mangoh_bridge_t* bridge, const unsigned char* data, unsigned int len)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);
    LE_ASSERT(data);

    if(LE_IS_TRACE_ENABLED(BridgeTraceRef))
    {
        mangoh_bridge_packet_dumpBuffer(data, len);
    }

    const unsigned char* ptr = data;
    unsigned int msgLen = len;

    while (msgLen > 0)
    {
        ssize_t bytesWrite = write(bridge->serialFd, ptr, msgLen);
        if (bytesWrite < 0)
        {
            LE_ERROR("ERROR write() failed(%zd/%d)", bytesWrite, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        msgLen -= bytesWrite;
        ptr += bytesWrite;
    }

cleanup:
    return res;
}

static int mangoh_bridge_process_msg_idx(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    res = mangoh_bridge_read(bridge, &bridge->packet.msg.idx, sizeof(bridge->packet.msg.idx));
    if (res < 0)
    {
        LE_ERROR("ERROR mangoh_bridge_read() failed(%d)", res);
        goto cleanup;
    }

    LE_TRACE(BridgeTraceRef, "message index(%u)", bridge->packet.msg.idx);
    bridge->packet.crc = mangoh_bridge_packet_crcUpdate(bridge->packet.crc, &bridge->packet.msg.idx, sizeof(bridge->packet.msg.idx));
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_process_payload_len(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    res = mangoh_bridge_read(bridge, (unsigned char*)&bridge->packet.msg.len, sizeof(bridge->packet.msg.len));
    if (res < 0)
    {
        LE_ERROR("ERROR mangoh_bridge_read() failed(%d)", res);
        goto cleanup;
    }

    bridge->packet.crc = mangoh_bridge_packet_crcUpdate(bridge->packet.crc, (unsigned char*)&bridge->packet.msg.len, sizeof(bridge->packet.msg.len));
    bridge->packet.msg.len = ntohs(bridge->packet.msg.len);
    if (bridge->packet.msg.len > sizeof(bridge->packet.msg.data))
    {
        LE_ERROR("ERROR invalid payload length(0x%04x > %u)", bridge->packet.msg.len, MANGOH_BRIDGE_PACKET_DATA_SIZE);
        res = LE_OUT_OF_RANGE;
        goto cleanup;
    }

    if (bridge->packet.msg.len)
    {
        LE_TRACE(BridgeTraceRef, "payload length(%u)", bridge->packet.msg.len);
        LE_TRACE(BridgeTraceRef, "---> payload");

        memset(bridge->packet.msg.data, 0, sizeof(bridge->packet.msg.data));
        res = mangoh_bridge_read(bridge, bridge->packet.msg.data, bridge->packet.msg.len);
        if (res < 0)
        {
            LE_ERROR("ERROR mangoh_bridge_read() failed(%d)", res);
            goto cleanup;
        }
    }

    bridge->packet.crc = mangoh_bridge_packet_crcUpdate(bridge->packet.crc, bridge->packet.msg.data, bridge->packet.msg.len);
    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_process_crc(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    res = mangoh_bridge_read(bridge, (unsigned char*)&bridge->packet.msg.crc, sizeof(bridge->packet.msg.crc));
    if (res < 0)
    {
        LE_ERROR("ERROR mangoh_bridge_read() failed(%d)", res);
        goto cleanup;
    }

    bridge->packet.msg.crc = ntohs(bridge->packet.msg.crc);
    LE_TRACE(BridgeTraceRef, "CRC (0x%04x/0x%04x)", bridge->packet.crc, bridge->packet.msg.crc);

    if (bridge->packet.crc != bridge->packet.msg.crc)
    {
        LE_ERROR("ERROR invalid crc(0x%04x != 0x%04x)", bridge->packet.crc, bridge->packet.msg.crc);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = LE_OK;

cleanup:
    return res;
}

static int mangoh_bridge_process_cmd(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    mangoh_bridge_packet_data_t* req = (mangoh_bridge_packet_data_t*)bridge->packet.msg.data;
    LE_TRACE(BridgeTraceRef, "command(0x%02x)", req->cmd);

    if (!bridge->cmdHdlrs[req->cmd].fcn || !bridge->cmdHdlrs[req->cmd].module)
    {
        LE_ERROR("ERROR unsupported command(0x%02x)", req->cmd);

        res = mangoh_bridge_sendNack(bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = bridge->cmdHdlrs[req->cmd].fcn(bridge->cmdHdlrs[req->cmd].module, req->buffer, bridge->packet.msg.len - sizeof(req->cmd));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR command processor('%c') failed(%d)", req->cmd, res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_close(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    LE_INFO("---> CLOSE");
    if (bridge->packet.msg.len != sizeof(bridge->packet.close))
    {
        LE_ERROR("ERROR invalid close command length(%u != %zu)", bridge->packet.msg.len, sizeof(bridge->packet.close));
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = mangoh_bridge_sendAck(bridge);
    if (res != LE_OK)
    {
          LE_ERROR("ERROR mangoh_bridge_sendAck() failed(%d)", res);
        goto cleanup;
    }

    bridge->closed = true;

cleanup:
    return res;
}

static int mangoh_bridge_reset(mangoh_bridge_t* bridge)
{
    unsigned int rxVersion[MANGOH_BRIDGE_PACKET_VERSION_SIZE] = {0};
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    LE_INFO("---> RESET");
    if (bridge->packet.msg.len != sizeof(bridge->packet.reset) + MANGOH_BRIDGE_PACKET_VERSION_SIZE)
    {
        LE_ERROR("ERROR invalid reset command length(%u != %zu)",
                bridge->packet.msg.len, sizeof(bridge->packet.reset) + MANGOH_BRIDGE_PACKET_VERSION_SIZE);

        res = mangoh_bridge_sendNack(bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    rxVersion[0] = bridge->packet.msg.data[sizeof(bridge->packet.reset)] - '0';
    rxVersion[1] = bridge->packet.msg.data[sizeof(bridge->packet.reset) + 1] - '0';
    rxVersion[2] = bridge->packet.msg.data[sizeof(bridge->packet.reset) + 2] - '0';

    LE_INFO("Rx version %u.%u.%u", rxVersion[0], rxVersion[1], rxVersion[2]);
    if (memcmp(&bridge->packet.msg.data[sizeof(bridge->packet.reset)], bridge->packet.version, sizeof(bridge->packet.version)))
    {
        LE_ERROR("ERROR unsupported version(%u.%u.%u != %c.%c.%c)",
                rxVersion[0], rxVersion[1], rxVersion[2],
                bridge->packet.version[0], bridge->packet.version[1], bridge->packet.version[2]);

        res = mangoh_bridge_sendNack(bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = mangoh_bridge_excute_resets(bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_excute_resets() failed(%d)", res);
        goto cleanup;
    }

    unsigned char* result = bridge->packet.msg.data;
    result[0] = res;
    memcpy(&result[1], bridge->packet.version, sizeof(bridge->packet.version));
    LE_TRACE(BridgeTraceRef, "result(%d) version(%u.%u.%u)", result[0], result[1] - '0', result[2] - '0', result[3] - '0');
    res = mangoh_bridge_sendResult(bridge, sizeof(uint8_t) + sizeof(bridge->packet.version));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

    bridge->closed = false;

cleanup:
    return res;
}

static int mangoh_bridge_process_payload(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    if (bridge->closed)
    {
        if (!memcmp(bridge->packet.msg.data, bridge->packet.reset, sizeof(bridge->packet.reset)))
        {
            res = mangoh_bridge_reset(bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_reset() failed(%d)", res);
                goto cleanup;
            }
        }
        else
        {
            LE_ERROR("ERROR unexpected command");
            mangoh_bridge_packet_dumpBuffer(bridge->packet.msg.data, bridge->packet.msg.len);
            res = LE_BAD_PARAMETER;
            goto cleanup;
        }
    }
    else
    {
        if (!memcmp(bridge->packet.msg.data, bridge->packet.close, sizeof(bridge->packet.close)))
        {
            res = mangoh_bridge_close(bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_close() failed(%d)", res);
                goto cleanup;
            }
        }
        else if (!memcmp(bridge->packet.msg.data, bridge->packet.reset, sizeof(bridge->packet.reset)))
        {
            res = mangoh_bridge_reset(bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_reset() failed(%d)", res);
                goto cleanup;
            }
        }
        else
        {
            res = mangoh_bridge_process_cmd(bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_process_cmd() failed(%d)", res);
                goto cleanup;
            }
        }
    }

cleanup:
    return res;
}

static int mangoh_bridge_process_msg(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    bridge->packet.crc = MANGOH_BRIDGE_PACKET_CRC_RESET;
    bridge->packet.crc = mangoh_bridge_packet_crcUpdate(bridge->packet.crc, &bridge->packet.msg.start, sizeof(bridge->packet.msg.start));

    res = mangoh_bridge_process_msg_idx(bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_process_msg_idx() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_process_payload_len(bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_process_payload_len() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_process_crc(bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_process_crc() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_process_payload(bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_process_payload() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_excute_runners(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    const le_sls_Link_t* link = le_sls_Peek(&bridge->runnerList);
    while (link)
    {
        mangoh_bridge_runner_item_t* currEntry = CONTAINER_OF(link, mangoh_bridge_runner_item_t, link);

        res = currEntry->info.fcn(currEntry->info.module);
        if (res)
        {
            LE_ERROR("ERROR bridge runner failed failed(%d)", res);
            goto cleanup;
        }

        link = le_sls_PeekNext(&bridge->runnerList, link);
    }

cleanup:
    return res;
}

static int mangoh_bridge_excute_resets(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    const le_sls_Link_t* link = le_sls_Peek(&bridge->resetList);
    while (link)
    {
        mangoh_bridge_reset_item_t* currEntry = CONTAINER_OF(link, mangoh_bridge_reset_item_t, link);

        res = currEntry->info.fcn(currEntry->info.module);
        if (res)
        {
            LE_ERROR("ERROR bridge reset failed failed(%d)", res);
            goto cleanup;
        }

        link = le_sls_PeekNext(&bridge->resetList, link);
    }

cleanup:
    return res;
}

static void mangoh_bridge_eventHandler(int fd, short events)
{
    mangoh_bridge_t* bridge = le_fdMonitor_GetContextPtr();
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    res = mangoh_bridge_excute_runners(bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_excute_runners() failed(%d)", res);
    }

    LE_TRACE(BridgeTraceRef, "read '%s'", MANGOH_BRIDGE_SERIAL_PORT_FN);
    res = mangoh_bridge_read(bridge, &bridge->packet.msg.start, sizeof(bridge->packet.msg.start));
    if (res < 0)
    {
        LE_ERROR("ERROR mangoh_bridge_read() failed(%d)", res);
    }
    else if (res > 0)
    {
        LE_TRACE(BridgeTraceRef, "read 0x%02x", bridge->packet.msg.start);
        if (bridge->packet.msg.start == MANGOH_BRIDGE_PACKET_START)
        {
            res = mangoh_bridge_process_msg(bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_process_msg() failed(%d)", res);
            }
        }
    }
}

static int mangoh_bridge_start(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    while (bridge->serialFd < 0)
    {
        LE_INFO("open '%s'", MANGOH_BRIDGE_SERIAL_PORT_FN);
        bridge->serialFd = open(MANGOH_BRIDGE_SERIAL_PORT_FN, O_RDWR | O_NOCTTY | O_EXCL);
        if (bridge->serialFd < 0)
        {
            LE_ERROR("ERROR open() '%s' failed(%d)", MANGOH_BRIDGE_SERIAL_PORT_FN, errno);
            sleep(1);
            continue;
        }

        res = fcntl(bridge->serialFd, F_SETFL, 0);
        if (res)
        {
            LE_ERROR("ERROR fcntl() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        struct termios tty = {0};
        res = tcgetattr(bridge->serialFd, &tty);
        if (res)
        {
            LE_ERROR("ERROR tcgetattr() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        res = cfsetospeed(&tty, B115200);
        if (res)
        {
            LE_ERROR("ERROR cfsetospeed() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        res = cfsetispeed(&tty, B115200);
        if (res)
        {
            LE_ERROR("ERROR cfsetispeed() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        cfmakeraw(&tty);

        tty.c_cc[VMIN] = 1;       // read blocks
        tty.c_cc[VTIME] = 0;      // seconds read timeout

        res = tcflush(bridge->serialFd, TCIFLUSH);
        if (res)
        {
            LE_ERROR("ERROR tcflush() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        res = tcsetattr(bridge->serialFd, TCSANOW, &tty);
        if (res)
        {
            LE_ERROR("ERROR tcsetattr() failed(%d/%d)", res, errno);
            res = LE_FAULT;
            goto cleanup;
        }

        bridge->fdMonitor = le_fdMonitor_Create(MANGOH_BRIDGE_FD_MONITOR_NAME, bridge->serialFd, mangoh_bridge_eventHandler, POLLIN);
        if (!bridge->fdMonitor)
        {
            LE_ERROR("ERROR le_fdMonitor_Create() failed");
            res = LE_FAULT;
            goto cleanup;
        }

        le_fdMonitor_SetContextPtr(bridge->fdMonitor, bridge);
    }

    LE_FATAL_IF(
      mangoh_muxCtrl_ArduinoDeassertReset() != LE_OK, "Couldn't deassert the Arduino reset");

cleanup:
    LE_INFO("bridge started(%d)", res);
    return res;
}

static int mangoh_bridge_stop(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    LE_INFO("bridge stop");
    if (bridge->serialFd != MANGOH_BRIDGE_SERIAL_FD_INVALID)
    {
        res = close(bridge->serialFd);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            res = errno;
            goto cleanup;
        }

        bridge->serialFd = MANGOH_BRIDGE_SERIAL_FD_INVALID;
        bridge->closed = false;

        le_fdMonitor_Delete(bridge->fdMonitor);
    }
    else
    {
        LE_WARN("WARNING already stopped");
    }

cleanup:
    return res;
}

static void mangoh_bridge_SigTermEventHandler(int sigNum)
{
    int32_t res = mangoh_bridge_destroy(&bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_destroy() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return;
}

static int mangoh_bridge_init(mangoh_bridge_t* bridge)
{
    char version[MANGOH_BRIDGE_PACKET_VERSION_SIZE] = MANGOH_BRIDGE_PACKET_VERSION;
    char reset[MANGOH_BRIDGE_PACKET_RESET_SIZE] = MANGOH_BRIDGE_PACKET_RESET;
    char close[MANGOH_BRIDGE_PACKET_CLOSE_SIZE] = MANGOH_BRIDGE_PACKET_CLOSE;
    int32_t res = LE_OK;

    LE_ASSERT(bridge);
    memset(bridge, 0, sizeof(mangoh_bridge_t));

    BridgeTraceRef = le_log_GetTraceRef("Bridge");

    LE_FATAL_IF(mangoh_muxCtrl_ArduinoAssertReset() != LE_OK, "Couldn't assert the Arduino reset");
    // Sleep for a while to ensure that the Arduino catches the reset
    usleep(300);

    memcpy(bridge->packet.version, version, sizeof(version));
    memcpy(bridge->packet.reset, reset, sizeof(reset));
    memcpy(bridge->packet.close, close, sizeof(close));
    bridge->serialFd = MANGOH_BRIDGE_SERIAL_FD_INVALID;
    bridge->packet.crc = MANGOH_BRIDGE_PACKET_CRC_RESET;
    bridge->packet.msg.crc = MANGOH_BRIDGE_PACKET_CRC_RESET;
    bridge->runnerList = LE_SLS_LIST_INIT;
    bridge->resetList = LE_SLS_LIST_INIT;

    res = mangoh_bridge_fileio_init(&bridge->modules.fileio, bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_fileio_init() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_console_init(&bridge->modules.console, bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_console_init() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_mailbox_init(&bridge->modules.mailbox, bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_mailbox_init() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_processes_init(&bridge->modules.processes, bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_processes_init() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_sockets_init(&bridge->modules.sockets, bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sockets_init() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_air_vantage_init(&bridge->modules.airVantage, bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_air_vantage_init() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

le_log_TraceRef_t mangoh_bridge_getTraceRef(void)
{
    return BridgeTraceRef;
}

int mangoh_bridge_sendAck(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    LE_INFO("<--- ACK");
    unsigned char* result = bridge->packet.msg.data;
    result[0] = MANGOH_BRIDGE_PACKET_ACK;

    res = mangoh_bridge_sendResult(bridge, sizeof(uint8_t));
    if (res)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

int mangoh_bridge_sendNack(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    LE_INFO("<--- NACK");
    unsigned char* result = bridge->packet.msg.data;
    result[0] = MANGOH_BRIDGE_PACKET_NACK;

    res = mangoh_bridge_sendResult(bridge, sizeof(uint8_t));
    if (res)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

int mangoh_bridge_sendResult(mangoh_bridge_t* bridge, uint32_t len)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    mangoh_bridge_packet_initResponse(&bridge->packet, len);
    LE_TRACE(BridgeTraceRef, "<--- RSP length(%u)", len);

    res = mangoh_bridge_write(bridge, (const uint8_t*)&bridge->packet.msg,
            sizeof(bridge->packet.msg.start) + sizeof(bridge->packet.msg.idx) + sizeof(bridge->packet.msg.len));
    if (res)
    {
        LE_ERROR("ERROR mangoh_bridge_write() failed(%d)", res);
        goto cleanup;
    }

    if (len)
    {
        res = mangoh_bridge_write(bridge, bridge->packet.msg.data, len);
        if (res)
        {
            LE_ERROR("ERROR mangoh_bridge_write() failed(%d)", res);
            goto cleanup;
        }
    }

    res = mangoh_bridge_write(bridge, (const unsigned char*)&bridge->packet.msg.crc, sizeof(bridge->packet.msg.crc));
    if (res)
    {
        LE_ERROR("ERROR mangoh_bridge_write() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

int mangoh_bridge_registerCommandProcessor(mangoh_bridge_t* bridge, uint8_t cmd, void* module, mangoh_bridge_cmd_proc_func_t cmdProc)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);
    if (bridge->cmdHdlrs[cmd].module || bridge->cmdHdlrs[cmd].fcn)
    {
        LE_ERROR("ERROR command processor '%c' already defined", cmd);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    LE_TRACE(BridgeTraceRef, "command('%c')", cmd);
    bridge->cmdHdlrs[cmd].module = module;
    bridge->cmdHdlrs[cmd].fcn = cmdProc;

cleanup:
    return res;
}

int mangoh_bridge_registerRunner(mangoh_bridge_t* bridge, void* module, mangoh_bridge_runner_func_t runner)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    mangoh_bridge_runner_item_t* nextEntry = calloc(1, sizeof(mangoh_bridge_runner_item_t));
    if (!nextEntry)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    nextEntry->info.module = module;
    nextEntry->info.fcn = runner;

    LE_TRACE(BridgeTraceRef, "register runner entry(%p)", nextEntry);
    le_sls_Queue(&bridge->runnerList, &nextEntry->link);

cleanup:
    return res;
}

int mangoh_bridge_registerReset(mangoh_bridge_t* bridge, void* module, mangoh_bridge_reset_func_t resetFcn)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    mangoh_bridge_reset_item_t* nextEntry = calloc(1, sizeof(mangoh_bridge_reset_item_t));
    if (!nextEntry)
    {
        LE_ERROR("ERROR calloc() failed");
        res = LE_NO_MEMORY;
        goto cleanup;
    }

    nextEntry->info.module = module;
    nextEntry->info.fcn = resetFcn;

    LE_TRACE(BridgeTraceRef, "register reset entry(%p)", nextEntry);
    le_sls_Queue(&bridge->resetList, &nextEntry->link);

cleanup:
    return res;
}

int mangoh_bridge_destroy(mangoh_bridge_t* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(bridge);

    res = mangoh_bridge_stop(bridge);
    if (res)
    {
        LE_ERROR("ERROR mangoh_bridge_stop() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_fileio_destroy(&bridge->modules.fileio);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_fileio_destroy() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_console_destroy(&bridge->modules.console);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_console_destroy() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_mailbox_destroy(&bridge->modules.mailbox);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_mailbox_destroy() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_processes_destroy(&bridge->modules.processes);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_processes_destroy() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_air_vantage_destroy(&bridge->modules.airVantage);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_air_vantage_destroy() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

__inline mangoh_bridge_air_vantage_t* mangoh_bridge_getAirVantageModule(void)
{
  return &bridge.modules.airVantage;
}

COMPONENT_INIT
{
    int32_t res = LE_OK;

    LE_INFO("MangOH Arduino Bridge Service Starting");

    le_sig_Block(SIGTERM);
    le_sig_SetEventHandler(SIGTERM, mangoh_bridge_SigTermEventHandler);

    res = mangoh_bridge_init(&bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_init() failed(%d)", res);
        goto cleanup;
    }

    LE_INFO("start bridge...");
    res = mangoh_bridge_start(&bridge);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_start() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return;
}
