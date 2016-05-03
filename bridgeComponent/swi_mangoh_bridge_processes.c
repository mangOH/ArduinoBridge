#include <arpa/inet.h>
#include "legato.h"
#include "swi_mangoh_bridge_utils.h"
#include "swi_mangoh_bridge_processes.h"
#include "swi_mangoh_bridge.h"

static int swi_mangoh_bridge_processes_run(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_processes_running(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_processes_wait(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_processes_cleanup(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_processes_readOutput(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_processes_availableOutput(void*, const unsigned char*, uint32_t);
static int swi_mangoh_bridge_processes_writeInput(void*, const unsigned char*, uint32_t);

static int swi_mangoh_bridge_processes_close(swi_mangoh_bridge_process_t*);
static int swi_mangoh_bridge_processes_reset(void*);

static int swi_mangoh_bridge_processes_close(swi_mangoh_bridge_process_t* process)
{
    int32_t res = LE_OK;

    res = close(process->infp);
    if (res < 0)
    {
        LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }
    process->infp = SWI_MANGOH_BRIDGE_PROCESSES_INVALID_FILE;

    res = close(process->outfp);
    if (res < 0)
    {
        LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }
    process->outfp = SWI_MANGOH_BRIDGE_PROCESSES_INVALID_FILE;

    process->pid = SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID;
    process->outputBuffLen = 0;
    process->status = 0;

    res = LE_OK;

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_run(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_run_req_t* const req = (swi_mangoh_bridge_process_run_req_t*)data;
    LE_DEBUG("---> RUN('%s')", req->cmd);

    const char search[2] = { SWI_MANGOH_BRIDGE_PROCESSES_SEPARATOR, 0 };
    char* cmdLine[SWI_MANGOH_BRIDGE_PROCESSES_CMD_LINE_MAX_ARGS] = {0};
    char* token = strtok((char*)req->cmd, search);
    uint16_t numArgs = 0;

    while (token)
    {
        LE_DEBUG("arg('%s')", token);
        cmdLine[numArgs++] = token;
        token = strtok(NULL, search);
    }

    swi_mangoh_bridge_process_run_rsp_t* const rsp = (swi_mangoh_bridge_process_run_rsp_t*)((swi_mangoh_bridge_t*)processes->bridge)->packet.msg.data;

    processes->list[processes->nextId].pid = popen2((char**)&cmdLine,
            &processes->list[processes->nextId].infp, &processes->list[processes->nextId].outfp);
    if (processes->list[processes->nextId].pid == SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID)
    {
        LE_ERROR("ERROR popen2() failed(%d/%d)", res, errno);
        rsp->result = SWI_MANGOH_BRIDGE_RESULT_FAILED;
        rsp->id = 0;
    }
    else
    {
        LE_DEBUG("started process[%u](%d)", processes->nextId, processes->list[processes->nextId].pid);
        rsp->result = SWI_MANGOH_BRIDGE_RESULT_OK;
        rsp->id = processes->nextId;

        processes->nextId = (processes->nextId + 1) % SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS;
    }

    res = swi_mangoh_bridge_sendResult(processes->bridge, sizeof(swi_mangoh_bridge_process_run_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_running(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_running_req_t* const req = (swi_mangoh_bridge_process_running_req_t*)data;
    LE_DEBUG("---> RUNNING(%u)", req->id);

    swi_mangoh_bridge_process_running_rsp_t* const rsp = (swi_mangoh_bridge_process_running_rsp_t*)((swi_mangoh_bridge_t*)processes->bridge)->packet.msg.data;

    if ((req->id >= SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS) || (processes->list[req->id].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID))
    {
        LE_DEBUG("check process(%u/%u)", req->id, processes->list[req->id].pid);

        res = waitpid(processes->list[req->id].pid, &processes->list[req->id].status, WNOHANG);
        if (res < 0)
        {
            if (errno == ECHILD)
            {
                LE_DEBUG("process[%u](%u) NOT running", req->id, processes->list[req->id].pid);
                rsp->result = false;
            }
            else
            {
                LE_ERROR("ERROR waitpid() failed(%d/%d)", res, errno);
                res = LE_FAULT;
                goto cleanup;
            }
        }
        else if (!res)
        {
            LE_DEBUG("process[%u](%u) running", req->id, processes->list[req->id].pid);
            rsp->result = true;
        }
        else if (res == processes->list[req->id].pid)
        {
            if (WIFEXITED(processes->list[req->id].status))
            {
                LE_DEBUG("process[%u](%u) NOT running", req->id, processes->list[req->id].pid);
                rsp->result = false;
            }
            else
            {
                LE_ERROR("ERROR waitpid() invalid result(%d/%d)", res, errno);
                goto cleanup;
            }
        }
    }
    else
    {
        LE_DEBUG("invalid id(%u)", req->id);
        rsp->result = false;
    }

    res = swi_mangoh_bridge_sendResult(processes->bridge, sizeof(swi_mangoh_bridge_process_running_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_wait(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_wait_req_t* const req = (swi_mangoh_bridge_process_wait_req_t*)data;
    LE_DEBUG("---> WAIT(%u)", req->id);

    if ((req->id >= SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS) || (processes->list[req->id].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID))
    {
        swi_mangoh_bridge_process_wait_rsp_t* const rsp = (swi_mangoh_bridge_process_wait_rsp_t*)((swi_mangoh_bridge_t*)processes->bridge)->packet.msg.data;

        LE_DEBUG("wait process[%u](%u)", req->id, processes->list[req->id].pid);
        if (!WIFEXITED(processes->list[req->id].status))
        {
            do
            {
                res = waitpid(processes->list[req->id].pid, &processes->list[req->id].status, 0);
                if (res < 0)
                {
                    LE_ERROR("ERROR waitpid() failed(%d/%d)", res, errno);
                    res = LE_FAULT;
                    goto cleanup;
                }
            } while (!WIFEXITED(processes->list[req->id].status));
        }

        int16_t exitCode = WEXITSTATUS(processes->list[req->id].status);
        LE_DEBUG("process[%u](%u) exit(%d)", req->id, processes->list[req->id].pid, exitCode);
        rsp->exitCode = htons(exitCode);
    }
    else
    {
        LE_ERROR("ERROR invalid id(%u)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = swi_mangoh_bridge_sendResult(processes->bridge, sizeof(swi_mangoh_bridge_process_wait_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_cleanup(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_cleanup_req_t* const req = (swi_mangoh_bridge_process_cleanup_req_t*)data;
    LE_DEBUG("---> CLEANUP(%u)", req->id);

    if ((req->id >= SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS) || (processes->list[req->id].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID))
    {
        if (!WIFEXITED(processes->list[req->id].status))
        {
            LE_DEBUG("kill process[%u](%u)", req->id, processes->list[req->id].pid);
            res = kill(processes->list[req->id].pid, SIGTERM);
            if (res < 0)
            {
                LE_ERROR("ERROR kill() pid(%d) failed(%d/%d)", processes->list[req->id].pid, res, errno);
                res = LE_FAULT;
                goto cleanup;
            }

            do
            {
                res = waitpid(processes->list[req->id].pid, &processes->list[req->id].status, 0);
                if (res < 0)
                {
                    LE_ERROR("ERROR waitpid() failed(%d/%d)", res, errno);
                    res = LE_FAULT;
                    goto cleanup;
                }
            } while (!WIFEXITED(processes->list[req->id].status));
        }

        res = swi_mangoh_bridge_processes_close(&processes->list[req->id]);
        if (res)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_processes_close() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        LE_ERROR("ERROR invalid id(%u)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = swi_mangoh_bridge_sendResult(processes->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_readOutput(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_read_output_req_t* const req = (swi_mangoh_bridge_process_read_output_req_t*)data;
    LE_DEBUG("---> READ OUTPUT(%u) length(%u)", req->id, req->len);

    if ((req->id >= SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS) || (processes->list[req->id].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID))
    {
        size_t len = 0;
        uint8_t id = req->id;
        uint8_t reqLen = req->len;

        LE_DEBUG("outputBuffLen(%u)", processes->list[id].outputBuffLen);
        ssize_t bytesRead = read(processes->list[id].outfp,
                &processes->list[id].outputBuff[processes->list[id].outputBuffLen],
                sizeof(processes->list[id].outputBuff) - processes->list[id].outputBuffLen);
        LE_DEBUG("outfp(%d) read(%d)", processes->list[id].outfp, bytesRead);
        if (bytesRead < 0)
        {
            LE_ERROR("ERROR read() failed(%d/%d)", bytesRead, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        processes->list[id].outputBuffLen += bytesRead;
        LE_DEBUG("outputBuffLen(%u)", processes->list[id].outputBuffLen);

        if (processes->list[id].outputBuffLen)
        {
            swi_mangoh_bridge_process_read_output_rsp_t* const rsp = (swi_mangoh_bridge_process_read_output_rsp_t*)((swi_mangoh_bridge_t*)processes->bridge)->packet.msg.data;

            len = (processes->list[id].outputBuffLen > reqLen) ? reqLen:processes->list[id].outputBuffLen;
            LE_DEBUG("len(%u)", len);
            memcpy(rsp->data, processes->list[id].outputBuff, len);
            memmove(processes->list[id].outputBuff, &processes->list[id].outputBuff[len], processes->list[id].outputBuffLen - len);
            processes->list[id].outputBuffLen -= len;
            LE_DEBUG("outputBuffLen(%u)", processes->list[id].outputBuffLen);
        }

        res = swi_mangoh_bridge_sendResult(processes->bridge, len);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
            goto cleanup;
        }
    }
    else
    {
        LE_ERROR("ERROR invalid id(%u)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_availableOutput(void* param, const unsigned char* data, uint32_t size)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_available_output_req_t* const req = (swi_mangoh_bridge_process_available_output_req_t*)data;
    LE_INFO("---> AVAILABLE OUTPUT(%u)", req->id);

    swi_mangoh_bridge_process_avail_output_rsp_t* const rsp = (swi_mangoh_bridge_process_avail_output_rsp_t*)((swi_mangoh_bridge_t*)processes->bridge)->packet.msg.data;

    if ((req->id >= SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS) || (processes->list[req->id].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID))
    {
        ssize_t bytesRead = read(processes->list[req->id].outfp,
                &processes->list[req->id].outputBuff[processes->list[req->id].outputBuffLen],
                sizeof(processes->list[req->id].outputBuff) - processes->list[req->id].outputBuffLen);
        LE_DEBUG("outfp(%d) read(%d)", processes->list[req->id].outfp, bytesRead);
        if (bytesRead < 0)
        {
            LE_ERROR("ERROR read() failed(%d/%d)", bytesRead, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }

        processes->list[req->id].outputBuffLen += bytesRead;
        rsp->len = processes->list[req->id].outputBuffLen;
    }
    else
    {
        LE_ERROR("ERROR invalid id(%u)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = swi_mangoh_bridge_sendResult(processes->bridge, sizeof(swi_mangoh_bridge_process_avail_output_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_writeInput(void* param, const unsigned char* data, uint32_t size)
{
    const swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(data);

    const swi_mangoh_bridge_process_write_input_req_t* const req = (swi_mangoh_bridge_process_write_input_req_t*)data;
    LE_DEBUG("---> WRITE INPUT(%u) '%s'", req->id, req->data);

    if ((req->id >= SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS) || (processes->list[req->id].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID))
    {
        ssize_t bytesWrite = write(processes->list[req->id].infp, req->data, size - sizeof(req->id));
        LE_DEBUG("infp(%d) write(%d)", processes->list[req->id].infp, bytesWrite);
        if (bytesWrite < 0)
        {
            LE_ERROR("ERROR write() failed(%d/%d)", bytesWrite, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }
    }
    else
    {
        LE_ERROR("ERROR invalid id(%u)", req->id);
        res = LE_BAD_PARAMETER;
        goto cleanup;
    }

    res = swi_mangoh_bridge_sendResult(processes->bridge, 0);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int swi_mangoh_bridge_processes_reset(void* param)
{
    swi_mangoh_bridge_processes_t* processes = (swi_mangoh_bridge_processes_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(processes);

    uint32_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS; idx++)
    {
        if (processes->list[idx].pid != SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID)
        {
            res = kill(processes->list[idx].pid, SIGTERM);
            if (res < 0)
            {
                LE_ERROR("ERROR kill() pid(%d) failed(%d/%d)", processes->list[idx].pid, res, errno);
                res = LE_FAULT;
                goto cleanup;
            }

            res = swi_mangoh_bridge_processes_close(&processes->list[idx]);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR swi_mangoh_bridge_processes_close() failed(%d)", res);
            }
        }

        memset(processes->list[idx].outputBuff, 0, sizeof(processes->list[idx].outputBuff));
    }

cleanup:
    return res;
}

int swi_mangoh_bridge_processes_init(swi_mangoh_bridge_processes_t* processes, void* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(processes);
    LE_ASSERT(bridge);

    LE_DEBUG("init");

    uint32_t idx = 0;
    for (idx = 0; idx < SWI_MANGOH_BRIDGE_PROCESSES_NUM_IDS; idx++)
    {
        processes->list[idx].pid = SWI_MANGOH_BRIDGE_PROCESSES_INVALID_PID;
        processes->list[idx].infp = SWI_MANGOH_BRIDGE_PROCESSES_INVALID_FILE;
        processes->list[idx].outfp = SWI_MANGOH_BRIDGE_PROCESSES_INVALID_FILE;
    }

    processes->bridge = bridge;

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_RUN, processes, swi_mangoh_bridge_processes_run);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_RUNNING, processes, swi_mangoh_bridge_processes_running);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_WAIT, processes, swi_mangoh_bridge_processes_wait);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_CLEAN_UP, processes, swi_mangoh_bridge_processes_cleanup);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_READ_OUTPUT, processes, swi_mangoh_bridge_processes_readOutput);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_AVAILABLE_OUTPUT, processes, swi_mangoh_bridge_processes_availableOutput);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerCommandProcessor(processes->bridge, SWI_MANGOH_BRIDGE_PROCESSES_WRITE_INPUT, processes, swi_mangoh_bridge_processes_writeInput);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = swi_mangoh_bridge_registerReset(processes->bridge, processes, swi_mangoh_bridge_processes_reset);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_registerReset() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    LE_DEBUG("init completed(%d)", res);
    return res;
}

int swi_mangoh_bridge_processes_destroy(swi_mangoh_bridge_processes_t* processes)
{
    int32_t res = swi_mangoh_bridge_processes_reset(processes);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR swi_mangoh_bridge_processes_reset() failed(%d)", res);
    }

    return LE_OK;
}


