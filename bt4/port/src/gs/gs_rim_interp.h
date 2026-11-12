#ifndef GS_RIM_INTERP_H
#define GS_RIM_INTERP_H
#include <string.h>
/* Rim uniforms are appended immediately after their shading pass. Reuse its
   exact interpolated transforms and light direction, rather than moving the
   overlaid triangles along a different matrix path and fighting their depth. */
static unsigned GsRim_Align(Vu0Uniform *blend,const Vu0Uniform *real,
 const uint16_t *kind,const uint32_t *object,const uint32_t *view,uint32_t count){
 uint32_t i;unsigned aligned=0;
 for(i=1;i<count;i++){
  if(kind[i]!=70||kind[i-1]!=65||object[i]!=object[i-1]||view[i]!=view[i-1])continue;
  if(memcmp(real[i].boneA,real[i-1].boneA,60*sizeof(float)))continue;
  memcpy(blend[i].boneA,blend[i-1].boneA,60*sizeof(float));++aligned;
 }
 return aligned;
}
#endif
