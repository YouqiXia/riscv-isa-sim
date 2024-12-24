#include "xkgost_common.h"
require_extension(EXT_XKGOST);
require_rv32;
WRITE_RD(sext32(gost32_tau1(RS1, RS2)));
