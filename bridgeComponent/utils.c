#include "legato.h"
#include "utils.h"

char* strreplace(const char* str, const char* old, const char* new)
{
    /* Adjust each of the below values to suit your needs. */

    /* Increment positions cache size initially by this number. */
    size_t cache_sz_inc = 16;
    /* Thereafter, each time capacity needs to be increased,
     * multiply the increment by this factor. */
    const size_t cache_sz_inc_factor = 3;
    /* But never increment capacity by more than this number. */
    const size_t cache_sz_inc_max = 1048576;

    char *pret, *ret = NULL;
    const char *pstr2, *pstr = str;
    size_t i, count = 0;
    ptrdiff_t *pos_cache = NULL;
    size_t cache_sz = 0;
    size_t cpylen, orglen, retlen, newlen, oldlen = strlen(old);

    /* Find all matches and cache their positions. */
    while ((pstr2 = strstr(pstr, old)) != NULL)
    {
        count++;

        /* Increase the cache size when necessary. */
        if (cache_sz < count) {
            cache_sz += cache_sz_inc;
            pos_cache = realloc(pos_cache, sizeof(*pos_cache) * cache_sz);
            if (pos_cache == NULL)
            {
                goto end_repl_str;
            }
            cache_sz_inc *= cache_sz_inc_factor;
            if (cache_sz_inc > cache_sz_inc_max)
            {
                cache_sz_inc = cache_sz_inc_max;
            }
        }

        pos_cache[count-1] = pstr2 - str;
        pstr = pstr2 + oldlen;
    }

    orglen = pstr - str + strlen(pstr);

    /* Allocate memory for the post-replacement string. */
    if (count > 0)
    {
        newlen = strlen(new);
        retlen = orglen + (newlen - oldlen) * count;
    } else    retlen = orglen;
    ret = malloc(retlen + 1);
    if (ret == NULL)
    {
        goto end_repl_str;
    }

    if (count == 0)
    {
        /* If no matches, then just duplicate the string. */
        strcpy(ret, str);
    } else
    {
        /* Otherwise, duplicate the string whilst performing
         * the replacements using the position cache. */
        pret = ret;
        memcpy(pret, str, pos_cache[0]);
        pret += pos_cache[0];
        for (i = 0; i < count; i++)
        {
            memcpy(pret, new, newlen);
            pret += newlen;
            pstr = str + pos_cache[i] + oldlen;
            cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - oldlen;
            memcpy(pret, pstr, cpylen);
            pret += cpylen;
        }
        ret[retlen] = '\0';
    }

end_repl_str:
    /* Free the cache and return the post-replacement string,
     * which will be NULL in the event of an error. */
    free(pos_cache);
    return ret;
}

pid_t popen2(char** command, int* infp, int* outfp)
{
    int p_stdin[2] = {0}, p_stdout[2] = {0};
    int32_t res = LE_OK;

    LE_ASSERT(command);

    res = pipe(p_stdin);
    if (res < 0)
    {
        LE_ERROR("ERROR pipe() failed(%d/%d)", res, errno);
        return res;
    }

    res = pipe(p_stdout);
    if (res < 0)
    {
        LE_ERROR("ERROR pipe() failed(%d/%d)", res, errno);
        return res;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        LE_ERROR("ERROR fork() failed(%d/%d)", res, errno);
        return pid;
    }
    else if (pid == 0)
    {
        // Code only executed by child process
        res = dup2(p_stdin[MANGOH_BRIDGE_UTILS_READ_FD], STDIN_FILENO);
        if (res < 0)
        {
            LE_ERROR("ERROR dup2() failed(%d/%d)", res, errno);
            _exit(EXIT_FAILURE);
        }

        res = dup2(p_stdout[MANGOH_BRIDGE_UTILS_WRITE_FD], STDOUT_FILENO);
        if (res < 0)
        {
            LE_ERROR("ERROR dup2() failed(%d/%d)", res, errno);
            _exit(EXIT_FAILURE);
        }

        //close unused descriptors on child process.
        res = close(p_stdin[MANGOH_BRIDGE_UTILS_READ_FD]);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            _exit(EXIT_FAILURE);
        }

        res = close(p_stdin[MANGOH_BRIDGE_UTILS_WRITE_FD]);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            _exit(EXIT_FAILURE);
        }

        res = close(p_stdout[MANGOH_BRIDGE_UTILS_READ_FD]);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            _exit(EXIT_FAILURE);
        }

        res = close(p_stdout[MANGOH_BRIDGE_UTILS_WRITE_FD]);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            _exit(EXIT_FAILURE);
        }

        res = execvp(*command, command);
        if (res < 0)
        {
            LE_ERROR("ERROR execvp() failed(%d/%d)", res, errno);
        }

        perror("execvp");
        exit(1);
    }

    // close unused descriptors on parent process.
    res = close(p_stdin[MANGOH_BRIDGE_UTILS_READ_FD]);
    if (res < 0)
    {
        LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    res = close(p_stdout[MANGOH_BRIDGE_UTILS_WRITE_FD]);
    if (res < 0)
    {
        LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
        res = LE_IO_ERROR;
        goto cleanup;
    }

    if (infp == NULL)
    {
        res = close(p_stdin[MANGOH_BRIDGE_UTILS_WRITE_FD]);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }
    }
    else
        *infp = p_stdin[MANGOH_BRIDGE_UTILS_WRITE_FD];

    if (outfp == NULL)
    {
        res = close(p_stdout[MANGOH_BRIDGE_UTILS_READ_FD]);
        if (res < 0)
        {
            LE_ERROR("ERROR close() failed(%d/%d)", res, errno);
            res = LE_IO_ERROR;
            goto cleanup;
        }
    }
    else
        *outfp = p_stdout[MANGOH_BRIDGE_UTILS_READ_FD];

cleanup:
    return pid;
}


