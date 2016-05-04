/*
 * @file utils.h
 *
 * Arduino bridge system call utilities module.
 *
 * This module contains system calls that are not provided by default in the Standard-C library
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
#ifndef MANGOH_BRIDGE_UTILS_INCLUDE_GUARD
#define MANGOH_BRIDGE_UTILS_INCLUDE_GUARD

#define MANGOH_BRIDGE_UTILS_READ_FD                 0
#define MANGOH_BRIDGE_UTILS_WRITE_FD                1

char* strreplace(const char*, const char*, const char*);
pid_t popen2(char**, int*, int*);

#endif
