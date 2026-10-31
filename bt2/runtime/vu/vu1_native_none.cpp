// A build without native translations of VU1 microprograms: every program is interpreted.
// tools/vu1/translate_programs.py generates the file that takes this one's place.
#include "vu1.h"

std::span<const Vu1::NativeEntry> Vu1::nativePrograms() {return {};}
