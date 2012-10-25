//
//  ReLULayer.cpp
//  DBN
//
//  Created by Devon Hjelm on 8/6/12.
//
//

#include "Layers.h"
#include "IO.h"


void ReLULayer::getExpectations(){
   //Apply softplus.  Might want to pass a general functor later
   for (int i = 0; i < nodenum; ++i){
      for (int j = 0; j < batchsize; ++j){
         float act = gsl_matrix_float_get(activations, i, j);
         float prob;
         //if (act < 100)
            prob = softplus(act);
         //else prob = act;
         gsl_matrix_float_set(expectations, i, j, prob);
      }
   }
}

void ReLULayer::sample(){
   // Sample = max(0, x+N(0,sigmoid(x)))
   for (int i = 0; i < nodenum; ++i){
      for (int j = 0; j < batchsize; ++j){
         float exp = gsl_matrix_float_get(expectations, i, j);
         float sam = fmaxf(0, exp + gsl_ran_gaussian(r, sigmoid(exp)));
         gsl_matrix_float_set(samples, i, j, sam);
      }
   }
   //print_gsl(expectations_);
   
}

void ReLULayer::update(ContrastiveDivergence *teacher){
   Layer::update(teacher);
}

float ReLULayer :: freeEnergy_contibution() {
   return 0;
}

//The input needs to be shaped depending on the type of visible layer.
void ReLULayer::shapeInput(DataSet* data){
   Input_t *input = data->train;
   float min, max;
   gsl_matrix_float_minmax(input, &min, &max);
   gsl_matrix_float_add_constant(input, -min);
   gsl_matrix_float_scale(input, (float)1/((float)(max-min)));
}

float ReLULayer::reconstructionCost(gsl_matrix_float *dataMat, gsl_matrix_float *modelMat){
   reconstruction_cost=0;
   gsl_matrix_float *squared_error = gsl_matrix_float_alloc(dataMat->size1, dataMat->size2);
   gsl_matrix_float_memcpy(squared_error, dataMat);
   gsl_matrix_float_sub(squared_error, modelMat);
   gsl_matrix_float_mul_elements(squared_error, squared_error);
   for (int i = 0; i < squared_error->size1; ++i)
      for (int j = 0; j < squared_error->size2; ++j)
         reconstruction_cost+=gsl_matrix_float_get(squared_error, i, j);
   reconstruction_cost /= squared_error->size2;
   gsl_matrix_float_free(squared_error);
   return reconstruction_cost;
}