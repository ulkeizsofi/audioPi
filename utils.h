/* utils.h*/

#ifndef UTILS
#define UTILS

/*****************************************************************************/

#include "ladspa.h"
#include <pthread.h>    

/*****************************************************************************/

typedef struct limits
{
   float min;
   float max;
}limits;

typedef struct effect_descriptor
{
  char names[10][100];
  int args[10];
  limits lims[10][5];
}effect_descriptor;


typedef struct effect_entry
{
    int idx;
    float args[10];
}effect_entry;

extern effect_descriptor effect_descriptor_array;
extern int idx;

extern int fd[2];


#endif

/* EOF */
