#ifndef PORT_RENDER_MARKERS_H
#define PORT_RENDER_MARKERS_H
/* Private metadata above the GS PRIM hardware bits. It travels in each sorted
   triangle packet, following actual boundary geometry rather than a texture. */
#define PORT_PRIM_BOUNDARY (1ULL << 32)
#define PORT_PRIM_PORTRAIT_LEFT (1ULL << 33)
#define PORT_PRIM_PORTRAIT_RIGHT (1ULL << 34)
#define PORT_PRIM_METADATA (PORT_PRIM_BOUNDARY | PORT_PRIM_PORTRAIT_LEFT | PORT_PRIM_PORTRAIT_RIGHT)
void Port_BoundaryBegin(void);
void Port_BoundaryEnd(void);
int Port_BoundaryDrawing(void);
#endif
