/* utils.h*/

#ifndef UTILS
#define UTILS

/*****************************************************************************/

#include "ladspa.h"

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

extern effectDescriptor effectDescriptorArray;
extern int idx;

#endif

/* EOF */
