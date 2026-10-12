#include "align_alloc.h"
#include "const.h"
#include "molecule.h"

bool is_line_blank(const char *line) {
    if (!line) return true;
    char c;
    while ((c = *line ++)) {
        if (!isspace(c)) return false;
    }
    return true;
}

#ifndef HAVE_UTHASH
unsigned int get_atomic_index(const char *element) {
    for (unsigned int i = 1u; i <= max_atomic_number; ++ i) {
        if (!strcmp(elements_list[i], element)) return i;
    }
    return 0;
}
#else
static Periodic_table_item *elements_hash_table = NULL;
static bool hash_table_generated = false;

void generate_elements_hash_table() {
    Periodic_table_item *current = NULL;

    for (unsigned int i = 1u; i <= max_atomic_number; ++ i) {
        current = (Periodic_table_item *)my_malloc(sizeof(Periodic_table_item));
        current->atomic_index = i;
        // if uses `const char *element` instead of `const char element[3]`, modify to 
        // `current->element = elements_list[i];`
        // and 
        // `HASH_ADD_KEYPTR(hh, elements_hash_table, current->element, strlen(current->element), current);`
        strcpy(current->element, elements_list[i]);
        HASH_ADD_STR(elements_hash_table, element, current);
    }
    hash_table_generated = true;

    return;
}

void destroy_elements_hash_table() {
    Periodic_table_item *current = NULL;
    Periodic_table_item *next_node = NULL;

    HASH_ITER(hh, elements_hash_table, current, next_node) {
        HASH_DEL(elements_hash_table, current);
        my_free(current);
    }
    next_node = NULL;
    current = NULL;
    hash_table_generated = false;
}

unsigned int get_atomic_index(const char *element) {
    if (!hash_table_generated) generate_elements_hash_table();
    Periodic_table_item *res = NULL;

    HASH_FIND_STR(elements_hash_table, element, res);
    if (res) return res->atomic_index;
    return 0;
}
#endif


inline float *atom_coord(Molecule *mol, unsigned int iatom) {
    return mol->coordinates + iatom * 3;
}

inline unsigned char *atom_connection(Molecule *mol, unsigned int iatom, unsigned int jatom) {
    return mol->connections + iatom * mol->natoms + jatom;
}

inline float *atom_distance(Molecule *mol, unsigned int iatom, unsigned int jatom) {
    return mol->distances + iatom * mol->natoms + jatom;
}


void init_molecule(Molecule *mol) {
    mol->natoms = 0u;;
    memset(mol->title, 0, BUFSIZ + 1);
    mol->elements = NULL;
    mol->atomic_indices = NULL;
    mol->atomic_covalence_radius = NULL;
    mol->coordinates = NULL;
    mol->connections = NULL;
    mol->distances = NULL;
    memset(mol->box, 0, 9 * sizeof(float));
    mol->bonds_indices = NULL;
    mol->nbonds = 0u;
    mol->ndim = 0u;
}

Molecule *new_molecule() {
    Molecule *mol = (Molecule *)my_malloc(sizeof(Molecule));
    if (!mol) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }
    init_molecule(mol);
    return mol;
}

Molecule *read_molecule(const char *ifilename) {
    // check file availablility
    if (!ifilename) {
          fprintf(stderr, "Error! Null pointer to filename.\n");
          exit(EXIT_FAILURE);
    }
    const char *pos = strrchr(ifilename, '.');
    if (!pos) {
        fprintf(stderr, "Error! Cannot find filename suffix.\n");
        exit(EXIT_FAILURE);
    }

    if (!strcmp(pos, ".gjf")) {
        return read_gjf(ifilename);
    } else if (!strcmp(pos, ".xyz")) {
        return read_xyz(ifilename);
    } else if (!strcmp(pos, ".out") || !strcmp(pos, ".log")) {
        return read_out(ifilename);
    } else if (!strcmp(pos, ".fch") || !strcmp(pos, ".fchk")) {
        return read_fch(ifilename);
    } else {
        fprintf(stderr, "Error! The filename suffix cannot be understood.\n");
        exit(EXIT_FAILURE);
    }
}

