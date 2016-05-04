#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "legato.h"
#include "bridge.h"
#include "fileIO.h"

static int mangoh_bridge_fileio_getMode(unsigned char, int*);
static int mangoh_bridge_fileio_open(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_write(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_read(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_isDir(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_seek(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_position(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_size(void*, const unsigned char*, uint32_t);
static int mangoh_bridge_fileio_close(void*, const unsigned char*, uint32_t);

static int mangoh_bridge_fileio_reset(void*);

static int mangoh_bridge_fileio_getMode(unsigned char mode, int* retMode)
{
    *retMode =  (mode == 'r') ? O_RDONLY | O_EXCL | O_NONBLOCK:
                 (mode == 'w') ? O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK:
                 (mode == 'a') ? O_WRONLY | O_APPEND| O_CREAT | O_EXCL | O_NONBLOCK:0;

    return *retMode ? LE_OK:LE_BAD_PARAMETER;
}

static int mangoh_bridge_fileio_open(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_open_req_t* const req = (mangoh_bridge_fileio_open_req_t*)data;
    uint32_t filenameLen = size - sizeof(req->mode);

    char filename[MANGOH_BRIDGE_FILEIO_MAX_FILENAME_LEN] = {0};
    memcpy(filename, req->filename, filenameLen);
    LE_DEBUG("---> OPEN '%s' - '%c'", filename, req->mode);

    int32_t mode = 0;
    res = mangoh_bridge_fileio_getMode(req->mode, &mode);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_fileio_getMode() failed(%d/%d)", res, errno);
        goto cleanup;
    }

    fileio->fdList[fileio->nextId] = open(filename, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fileio->fdList[fileio->nextId] < 0)
    {
        if (errno == EEXIST)
        {
            LE_DEBUG("file('%s') exists", filename);
            mode &= ~O_CREAT;

            fileio->fdList[fileio->nextId] = open(filename, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (fileio->fdList[fileio->nextId] < 0)
            {
                LE_ERROR("ERROR open() failed(%d/%d)", fileio->fdList[fileio->nextId], errno);

                res = mangoh_bridge_sendNack(fileio->bridge);
                if (res != LE_OK)
                {
                    LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
                    goto cleanup;
                }

                res = LE_IO_ERROR;
                goto cleanup;
            }
        }
        else
        {
            LE_ERROR("ERROR open() failed(%d/%d)", fileio->fdList[fileio->nextId], errno);

            res = mangoh_bridge_sendNack(fileio->bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
                goto cleanup;
            }

            res = LE_IO_ERROR;
            goto cleanup;
        }
    }

    LE_DEBUG("file('%s') fd[%u](%d)", filename, fileio->nextId, fileio->fdList[fileio->nextId]);

    mangoh_bridge_fileio_open_rsp_t* const rsp = (mangoh_bridge_fileio_open_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;
    rsp->result = res;
    rsp->fd = fileio->nextId;
    LE_DEBUG("open(%d), result(%d)", rsp->fd, rsp->result);

    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_open_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

    fileio->nextId = (fileio->nextId + 1) % MANGOH_BRIDGE_FILEIO_LIST_SIZE;

cleanup:
    return res;
}

static int mangoh_bridge_fileio_write(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_write_req_t* const req = (mangoh_bridge_fileio_write_req_t*)data;
    const uint32_t numBytes = size - sizeof(req->fd);
    mangoh_bridge_packet_dumpBuffer(req->buffer, numBytes);

    LE_DEBUG("---> WRITE fd[%u](%d) size(%u)", req->fd, fileio->fdList[req->fd], numBytes);
    ssize_t bytesWrite = write(fileio->fdList[req->fd], req->buffer, numBytes);
    if (bytesWrite != numBytes)
    {
        LE_ERROR("ERROR write() fd[%u](%d) failed(%d/%d)",  req->fd, fileio->fdList[req->fd], bytesWrite, errno);

        res = mangoh_bridge_sendNack(fileio->bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_IO_ERROR;
        goto cleanup;
    }

    LE_DEBUG("fd[%u](%d), result(%d/%d)", req->fd, fileio->fdList[req->fd], bytesWrite, res);
    mangoh_bridge_fileio_write_rsp_t* const rsp = (mangoh_bridge_fileio_write_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;
    rsp->result = res;
    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_write_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_fileio_isDir(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_is_dir_req_t* const req = (mangoh_bridge_fileio_is_dir_req_t*)data;

    char filename[MANGOH_BRIDGE_FILEIO_MAX_FILENAME_LEN] = {0};
    memcpy(filename, req->filename, size);
    LE_DEBUG("---> IS DIRECTORY '%s'", filename);

    mangoh_bridge_fileio_is_dir_rsp_t* const rsp = (mangoh_bridge_fileio_is_dir_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;

    DIR* dir = opendir(filename);
    if (dir)
    {
        rsp->result = true;

        res = closedir(dir);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR closedir() failed(%d/%d)", res, errno);

            res = mangoh_bridge_sendNack(fileio->bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
                goto cleanup;
            }

            res = LE_IO_ERROR;
            goto cleanup;
        }
    }
    else
    {
        rsp->result = false;
    }

    LE_DEBUG("result(%u)", rsp->result);
    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_is_dir_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_fileio_seek(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_seek_req_t* const req = (mangoh_bridge_fileio_seek_req_t*)data;

    uint32_t pos = 0;
    memcpy(&pos, &req->pos, sizeof(pos));
    pos = ntohl(pos);

    LE_DEBUG("---> SEEK fd[%u](%d) pos(%u)", req->fd, fileio->fdList[req->fd], pos);
    res = lseek(fileio->fdList[req->fd], pos, SEEK_SET);
    if (res != pos)
    {
        LE_ERROR("ERROR lseek() fd[%u](%d) failed(%d/%d)", req->fd, fileio->fdList[req->fd], res, errno);

        res = mangoh_bridge_sendNack(fileio->bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_IO_ERROR;
        goto cleanup;
    }

    LE_DEBUG("fd[%u](%d) result(%u)", req->fd, fileio->fdList[req->fd], res);
    mangoh_bridge_fileio_seek_rsp_t* const rsp = (mangoh_bridge_fileio_seek_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;
    rsp->result = LE_OK;

    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_seek_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_fileio_position(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_position_req_t* const req = (mangoh_bridge_fileio_position_req_t*)data;
    LE_DEBUG("---> POSITION fd[%u](%d)", req->fd, fileio->fdList[req->fd]);

    const int32_t pos = lseek(fileio->fdList[req->fd], 0, SEEK_CUR);
    if (pos < 0)
    {
        LE_ERROR("ERROR lseek() fd(%d) failed(%d/%d)", req->fd, res, errno);

        res = mangoh_bridge_sendNack(fileio->bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_IO_ERROR;
        goto cleanup;
    }

    LE_DEBUG("fd[%u](%d) result(%u)", req->fd, fileio->fdList[req->fd], pos);
    mangoh_bridge_fileio_pos_rsp_t* const rsp = (mangoh_bridge_fileio_pos_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;
    rsp->result = 0;
    rsp->pos = htonl(pos);

    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_pos_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_fileio_size(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_size_req_t* const req = (mangoh_bridge_fileio_size_req_t*)data;
    LE_DEBUG("---> SIZE fd[%u](%d)", req->fd, fileio->fdList[req->fd]);

    res = lseek(fileio->fdList[req->fd], 0, SEEK_CUR);
    if (res < 0)
    {
        LE_ERROR("ERROR lseek() fd[%u](%d) failed(%d/%d)", req->fd, fileio->fdList[req->fd], res, errno);

        res = mangoh_bridge_sendNack(fileio->bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_IO_ERROR;
        goto cleanup;
    }

    struct stat buf = {0};
    res = fstat(fileio->fdList[req->fd], &buf);
    if (res < 0)
    {
        LE_ERROR("ERROR fstat() fd[%u](%d) failed(%d/%d)", req->fd, fileio->fdList[req->fd], res, errno);

        res = mangoh_bridge_sendNack(fileio->bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_IO_ERROR;
        goto cleanup;
    }

    const uint32_t fileSize = buf.st_size;
    LE_INFO("fd[%u](%d) size(%u)", req->fd, fileio->fdList[req->fd], fileSize);

    mangoh_bridge_fileio_size_rsp_t* const rsp = (mangoh_bridge_fileio_size_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;

    rsp->result = LE_OK;
    rsp->size = htonl(fileSize);
    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_size_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_fileio_read(void* param, const unsigned char* data, uint32_t size)
{
    const mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_read_req_t* const req = (mangoh_bridge_fileio_read_req_t*)data;
    LE_DEBUG("---> READ fd[%u](%d) bytes(%u)", req->fd, fileio->fdList[req->fd], req->size);

    mangoh_bridge_fileio_read_rsp_t* const rsp = (mangoh_bridge_fileio_read_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;
    const ssize_t bytesRead = read(fileio->fdList[req->fd], rsp->data, req->size);
    if (bytesRead < 0)
    {
        LE_ERROR("ERROR read() fd[%u](%d) failed(%d/%d)",  req->fd, fileio->fdList[req->fd], bytesRead, errno);

        res = mangoh_bridge_sendNack(fileio->bridge);
        if (res != LE_OK)
        {
            LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
            goto cleanup;
        }

        res = LE_IO_ERROR;
        goto cleanup;
    }

    rsp->len = bytesRead;
    LE_DEBUG("fd[%u](%d), result(%d)", req->fd, fileio->fdList[req->fd], rsp->len);
    mangoh_bridge_packet_dumpBuffer(rsp->data, rsp->len);

    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(rsp->len) + rsp->len);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    return res;
}

static int mangoh_bridge_fileio_close(void* param, const unsigned char* data, uint32_t size)
{
    mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(data);

    const mangoh_bridge_fileio_close_req_t* const req = (mangoh_bridge_fileio_close_req_t*)data;
    LE_DEBUG("---> CLOSE fd[%u](%d)", req->fd, fileio->fdList[req->fd]);

    res = close(fileio->fdList[req->fd]);
    if (res != LE_OK)
    {
        if (errno == EBADF)
        {
            LE_WARN("WARNING close() fd[%u](%d) failed(%d/%d)", req->fd, fileio->fdList[req->fd], res, errno);
            res = LE_OK;
        }
        else
        {
            LE_ERROR("ERROR close() fd[%u](%d) failed(%d/%d)", req->fd, fileio->fdList[req->fd], res, errno);

            res = mangoh_bridge_sendNack(fileio->bridge);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR mangoh_bridge_sendNack() failed(%d)", res);
                goto cleanup;
            }

            res = LE_IO_ERROR;
            goto cleanup;
        }
    }

    mangoh_bridge_fileio_close_rsp_t* const rsp = (mangoh_bridge_fileio_close_rsp_t*)((mangoh_bridge_t*)fileio->bridge)->packet.msg.data;
    rsp->result = res;
    LE_DEBUG("fd[%u](%d) result(%u)", req->fd, fileio->fdList[req->fd], rsp->result);
    res = mangoh_bridge_sendResult(fileio->bridge, sizeof(mangoh_bridge_fileio_close_rsp_t));
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_sendResult() failed(%d)", res);
        goto cleanup;
    }

    fileio->fdList[req->fd] = MANGOH_BRIDGE_FILEIO_INVALID_FD;

cleanup:
    return res;
}

static int mangoh_bridge_fileio_reset(void* param)
{
    mangoh_bridge_fileio_t* fileio = (mangoh_bridge_fileio_t*)param;
    int32_t res = LE_OK;

    LE_ASSERT(fileio);

    uint32_t idx = 0;
    for (idx = 0; idx < MANGOH_BRIDGE_FILEIO_LIST_SIZE; idx++)
    {
        if (fileio->fdList[idx] != MANGOH_BRIDGE_FILEIO_INVALID_FD)
        {
            LE_DEBUG("close fd[%u](%d)", idx, fileio->fdList[idx]);
            res = close(fileio->fdList[idx]);
            if (res != LE_OK)
            {
                LE_ERROR("ERROR close() fd[%u](%d) failed(%d/%d)", idx, fileio->fdList[idx], res, errno);
                res = LE_IO_ERROR;
                goto cleanup;
            }

            fileio->fdList[idx] = MANGOH_BRIDGE_FILEIO_INVALID_FD;
        }
    }

    fileio->nextId = 0;

cleanup:
    return res;
}

int mangoh_bridge_fileio_init(mangoh_bridge_fileio_t* fileio, void* bridge)
{
    int32_t res = LE_OK;

    LE_ASSERT(fileio);
    LE_ASSERT(bridge);

    LE_DEBUG("init");

    fileio->bridge = bridge;
    memset(fileio->fdList, MANGOH_BRIDGE_FILEIO_INVALID_FD, sizeof(fileio->fdList));

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_OPEN, fileio, mangoh_bridge_fileio_open);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_CLOSE, fileio, mangoh_bridge_fileio_close);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_WRITE, fileio, mangoh_bridge_fileio_write);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_READ, fileio, mangoh_bridge_fileio_read);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_IS_DIRECTORY, fileio, mangoh_bridge_fileio_isDir);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_SEEK, fileio, mangoh_bridge_fileio_seek);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_POSITION, fileio, mangoh_bridge_fileio_position);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerCommandProcessor(fileio->bridge, MANGOH_BRIDGE_FILEIO_SIZE, fileio, mangoh_bridge_fileio_size);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerCommandProcessor() failed(%d)", res);
        goto cleanup;
    }

    res = mangoh_bridge_registerReset(fileio->bridge, fileio, mangoh_bridge_fileio_reset);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_registerReset() failed(%d)", res);
        goto cleanup;
    }

cleanup:
    LE_DEBUG("init completed(%d)", res);
    return res;
}

int mangoh_bridge_fileio_destroy(mangoh_bridge_fileio_t* fileio)
{
    int32_t res = LE_OK;

    LE_ASSERT(fileio);

    res = mangoh_bridge_fileio_reset(fileio);
    if (res != LE_OK)
    {
        LE_ERROR("ERROR mangoh_bridge_fileio_reset() failed(%d)", res);
    }

    return LE_OK;
}
