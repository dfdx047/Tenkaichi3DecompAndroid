#ifndef PORT_CAMERA_H
#define PORT_CAMERA_H

/* Optional camera.txt settings, supplied by the platform port. */
int Port_CameraEnabled(void);
float Port_CameraValue(const char *name, float fallback);

#endif
