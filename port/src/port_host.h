/* PORT_HOST: a variable of a port file that is NOT part of the game's state although the file's other variables
   are (port/tools/make_state.py lists those for saving and restoring): handles of the operating system (open
   files) and the like. Restoring an earlier state must not bring an old handle back. Such variables go into a
   section of their own, which the list leaves out. */
#ifndef PORT_HOST_H
#define PORT_HOST_H
#define PORT_HOST __attribute__((section(".porthst")))
#endif
