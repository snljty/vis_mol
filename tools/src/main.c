#include "embed_shaders.h"

int main(int argc, char *argv[]) {
    if (argc - 1 != 1) {
        fprintf(stderr, "Usage: \"%s\" SOURCE_ROOT_PATH\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char source_root_path[PATH_MAX + 1] = "";
    char dest_path[PATH_MAX + 9 + 1] = "";

    strncpy(source_root_path, argv[1], PATH_MAX);
    source_root_path[PATH_MAX] = '\0';
    size_t source_root_path_len = strlen(source_root_path);
    if (!source_root_path_len) {
        fprintf(stderr, "Error! Incorrect SOURCE_ROOT_PATH.\n");
        exit(EXIT_FAILURE);
    }

    if (source_root_path[source_root_path_len - 1] == splitter) {
        sprintf(dest_path, "%sshaders%c", source_root_path, splitter);
    } else {
        sprintf(dest_path, "%s%cshaders%c", source_root_path, splitter, splitter);
    }

    if (chdir(dest_path)) {
        fprintf(stderr, "Error! Incorrect source code tree or SOURCE_ROOT_PATH.\n");
        exit(EXIT_FAILURE);
    }
    
    foreach_file_in_dir(".", shader_src_to_header);

    return 0;
}

