//
//  SupportFunctions.h
//  DBN
//
//  Created by Devon Hjelm on 7/20/12.
//  Copyright 2012 __MyCompanyName__. All rights reserved.
//

#ifndef DBN_SupportFunctions_h
#define DBN_SupportFunctions_h
#include "Types.h"

gsl_vector_int *makeShuffleList(int length);
void print_gsl(gsl_vector_float *v);
void print_gsl(gsl_vector_int *v);
void print_gsl(gsl_matrix_float *m);
void load_vec_into_matrix(gsl_vector_float *from, gsl_matrix_float *to);
void load_vec_into_matrix(gsl_matrix_float *from, gsl_matrix_float *to);
std::string readTextFile(const std::string& filename);
void save_gsl_matrix(gsl_matrix_float *m);
#endif
