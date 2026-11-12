/* Extra symbols for the menu overlay (DBZP.BIN) link.
 * The compiler's soft-float calls: ee-gcc 2.96 names int -> double and double -> int `litodp` / `dptoli`;
 * config/symbols/lib.txt has the functions under their libgcc names. */
litodp = __floatsidf;
dptoli = __fixdfsi;
