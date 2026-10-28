#pragma once
#ifndef EMBEG_SHADERS_H
#define EMBEG_SHADERS_H

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <windows.h>
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
    #include <direct.h>
    #ifndef chdir
        #define chdir _chdir
    #endif
#else
    #ifndef __linux__
        #error "Only supports Windows and Linux."
    #endif
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <limits.h>
#endif
#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif
#include <string.h>
#include <stdbool.h>

void get_this_exe_path(char *curr_exe_path);

typedef void (*file_callback)(const char *filename);

void shader_src_to_header(const char *ifilename);

void foreach_file_in_dir(const char *dir_path, file_callback callback);

extern const char splitter;

#endif

