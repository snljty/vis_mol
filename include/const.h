#pragma once
#ifndef CONST_H
#define CONST_H

extern const unsigned int max_atomic_number;

extern const char *const elements_list[];

// RGB color for each element
extern const float element_colors_list[];

const float *element_color(unsigned int ielement);

// atomic covalent radius, from "Dalton Trans., 2008, 2832-2838", unit is Angstrom
extern const float covalence_radius_list[];

// Standard atomic weight. Computed from abundance and isotope masses
// the data was obtained from https://physics.nist.gov/cgi-bin/Compositions/stand_alone.pl?ele=&ascii=ascii2&isotype=all
// For radioactive elements, the mass corresponds to longest-living isotope
// Unit is Da, or simply be regarded as relative atomic mass (dimensionless)
extern const float standard_atomic_weight_list[];

#endif // CONST_H
