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

typedef struct effectDescriptor
{
  char names[10][100];
  int args[10];
  limits lims[10][5];
}effectDescriptor;


typedef struct effectEntry
{
    int idx;
    float args[10];
}effectEntry;

extern effectDescriptor effectDescriptorArray;
extern int idx;

extern int fd[2];


#endif

/* EOF */
