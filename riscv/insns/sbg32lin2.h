#include "xkgost_common.h"
require_extension(EXT_XKGOST);
require_rv32;
WRITE_RD(sext32(xkgost_stbg(zext32(RS1) | zext32(RS2) << 32) >> 32));