Molecule *read_xyz(const char *ifilename) {
    // check file availablility
    if (!ifilename) {
        fprintf(stderr, "Error! The filename suffix should be \".xyz\".\n");
        exit(EXIT_FAILURE);
    }
    const char *pos = strrchr(ifilename, '.');
    if (!pos || strcmp(pos, ".xyz")) {
        fprintf(stderr, "Error! The filename suffix should be \".xyz\".\n");
        exit(EXIT_FAILURE);
    }
    FILE *ifile = fopen(ifilename, "r");
    if (!ifile) {
        fprintf(stderr, "Error! Cannot open \"%s\" for reading.\n", ifilename);
        exit(EXIT_FAILURE);
    }

    Molecule *mol = new_molecule();

    char line[BUFSIZ + 1] = "";
    mol->natoms = 0u;
    nullify_molecule(mol);

    if (!fgets(line, BUFSIZ, ifile) || sscanf(line, "%u", &mol->natoms) != 1) {
        fprintf(stderr, "Error! Cannot get amount of atoms.\n");
        exit(EXIT_FAILURE);
    }
    if (!fgets(mol->title, BUFSIZ, ifile)) {
        fprintf(stderr, "Error! Cannot read title.\n");
        exit(EXIT_FAILURE);
    }
    size_t title_len = strlen(mol->title);
    if (title_len) {
        if (mol->title[title_len - 1] == '\n') {
            mol->title[-- title_len] = '\0';
        }
    }
    allocate_molecule_memory(mol);

    const char *line_pos = NULL;
    if ((line_pos = strstr(mol->title, "Tv_1:"))) {
        float coord[3];
        if (sscanf(line_pos + strlen("Tv_1:"), "%g%g%g", coord, coord + 1, coord + 2) == 3) {
            memcpy(mol->box, coord, 3 * sizeof(float));
            mol->ndim = 1u;

            if ((line_pos = strstr(mol->title, "Tv_2:"))) {
                if (sscanf(line_pos + strlen("Tv_2:"), "%g%g%g", coord, coord + 1, coord + 2) == 3) {
                    memcpy(mol->box + 3, coord, 3 * sizeof(float));
                    mol->ndim = 2u;

                    if ((line_pos = strstr(mol->title, "Tv_3:"))) {
                        if (sscanf(line_pos + strlen("Tv_3:"), "%g%g%g", coord, coord + 1, coord + 2) == 3) {
                            memcpy(mol->box + 6, coord, 3 * sizeof(float));
                            mol->ndim = 3u;
                        } else {
                            fprintf(stderr, "Error! Cannot read box vector 3.\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                } else {
                    fprintf(stderr, "Error! Cannot read box vector 2.\n");
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            fprintf(stderr, "Error! Cannot read box vector 1.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    for (unsigned iatom = 0u; iatom < mol->natoms; ++ iatom) {
        if (!fgets(line, BUFSIZ, ifile) || sscanf(line, "%2s%g%g%g", 
            mol->elements[iatom], 
            atom_coord(mol, iatom), 
            atom_coord(mol, iatom) + 1, 
            atom_coord(mol, iatom) + 2) != 4) {
            fprintf(stderr, "Error! Cannot read atom %u.\n", iatom + 1);
            exit(EXIT_FAILURE);
        }
        set_atomic_properties(mol, iatom);
    }

    fclose(ifile);
    ifile = NULL;

    return mol;
}

Molecule *read_gjf(const char *ifilename) {
    // check file availablility
    if (!ifilename) {
        fprintf(stderr, "Error! The filename suffix should be \".gjf\".\n");
        exit(EXIT_FAILURE);
    }
    const char *pos = strrchr(ifilename, '.');
    if (!pos || strcmp(pos, ".gjf")) {
        fprintf(stderr, "Error! The filename suffix should be \".gjf\".\n");
        exit(EXIT_FAILURE);
    }
    FILE *ifile = fopen(ifilename, "r");
    if (!ifile) {
        fprintf(stderr, "Error! Cannot open \"%s\" for reading.\n", ifilename);
        exit(EXIT_FAILURE);
    }

    Molecule *mol = new_molecule();

    char line[BUFSIZ + 1] = "";

    bool found_blank_line = false;
    while (fgets(line, BUFSIZ, ifile)) {
        if (is_line_blank(line)) {
            found_blank_line = true;
            break;
        }
    }
    if (!found_blank_line) {
        fprintf(stderr, "Error! Cannot find the first blank line.\n");
        exit(EXIT_FAILURE);
    }

    found_blank_line = false;
    while (fgets(line, BUFSIZ, ifile)) {
        if (is_line_blank(line)) {
            found_blank_line = true;
            break;
        }
    }
    if (!found_blank_line) {
        fprintf(stderr, "Error! Cannot find the second blank line.\n");
        exit(EXIT_FAILURE);
    }

    int charge = 0, multiplicity = 1;
    if (!fgets(line, BUFSIZ, ifile) || sscanf(line, "%d%d", &charge, &multiplicity) != 2) {
        fprintf(stderr, "Error! Cannot read charge and multiplicity.\n");
        exit(EXIT_FAILURE);
    }

    mol->natoms = 0u;
    nullify_molecule(mol);
    found_blank_line = false;
    while (fgets(line, BUFSIZ, ifile)) {
        if (is_line_blank(line)) {
            found_blank_line = true;
            break;
        }
        char name[3];
        if (sscanf(line, "%2s", name) != 1) {
            fprintf(stderr, "Error! Cannot read element of atom %u.\n", mol->natoms);
        }
        if (strcmp(name, "Tv")) ++ mol->natoms;
    }
    if (!found_blank_line) {
        fprintf(stderr, "Error! Cannot find the third blank line.\n");
        exit(EXIT_FAILURE);
    }

    fseek(ifile, 0, SEEK_SET);

    while (fgets(line, BUFSIZ, ifile)) {
        if (is_line_blank(line)) break;
    }
    if (!fgets(mol->title, BUFSIZ, ifile) || is_line_blank(mol->title)) {
        fprintf(stderr, "Error! Cannot read title line.\n");
        exit(EXIT_FAILURE);
    }
    size_t title_len = strlen(mol->title);
    if (title_len) {
        if (mol->title[title_len - 1] == '\n') {
            mol->title[-- title_len] = '\0';
        }
    }

    while (fgets(line, BUFSIZ, ifile)) {
        if (is_line_blank(line)) break;
    }

    if (!fgets(line, BUFSIZ, ifile)) {
        fprintf(stderr, "Error! Cannot read charge and multiplicity.\n");
        exit(EXIT_FAILURE);
    }

    allocate_molecule_memory(mol);
    
    char name[3];
    float coord[3];
    unsigned int iatom = 0u;
    unsigned int ibox = 0u;
    while (fgets(line, BUFSIZ, ifile)) {
        if (is_line_blank(line)) break;
        if (sscanf(line, "%2s%g%g%g", name, coord, coord + 1, coord + 2) != 4) {
            fprintf(stderr, "Error! Cannot read atom %u.\n", iatom + 1);
            exit(EXIT_FAILURE);
        }
        if (strcmp(name, "Tv")) {
            if (iatom == mol->natoms) {
                fprintf(stderr, "Error! Extra atom found.\n");
                exit(EXIT_FAILURE);
            }
            strcpy(mol->elements[iatom], name);
            set_atomic_properties(mol, iatom);
            memcpy(atom_coord(mol, iatom), coord, 3 * sizeof(float));
            ++ iatom;
        } else {
            if (ibox >= 3u) {
                fprintf(stderr, "Error! Dimension of box is too large.\n");
                exit(EXIT_FAILURE);
            }
            memcpy(mol->box + ibox * 3, coord, 3 * sizeof(float));
            ++ ibox;
        }
    }
    if (iatom != mol->natoms) {
        fprintf(stderr, "Error! Missing atoms.\n");
        exit(EXIT_FAILURE);
    }
    mol->ndim = ibox;

    fclose(ifile);
    ifile = NULL;

    return mol;
}

Molecule *read_out(const char *ifilename) {
    // check file availablility
    if (!ifilename) {
        fprintf(stderr, "Error! The filename suffix should be \".out\" or \".log\".\n");
        exit(EXIT_FAILURE);
    }
    const char *pos = strrchr(ifilename, '.');
    if (!pos || (strcmp(pos, ".out") && strcmp(pos, ".log"))) {
        fprintf(stderr, "Error! The filename suffix should be \".out\" or \".log\".\n");
        exit(EXIT_FAILURE);
    }
    FILE *ifile = fopen(ifilename, "r");
    if (!ifile) {
        fprintf(stderr, "Error! Cannot open \"%s\" for reading.\n", ifilename);
        exit(EXIT_FAILURE);
    }

    Molecule *mol = new_molecule();

    mol->natoms = 0u;
    nullify_molecule(mol);

    char line[BUFSIZ + 1] = "";

    const char *label = "Standard orientation:";
    bool found_natoms = false;
    bool found_label = false;
    char *line_pos = NULL;
    const char natoms_label[] = " NAtoms=";

    // use the last time, the first time contains box vector
    while (fgets(line, BUFSIZ, ifile)) {
        if ((line_pos = strstr(line, natoms_label))) {
            found_natoms = true;
            if (sscanf(line_pos + strlen(natoms_label), "%u", &mol->natoms) != 1) {
                fprintf(stderr, "Error! Cannot get amount of atoms.\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    if (!found_natoms) {
        fprintf(stderr, "Error! Cannot get amount of atoms.\n");
        exit(EXIT_FAILURE);
    }
    fseek(ifile, 0, SEEK_SET);

    while (fgets(line, BUFSIZ, ifile)) {
        if (strstr(line, label)) {
            found_label = true;
            break;
        }
    }

    if (!found_label) {
        fseek(ifile, 0, SEEK_SET);
        label = "Input orientation:";
        while (fgets(line, BUFSIZ, ifile)) {
            if (strstr(line, label)) {
                found_label = true;
                break;
            }
        }
    }

    if (!found_label) {
        fprintf(stderr, "Error! Cannot find coordinates label.\n");
        exit(EXIT_FAILURE);
    }
    fseek(ifile, 0, SEEK_SET);

    allocate_molecule_memory(mol);

    while (fgets(line, BUFSIZ, ifile)) {
        if (strstr(line, label)) {
            for (unsigned int i = 0u; i < 4; ++ i) {
                if (!fgets(line, BUFSIZ, ifile)) {
                    fprintf(stderr, "Error! Cannot read coordinates.\n");
                    exit(EXIT_FAILURE);
                }
            }
            int index;
            float coord[3];
            unsigned int iatom = 0u;
            unsigned int ibox = 0u;
            while (fgets(line, BUFSIZ, ifile)) {
                if (strstr(line, "--------")) break;
                if (sscanf(line, "%*d%d%*d%g%g%g", 
                    &index, coord, coord + 1, coord + 2) != 4) {
                    fprintf(stderr, "Error! Cannot read atom %u.\n", iatom + 1);
                    exit(EXIT_FAILURE);
                }
                if (index != -2) {
                    if (iatom == mol->natoms) {
                        fprintf(stderr, "Error! Extra atom found.\n");
                        exit(EXIT_FAILURE);
                    }
                    if (index < 0 || index > (int)max_atomic_number) {
                        fprintf(stderr, "Error! Invalid atomic index: %d\n", index);
                        exit(EXIT_FAILURE);
                    }
                    strcpy(mol->elements[iatom], elements_list[index]);
                    set_atomic_properties(mol, iatom);
                    memcpy(atom_coord(mol, iatom), coord, 3 * sizeof(float));
                    ++ iatom;
                } else {
                    if (ibox >= 3u) {
                        fprintf(stderr, "Error! Dimension of box is too large.\n");
                        exit(EXIT_FAILURE);
                    }
                    memcpy(mol->box + ibox * 3, coord, 3 * sizeof(float));
                    ++ ibox;
                }
            }
            if (iatom != mol->natoms) {
                fprintf(stderr, "Error! Missing atoms.\n");
                exit(EXIT_FAILURE);
            }
            mol->ndim = ibox;
        }
    }

    fclose(ifile);
    ifile = NULL;

    return mol;
}

Molecule *read_fch(const char *ifilename) {
    // check file availablility
    if (!ifilename) {
        fprintf(stderr, "Error! The filename suffix should be \".fch\" or \".fchk\".\n");
        exit(EXIT_FAILURE);
    }
    const char *pos = strrchr(ifilename, '.');
    if (!pos || (strcmp(pos, ".fch") && strcmp(pos, ".fchk"))) {
        fprintf(stderr, "Error! The filename suffix should be \".fch\" or \".fchk\".\n");
        exit(EXIT_FAILURE);
    }
    FILE *ifile = fopen(ifilename, "r");
    if (!ifile) {
        fprintf(stderr, "Error! Cannot open \"%s\" for reading.\n", ifilename);
        exit(EXIT_FAILURE);
    }

    Molecule *mol = new_molecule();

    mol->natoms = 0u;
    nullify_molecule(mol);

    const float Angstrom_per_Bohr = 0.52917721092f;

    char line[BUFSIZ + 1] = "";

    if (!fgets(mol->title, BUFSIZ, ifile)) {
        fprintf(stderr, "Error! Cannot read title.\n");
        exit(EXIT_FAILURE);
    } else {
        char *line_pos = mol->title + strlen(mol->title) - 1;
        if (*line_pos == '\n') *line_pos -- = '\0';
        while (*line_pos == ' ' && line_pos - mol->title >= 0) {
            *line_pos -- = '\0';
        }
    }

    bool found_label = false;
    while (fgets(line, BUFSIZ, ifile)) {
        if (!strncmp(line, "Number of atoms", strlen("Number of atoms"))) {
            found_label = true;
            break;
        }
    }
    if (!found_label || strlen(line) < 44 || sscanf(line + 44, "%u", &mol->natoms) != 1) {
        fprintf(stderr, "Error! Cannot get amount of atoms.\n");
        exit(EXIT_FAILURE);
    }
    
    allocate_molecule_memory(mol);

    found_label = false;
    while (fgets(line, BUFSIZ, ifile)) {
        if (!strncmp(line, "Atomic numbers", strlen("Atomic numbers"))) {
            found_label = true;
            break;
        }
        if (!strncmp(line, "Number of translation vectors", strlen("Number of translation vectors"))) {
            if (strlen(line) < 44 || sscanf(line + 44, "%u", &mol->ndim) != 1) {
                fprintf(stderr, "Error! Cannot read dimension of system.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (!found_label) {
        fprintf(stderr, "Error! Cannot read atomic indices.\n");
        exit(EXIT_FAILURE);
    }
    const char *line_pos = NULL;
    char content[17] = "";
    content[16] = '\0';
    content[12] = '\0';
    for (unsigned int iatom = 0; iatom < mol->natoms; ++ iatom) {
        unsigned int index = 0u;
        if (iatom % 6 == 0) {
            if (!fgets(line, BUFSIZ, ifile)) {
                fprintf(stderr, "Error! Cannot read atom %u.\n", iatom + 1);
                exit(EXIT_FAILURE);
            }
            line_pos = line;
        }
        if (strlen(line_pos) < 12) {
            fprintf(stderr, "Error! Cannot read atom %u.\n", iatom + 1);
            exit(EXIT_FAILURE);
        }
        strncpy(content, line_pos, 12);
        line_pos += 12;
        if (sscanf(content, "%u", &index) != 1 || index > max_atomic_number) {
            fprintf(stderr, "Error! Cannot read atom %u.\n", iatom + 1);
            exit(EXIT_FAILURE);
        }
        strcpy(mol->elements[iatom], elements_list[index]);
        set_atomic_properties(mol, iatom);
    }

    found_label = false;
    while (fgets(line, BUFSIZ, ifile)) {
        if (!strncmp(line, "Current cartesian coordinates", strlen("Current cartesian coordinates"))) {
            found_label = true;
            break;
        }
    }
    if (!found_label) {
        fprintf(stderr, "Error! Cannot read coordinates.\n");
        exit(EXIT_FAILURE);
    }

    for (unsigned int i = 0; i < mol->natoms * 3; ++ i) {
        float value;
        if (i % 5 == 0) {
            if (!fgets(line, BUFSIZ, ifile)) {
                fprintf(stderr, "Error! Cannot read atom %u.\n", i / 3 + 1);
                exit(EXIT_FAILURE);
            }
            line_pos = line;
        }
        if (strlen(line_pos) < 16) {
            fprintf(stderr, "Error! Cannot read atom %u.\n", i / 3 + 1);
            exit(EXIT_FAILURE);
        }
        strncpy(content, line_pos, 16);
        line_pos += 16;
        if (sscanf(content, "%g", &value) != 1) {
            fprintf(stderr, "Error! Cannot read atom %u.\n", i / 3  + 1);
            exit(EXIT_FAILURE);
        }
        mol->coordinates[i] = value;
    }

    if (mol->ndim) {
        found_label = false;
        while (fgets(line, BUFSIZ, ifile)) {
            if (!strncmp(line, "Translation vectors", strlen("Translation vectors"))) {
                found_label = true;
                break;
            }
        }
        if (!found_label) {
            fprintf(stderr, "Error! Cannot read translation vectors.\n");
            exit(EXIT_FAILURE);
        }

        for (unsigned int idim = 0; idim < mol->ndim; ++ idim) {
            if (!fgets(line, BUFSIZ, ifile)) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
            line_pos = line;
            if (strlen(line_pos) < 16) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
            strncpy(content, line_pos, 16);
            if (sscanf(content, "%g", mol->box + idim * 3) != 1) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
            line_pos += 16;
            if (strlen(line_pos) < 16) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
            strncpy(content, line_pos, 16);
            if (sscanf(content, "%g", mol->box + idim * 3 + 1) != 1) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
            line_pos += 16;
            if (strlen(line_pos) < 16) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
            strncpy(content, line_pos, 16);
            if (sscanf(content, "%g", mol->box + idim * 3 + 2) != 1) {
                fprintf(stderr, "Error! Cannot read translation vector %u\n", idim + 1);
                exit(EXIT_FAILURE);
            }
        }
    }

    // dscal(mol->natoms * 3, Angstrom_per_Bohr, mol->coordinates, 1);
    // if (mol->box) dscal(9, Angstrom_per_Bohr, mol->box, 1);

    for (unsigned int i = 0u; i < mol->natoms * 3; ++ i) {
        mol->coordinates[i] *= Angstrom_per_Bohr;
    }
    if (mol->box) {
        for (unsigned int i = 0u; i < 9u; ++ i) {
            mol->box[i] *= Angstrom_per_Bohr;
        }
    }

    fclose(ifile);
    ifile = NULL;

    return mol;
}

void destroy_molecule(Molecule *mol) {
    if (!mol) return;
    if (mol->natoms) {
        if (mol->distances) {
            my_free(mol->distances);
            mol->distances = NULL;
        }
        if (mol->connections) {
            my_free(mol->connections);
            mol->connections = NULL;
        }
        if (mol->coordinates) {
            my_free(mol->coordinates);
            mol->coordinates = NULL;
        }
        if (mol->atomic_indices) {
            my_free(mol->atomic_indices);
            mol->atomic_indices = NULL;
        }
        if (mol->atomic_covalence_radius) {
            my_free(mol->atomic_covalence_radius);
            mol->atomic_covalence_radius = NULL;
        }
        if (mol->elements) {
            my_free(mol->elements);
            mol->elements = NULL;
        }
        if (mol->bonds_indices) {
            my_free(mol->bonds_indices);
            mol->bonds_indices = NULL;
        }
        mol->natoms = 0u;
        mol->nbonds = 0u;
    }
    my_free(mol);
}

/*
void get_intramolecular_connectivity(Molecule *mol) {
    // this scale_tol from `Multiwfn`
    const float scale_tol = 1.15f;

    float *atomic_covalence_radius_sum = (float *)my_malloc(mol->natoms * mol->natoms * sizeof(float));
    if (!atomic_covalence_radius_sum) {
          fprintf(stderr, "Error! Cannot allocate memory.\n");
          exit(EXIT_FAILURE);
    }
    memset(atomic_covalence_radius_sum, 0, mol->natoms * mol->natoms * sizeof(float));
    memset(mol->connections, 0, mol->natoms * mol->natoms);
    mol->nbonds = 0u;

    // maybe calculating atomic_pair_distances and atomic_covalence_radius by SIMD and optimized LAPACK+BLAS is a better idea.
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        *atom_distance(mol, iatom, iatom) = (unsigned char)0u;
        for (unsigned int jatom = iatom + 1; jatom < mol->natoms; ++ jatom) {
            float v1 = mol->atomic_covalence_radius[iatom] + mol->atomic_covalence_radius[jatom];
            atomic_covalence_radius_sum[iatom * mol->natoms + jatom] = v1;
            float dx = atom_coord(mol, iatom)[0] - atom_coord(mol, jatom)[0];
            float dy = atom_coord(mol, iatom)[1] - atom_coord(mol, jatom)[1];
            float dz = atom_coord(mol, iatom)[2] - atom_coord(mol, jatom)[2];
            float v2 = sqrtf(dx * dx + dy * dy + dz * dz);;
            mol->distances[iatom * mol->natoms + jatom] = v2;
            if (v2 <= v1 * scale_tol) {
                *atom_connection(mol, iatom, jatom) = (unsigned char)1u;
                *atom_connection(mol, jatom, iatom) = (unsigned char)1u;
                ++ mol->nbonds;
            }
        }
    }

    my_free(atomic_covalence_radius_sum);
    atomic_covalence_radius_sum = NULL;

    mol->bonds_indices = (unsigned int*)my_malloc(mol->nbonds * 2 * sizeof(unsigned int));
    unsigned int ibond = 0u;
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        for (unsigned int jatom = iatom + 1; jatom < mol->natoms; ++ jatom) {
            if (*atom_connection(mol, iatom, jatom)) {
                mol->bonds_indices[ibond * 2    ] = iatom;
                mol->bonds_indices[ibond * 2 + 1] = jatom;
                ++ ibond;
            }
        }
    }
}
*/

void get_intramolecular_connectivity(Molecule *mol) {
    // this scale_tol from `Multiwfn`
    const float scale_tol = 1.15f;

    // fill it to zero just in case the molecule is reused
    memset(mol->connections, 0, mol->natoms * mol->natoms);
    mol->nbonds = 0u;
    
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        *atom_distance(mol, iatom, iatom) = (unsigned char)0u;
        for (unsigned int jatom = iatom + 1; jatom < mol->natoms; ++ jatom) {
            float v1 = mol->atomic_covalence_radius[iatom] + mol->atomic_covalence_radius[jatom];
            float dx = atom_coord(mol, iatom)[0] - atom_coord(mol, jatom)[0];
            float dy = atom_coord(mol, iatom)[1] - atom_coord(mol, jatom)[1];
            float dz = atom_coord(mol, iatom)[2] - atom_coord(mol, jatom)[2];
            float v2 = sqrtf(dx * dx + dy * dy + dz * dz);
            *atom_distance(mol, iatom, jatom) = v2;

            if (v2 <= v1 * scale_tol) {
                *atom_connection(mol, iatom, jatom) = (unsigned char)1u;
                *atom_connection(mol, jatom, iatom) = (unsigned char)1u;
                ++ mol->nbonds;
            }
        }
    }

    mol->bonds_indices = (unsigned int*)my_malloc(mol->nbonds * 2 * sizeof(unsigned int));
    unsigned int ibond = 0u;
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        for (unsigned int jatom = iatom + 1; jatom < mol->natoms; ++ jatom) {
            if (*atom_connection(mol, iatom, jatom)) {
                mol->bonds_indices[ibond * 2    ] = iatom;
                mol->bonds_indices[ibond * 2 + 1] = jatom;
                ++ ibond;
            }
        }
    }
}

void print_molecule(Molecule *mol) {
    printf("%5u\n", mol->natoms);
    printf("%s\n", mol->title);
    for (unsigned int iatom = 0; iatom < mol->natoms; ++ iatom) {
        printf(" %2s        %13.8f    %13.8f    %13.8f\n", 
            mol->elements[iatom], 
            atom_coord(mol, iatom)[0], 
            atom_coord(mol, iatom)[1], 
            atom_coord(mol, iatom)[2]);
    }
}

void print_intramolecular_connectivity(Molecule *mol) {
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        printf("%u", iatom + 1);
        for (unsigned int jatom = iatom + 1; jatom < mol->natoms; ++ jatom) {
            if (*atom_connection(mol, iatom, jatom)) {
                printf(" %u %.0f", jatom + 1, 1.);
            }
        }
        printf("\n");
    }
}

void print_box(Molecule *mol) {
    if (mol->box) {
        for (unsigned int ibox = 0u; ibox < 3u; ++ ibox) {
            for (unsigned int jbox = 0u; jbox < 3u; ++ jbox) {
                printf("    %13.10f", mol->box[ibox * 3 + jbox]);
            }
            printf("\n");
        }
    }
}

void allocate_molecule_memory(Molecule *mol) {
    if (!mol->natoms) return;
    if (mol->elements) my_free(mol->elements);
    mol->elements = (element_name *)my_malloc(mol->natoms * sizeof(element_name));
    if (mol->atomic_indices) my_free(mol->atomic_indices);
    mol->atomic_indices = (unsigned int *)my_malloc(mol->natoms * sizeof(unsigned int));
    if (mol->atomic_covalence_radius) my_free(mol->atomic_covalence_radius);
    mol->atomic_covalence_radius = (float *)my_malloc(mol->natoms * sizeof(float));
    if (mol->coordinates) my_free(mol->coordinates);
    mol->coordinates = (float *)my_malloc(3 * mol->natoms * sizeof(float));
    if (mol->connections) my_free(mol->connections);
    mol->connections = (unsigned char *)my_malloc(mol->natoms * mol->natoms);
    if (mol->distances) my_free(mol->distances);
    mol->distances = (float *)my_malloc(mol->natoms * mol->natoms * sizeof(float));
    if (!(mol->elements && mol->atomic_indices && mol->atomic_covalence_radius && 
          mol->coordinates && mol->connections && mol->distances)) {
          fprintf(stderr, "Error! Cannot allocate memory.\n");
          exit(EXIT_FAILURE);
    }
    memset(mol->connections, 0, mol->natoms * mol->natoms);
    memset(mol->distances, 0, mol->natoms * mol->natoms * sizeof(float));
}

void set_atomic_properties(Molecule *mol, unsigned int iatom) {
    mol->atomic_indices[iatom] = get_atomic_index(mol->elements[iatom]);
    if (!mol->atomic_indices[iatom]) {
        fprintf(stderr, "Warning: unrecognizable element: \"%s\", regarding as dummy atom.\n", mol->elements[iatom]);
    }
    mol->atomic_covalence_radius[iatom] = covalence_radius_list[mol->atomic_indices[iatom]];
}

void nullify_molecule(Molecule *mol) {
    if (mol->natoms) {
        fprintf(stderr, "Error! Molecule is not empty.\n");
        exit(EXIT_FAILURE);
    }
    mol->elements = NULL;
    mol->atomic_indices = NULL;
    mol->atomic_covalence_radius = NULL;
    mol->coordinates = NULL;
    mol->connections = NULL;
    mol->ndim = 0u;
}

