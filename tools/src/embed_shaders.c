#include "embed_shaders.h"

#ifdef _WIN32
const char splitter = '\\';
#else
const char splitter = '/';
#endif

static bool is_blank(const char *s) {
    while (*s != '\0') {
        if (*s != ' ' && *s != '\t') {
            return false;
        }
        s += 1;
    }
    return true;
}

static void strip_glsl_comments(char *dst, const char *src, int *in_block_comment) {
    const char *s = src;
    char *d = dst;
    int in_string = 0;

    while (*s != '\0') {
        if (*in_block_comment) {
            if (s[0] == '*' && s[1] == '/') {
                *in_block_comment = 0;
                s += 2;
            } else {
                s += 1;
            }
        } else if (in_string) {
            if (*s == '\\' && s[1] != '\0') {
                *d ++ = *s ++;
                *d ++ = *s ++;
            } else {
                if (*s == '"') in_string = 0;
                *d ++ = *s ++;
            }
        } else {
            if (*s == '"') {
                in_string = 1;
                *d ++ = *s ++;
            } else if (s[0] == '/' && s[1] == '/') {
                break;
            } else if (s[0] == '/' && s[1] == '*') {
                *in_block_comment = 1;
                s += 2;
            } else {
                *d ++ = *s ++;
            }
        }
    }
    *d = '\0';

    while (d > dst && (d[-1] == ' ' || d[-1] == '\t')) {
        d -= 1;
        *d = '\0';
    }
}

void get_this_exe_path(char *curr_exe_path) {
    char *pos = NULL;

    #ifdef _WIN32
    {
        DWORD result = GetModuleFileName(NULL, curr_exe_path, PATH_MAX);
        if (result == 0 || (result == PATH_MAX && GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
            fprintf(stderr, "Error! Cannot get file path.\n");
            exit(EXIT_FAILURE);
        }
    }
    #else
    {
        ssize_t nchars = readlink("/proc/self/exe", curr_exe_path, PATH_MAX);
        if (nchars == -1 || nchars == PATH_MAX) {
            fprintf(stderr, "Error! Cannot get file path.\n");
            exit(EXIT_FAILURE);
        }
        curr_exe_path[nchars] = '\0';
    }
    #endif
    pos = strrchr(curr_exe_path, splitter);
    if (!pos) {
        fprintf(stderr, "Error! Cannot get file path splitter.\n");
        exit(EXIT_FAILURE);
    }
    *pos = splitter;
    pos[1] = '\0';
}

void shader_src_to_header(const char *filename) {
    char ifilename[BUFSIZ + 1] = "", ofilename[BUFSIZ + 13 + 1] = "", varname[BUFSIZ + 14 + 1] = "";
    char *pos = NULL;
    char line[BUFSIZ + 1] = "";
    int in_block_comment = 0;

    strcpy(ifilename, filename);
    strcpy(varname, ifilename);
    if ((pos = strrchr(varname, '.'))) *pos = '\0';
    strcat(varname, "ShaderSource");
    sprintf(ofilename, "..%cinclude%c%s.h", splitter, splitter, ifilename);

    FILE *ifile = fopen(ifilename, "r");
    if (!ifile) {
        fprintf(stderr, "Error! Cannot open \"%s\" for reading.\n", ifilename);
    }
    FILE *ofile = fopen(ofilename, "w");
    if (!ofile) {
        fprintf(stderr, "Error! Cannot open \"%s\" for writing.\n", ofilename);
    }

    fprintf(ofile, "const char %s[] = \\", varname);
    while (fgets(line, BUFSIZ, ifile)) {
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n') line[line_len - 1] = '\0';

        char clean_line[BUFSIZ + 1] = "";
        strip_glsl_comments(clean_line, line, &in_block_comment);

        if (is_blank(clean_line)) continue;

        fprintf(ofile, "\n    \"");
        const char *s = clean_line;
        char c;
        while ((c = *s ++) != '\0') {
            if (c == '"') {
                fputc('\\', ofile);
            } else if (c == '\\') {
                fputc('\\', ofile);
            }
            fputc(c, ofile);
        }
        fprintf(ofile, "\\n\"");
    }
    fprintf(ofile, ";\n\n");

    fclose(ifile);
    ifile = NULL;
    fclose(ofile);
    ofile = NULL;
}

void foreach_file_in_dir(const char *dir_path, file_callback callback) {
    #ifdef _WIN32
        char search_path[MAX_PATH];
        snprintf(search_path, MAX_PATH, "%s%c*", dir_path, splitter);

        WIN32_FIND_DATA find_data;
        HANDLE hFind = FindFirstFile(search_path, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "Cannot open directory: \"%s\"\n", dir_path);
            return;
        }

        do {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (!(strcmp(find_data.cFileName, ".") && strcmp(find_data.cFileName, ".."))) continue;

            callback(find_data.cFileName);
        } while (FindNextFile(hFind, &find_data));

        FindClose(hFind);

    #else
        DIR *dir = opendir(dir_path);
        if (!dir) {
            fprintf(stderr, "Cannot open directory: %s\n", dir_path);
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (!(strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))) continue;

            char full_path[PATH_MAX + 1];
            snprintf(full_path, PATH_MAX, "%s%c%s", dir_path, splitter, entry->d_name);

            struct stat st;
            if (stat(full_path, &st) != 0) continue; // no access to file property

            if (S_ISREG(st.st_mode)) callback(entry->d_name);
        }

        closedir(dir);
    #endif
}

