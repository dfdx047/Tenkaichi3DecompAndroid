/* Pass this file to the linker with the `-T linker_script_extra.ld` flag */
ENTRY(_start);

/* Menu overlay (DBZP.BIN, loaded at 0x334C00) symbols that C files of the main executable refer to
 * (src/sys/late_a*.c: gMenuArc12, ItemHelp_*) are defined by build/SLUS_216.78_extern_syms.ld, which configure.py
 * generates from the listed symbol files (every listed symbol outside this image). */
