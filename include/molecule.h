#pragma once
#ifndef MOLECULE_H
#define MOLECULE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#ifdef HAVE_UTHASH
#include <uthash.h>
#endif

#ifdef _WIN32
#include <windows.h>
#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#endif
#endif
#else
#include <limits.h>
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef HAVE_UTHASH
typedef struct {
    char element[3];
    unsigned int atomic_index;
    UT_hash_handle hh;
} Periodic_table_item;

void generate_elements_hash_table();
void destroy_elements_hash_table();

#endif

typedef char element_name[3];

// char (*elements)[3] = (char (*)[3])malloc(natoms * sizeof(char[3]));

typedef struct {
    unsigned int natoms;
    char title[BUFSIZ + 1];
    element_name *elements;
    unsigned int *atomic_indices;
    float *atomic_covalence_radius;
    float *coordinates;
    unsigned char *connections;
    float *distances; // atomic-pairwise distances
    float box[9];
    unsigned int *bonds_indices; // (iatom, jatom) pairs
    unsigned int nbonds;
    unsigned int ndim;
} Molecule;

bool is_line_blank(const char *line);

unsigned int get_atomic_index(const char *element);

void init_molecule(Molecule *mol);

Molecule *new_molecule();

Molecule *read_xyz(const char *ifilename);

Molecule *read_gjf(const char *ifilename);

Molecule *read_out(const char *ifilename);

Molecule *read_fch(const char *ifilename);

Molecule *read_molecule(const char *ifilename);

void destroy_molecule(Molecule *mol);

float *atom_coord(Molecule *mol, unsigned int iatom);

unsigned char *atom_connection(Molecule *mol, unsigned int iatom, unsigned int jatom);

float *atom_distance(Molecule *mol, unsigned int iatom, unsigned int jatom);

void get_intramolecular_connectivity(Molecule *mol);

void print_molecule(Molecule *mol); // const

void print_intramolecular_connectivity(Molecule *mol); // const

void print_box(Molecule *mol); // const

void allocate_molecule_memory(Molecule *mol);

void set_atomic_properties(Molecule *mol, unsigned int iatom);

void nullify_molecule(Molecule *mol);

#endif

