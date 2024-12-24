#include "xkgost_common.h"
require_extension(EXT_XKGOST);
require_rv64;
WRITE_RD(gost64_tau1(RS1, RS2));
