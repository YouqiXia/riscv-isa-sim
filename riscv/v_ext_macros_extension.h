#ifndef _RISCV_V_EXT_MACROS_EXTENSION_H
#define _RISCV_V_EXT_MACROS_EXTENSION_H

#include "v_ext_macros.h"

#include <limits>

#define MAX(x, y) (x >= y ? x : y)

#define V_FILL_ONE (vd |= std::numeric_limits<uint64_t>::max());

#define VF_FILL_ONE \
  switch (sizeof(vd)) { \
    case 8: \
    *(uint8_t*)&vd |= std::numeric_limits<uint64_t>::max(); \
    break; \
    case 16: \
    *(uint16_t*)&vd |= std::numeric_limits<uint64_t>::max(); \
    break; \
    case 32: \
    *(uint32_t*)&vd |= std::numeric_limits<uint64_t>::max(); \
    break; \
    case 64: \
    default: \
    *(uint64_t*)&vd |= std::numeric_limits<uint64_t>::max(); \
    break; \
  }

#define V_HANDLE_MASK(BODY) \
  if (sew == e8) { \
    BODY(e8); \
    V_FILL_ONE \
  } else if (sew == e16) { \
    BODY(e16); \
    V_FILL_ONE \
  } else if (sew == e32) { \
    BODY(e32); \
    V_FILL_ONE \
  } else if (sew == e64) { \
    BODY(e64); \
    V_FILL_ONE \
  }

#define V_HANDLE_MASK_NARROW(BODY) \
  if (sew == e8) { \
    BODY(e8, e16); \
    V_FILL_ONE \
  } else if (sew == e16) { \
    BODY(e16, e32); \
    V_FILL_ONE \
  } else if (sew == e32) { \
    BODY(e32, e64); \
    V_FILL_ONE \
  }

#define V_HANDLE_MASK_WIDEN(BODY) \
  if (sew == e8) { \
    BODY(e8); \
    V_FILL_ONE \
  } else if (sew == e16) { \
    BODY(e16); \
    V_FILL_ONE \
  } else if (sew == e32) { \
    BODY(e32); \
    V_FILL_ONE \
  }

#define V_HANDLE_MASK_VF_MERGE(BODY) \
  if (P.VU.vsew == e16) { \
    BODY(16); \
    VF_FILL_ONE \
  } else if (P.VU.vsew == e32) { \
    BODY(32); \
    VF_FILL_ONE \
  } else if (P.VU.vsew == e64) { \
    BODY(64); \
    VF_FILL_ONE \
  }

#define SE_VI_LOOP_ELEMENT_SKIP(BODY1, BODY2) \
  VI_MASK_VARS \
  if (insn.v_vm() == 0) { \
    BODY1; \
    bool skip = ((P.VU.elt<uint64_t>(0, midx) >> mpos) & 0x1) == 0; \
    if (skip) { \
        BODY2; \
        continue; \
    } \
  }

#define SE_VI_LOOP_BASE(BODY1, BODY2) \
  VI_GENERAL_LOOP_BASE \
  SE_VI_LOOP_ELEMENT_SKIP({}, BODY1(BODY2));

#define SE_VI_LOOP_NSHIFT_BASE(BODY1, BODY2) \
  VI_GENERAL_LOOP_BASE; \
  SE_VI_LOOP_ELEMENT_SKIP({ \
    require(!(insn.rd() == 0 && P.VU.vflmul > 1)); \
  }, BODY1(BODY2));

#define SE_VI_LOOP_END \
  VI_LOOP_END_BASE

#define V_HANDLE_TAIL(BODY1, BODY2) \
  if (g_easy_args.vmaskone) { \
    for (reg_t i = vl; i < MAX(P.VU.vlmax, P.VU.VLEN / P.VU.ELEN); ++i) { \
      BODY1(BODY2) \
    } \
  } \
  P.VU.vstart->write(0);

#define V_HANDLE_TAIL_SPECIAL(BODY1, BODY2) \
  if (g_easy_args.vmaskone) { \
    auto rd_num = insn.rd(); \
    auto sew = P.VU.vsew; \
    for (reg_t i = vl; i < MAX(P.VU.vlmax, P.VU.VLEN / P.VU.ELEN); ++i) { \
      BODY1(BODY2) \
    } \
  }

// genearl VXI signed/unsigned loop
#ifdef VI_VV_ULOOP
#undef VI_VV_ULOOP
#define VI_VV_ULOOP(BODY) \
  VI_CHECK_SSS(true) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VV_U_PARAMS) \
  if (sew == e8) { \
    VV_U_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VV_U_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VV_U_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VV_U_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VV_U_PARAMS)
#endif

#ifdef VI_VV_LOOP
#undef VI_VV_LOOP
#define VI_VV_LOOP(BODY) \
  VI_CHECK_SSS(true) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VV_PARAMS) \
  if (sew == e8) { \
    VV_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VV_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VV_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VV_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VV_PARAMS)
#endif

#ifdef VI_V_ULOOP
#undef VI_V_ULOOP
#define VI_V_ULOOP(BODY) \
  VI_CHECK_SSS(false) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, V_U_PARAMS) \
  if (sew == e8) { \
    V_U_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    V_U_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    V_U_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    V_U_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, V_U_PARAMS)
#endif

#ifdef VI_VX_ULOOP
#undef VI_VX_ULOOP
#define VI_VX_ULOOP(BODY) \
  VI_CHECK_SSS(false) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VX_U_PARAMS) \
  if (sew == e8) { \
    VX_U_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VX_U_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VX_U_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VX_U_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VX_U_PARAMS)
#endif

#ifdef VI_VX_LOOP
#undef VI_VX_LOOP
#define VI_VX_LOOP(BODY) \
  VI_CHECK_SSS(false) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VX_PARAMS) \
  if (sew == e8) { \
    VX_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VX_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VX_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VX_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VX_PARAMS)
#endif

#ifdef VI_VI_ULOOP
#undef VI_VI_ULOOP
#define VI_VI_ULOOP(BODY) \
  VI_CHECK_SSS(false) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VI_U_PARAMS) \
  if (sew == e8) { \
    VI_U_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VI_U_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VI_U_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VI_U_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VI_U_PARAMS)
#endif

#ifdef VI_VI_LOOP
#undef VI_VI_LOOP
#define VI_VI_LOOP(BODY) \
  VI_CHECK_SSS(false) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VI_PARAMS) \
  if (sew == e8) { \
    VI_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VI_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VI_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VI_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VI_PARAMS)
#endif

// signed unsigned operation loop (e.g. mulhsu)
#ifdef VI_VV_SU_LOOP
#undef VI_VV_SU_LOOP
#define VI_VV_SU_LOOP(BODY) \
  VI_CHECK_SSS(true) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VV_SU_PARAMS) \
  if (sew == e8) { \
    VV_SU_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VV_SU_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VV_SU_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VV_SU_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VV_SU_PARAMS)
#endif

#ifdef VI_VX_SU_LOOP
#undef VI_VX_SU_LOOP
#define VI_VX_SU_LOOP(BODY) \
  VI_CHECK_SSS(false) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK, VX_SU_PARAMS) \
  if (sew == e8) { \
    VX_SU_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VX_SU_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VX_SU_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VX_SU_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VX_SU_PARAMS)
#endif

// narrow operation loop
#ifdef VI_VV_LOOP_NARROW
#undef VI_VV_LOOP_NARROW
#define VI_VV_LOOP_NARROW(BODY) \
  VI_CHECK_SDS(true); \
  SE_VI_LOOP_BASE(V_HANDLE_MASK_NARROW, VV_NARROW_PARAMS) \
  if (sew == e8) { \
    VV_NARROW_PARAMS(e8, e16) \
    BODY; \
  } else if (sew == e16) { \
    VV_NARROW_PARAMS(e16, e32) \
    BODY; \
  } else if (sew == e32) { \
    VV_NARROW_PARAMS(e32, e64) \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_NARROW, VV_NARROW_PARAMS)
#endif

#ifdef VI_VX_LOOP_NARROW
#undef VI_VX_LOOP_NARROW
#define VI_VX_LOOP_NARROW(BODY) \
  VI_CHECK_SDS(false); \
  SE_VI_LOOP_BASE(V_HANDLE_MASK_NARROW, VX_NARROW_PARAMS) \
  if (sew == e8) { \
    VX_NARROW_PARAMS(e8, e16) \
    BODY; \
  } else if (sew == e16) { \
    VX_NARROW_PARAMS(e16, e32) \
    BODY; \
  } else if (sew == e32) { \
    VX_NARROW_PARAMS(e32, e64) \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_NARROW, VX_NARROW_PARAMS)
#endif

#ifdef VI_VI_LOOP_NARROW
#undef VI_VI_LOOP_NARROW
#define VI_VI_LOOP_NARROW(BODY) \
  VI_CHECK_SDS(false); \
  SE_VI_LOOP_BASE(V_HANDLE_MASK_NARROW, VI_NARROW_PARAMS) \
  if (sew == e8) { \
    VI_NARROW_PARAMS(e8, e16) \
    BODY; \
  } else if (sew == e16) { \
    VI_NARROW_PARAMS(e16, e32) \
    BODY; \
  } else if (sew == e32) { \
    VI_NARROW_PARAMS(e32, e64) \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_NARROW, VI_NARROW_PARAMS)
#endif

#ifdef VI_VI_LOOP_NSHIFT
#undef VI_VI_LOOP_NSHIFT
#define VI_VI_LOOP_NSHIFT(BODY) \
  VI_CHECK_SDS(false); \
  SE_VI_LOOP_NSHIFT_BASE(V_HANDLE_MASK_NARROW, VI_NARROW_PARAMS) \
  if (sew == e8) { \
    VI_NARROW_PARAMS(e8, e16) \
    BODY; \
  } else if (sew == e16) { \
    VI_NARROW_PARAMS(e16, e32) \
    BODY; \
  } else if (sew == e32) { \
    VI_NARROW_PARAMS(e32, e64) \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_NARROW, VI_NARROW_PARAMS)
#endif

#ifdef VI_VX_LOOP_NSHIFT
#undef VI_VX_LOOP_NSHIFT
#define VI_VX_LOOP_NSHIFT(BODY) \
  VI_CHECK_SDS(false); \
  SE_VI_LOOP_NSHIFT_BASE(V_HANDLE_MASK_NARROW, VX_NARROW_PARAMS) \
  if (sew == e8) { \
    VX_NARROW_PARAMS(e8, e16) \
    BODY; \
  } else if (sew == e16) { \
    VX_NARROW_PARAMS(e16, e32) \
    BODY; \
  } else if (sew == e32) { \
    VX_NARROW_PARAMS(e32, e64) \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_NARROW, VX_NARROW_PARAMS)
#endif

#ifdef VI_VV_LOOP_NSHIFT
#undef VI_VV_LOOP_NSHIFT
#define VI_VV_LOOP_NSHIFT(BODY) \
  VI_CHECK_SDS(true); \
  SE_VI_LOOP_NSHIFT_BASE(V_HANDLE_MASK_NARROW, VV_NARROW_PARAMS) \
  if (sew == e8) { \
    VV_NARROW_PARAMS(e8, e16) \
    BODY; \
  } else if (sew == e16) { \
    VV_NARROW_PARAMS(e16, e32) \
    BODY; \
  } else if (sew == e32) { \
    VV_NARROW_PARAMS(e32, e64) \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_NARROW, VV_NARROW_PARAMS)
#endif

// widen operation loop
#ifdef VI_VV_LOOP_WIDEN
#undef VI_VV_LOOP_WIDEN
#define VI_VV_LOOP_WIDEN(BODY) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK_WIDEN, VV_PARAMS) \
  if (sew == e8) { \
    VV_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VV_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VV_PARAMS(e32); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_WIDEN, VV_PARAMS)
#endif

#ifdef VI_VX_LOOP_WIDEN
#undef VI_VX_LOOP_WIDEN
#define VI_VX_LOOP_WIDEN(BODY) \
  SE_VI_LOOP_BASE(V_HANDLE_MASK_WIDEN, VX_PARAMS) \
  if (sew == e8) { \
    VX_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VX_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VX_PARAMS(e32); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_WIDEN, VX_PARAMS)
#endif

#ifdef VI_VV_MERGE_LOOP
#undef VI_VV_MERGE_LOOP
#define VI_VV_MERGE_LOOP(BODY) \
  VI_CHECK_SSS(true); \
  VI_MERGE_LOOP_BASE \
  if (sew == e8) { \
    VV_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VV_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VV_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VV_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VV_PARAMS)
#endif

#ifdef VI_VX_MERGE_LOOP
#undef VI_VX_MERGE_LOOP
#define VI_VX_MERGE_LOOP(BODY) \
  VI_CHECK_SSS(false); \
  VI_MERGE_LOOP_BASE \
  if (sew == e8) { \
    VX_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VX_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VX_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VX_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VX_PARAMS)
#endif

#ifdef VI_VI_MERGE_LOOP
#undef VI_VI_MERGE_LOOP
#define VI_VI_MERGE_LOOP(BODY) \
  VI_CHECK_SSS(false); \
  VI_MERGE_LOOP_BASE \
  if (sew == e8) { \
    VI_PARAMS(e8); \
    BODY; \
  } else if (sew == e16) { \
    VI_PARAMS(e16); \
    BODY; \
  } else if (sew == e32) { \
    VI_PARAMS(e32); \
    BODY; \
  } else if (sew == e64) { \
    VI_PARAMS(e64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK, VI_PARAMS)
#endif

#ifdef VI_VF_MERGE_LOOP
#undef VI_VF_MERGE_LOOP
#define VI_VF_MERGE_LOOP(BODY) \
  VI_CHECK_SSS(false); \
  VI_VFP_COMMON \
  for (reg_t i = P.VU.vstart->read(); i < vl; ++i) { \
  VI_MERGE_VARS \
  if (P.VU.vsew == e16) { \
    VFP_VF_PARAMS(16); \
    BODY; \
  } else if (P.VU.vsew == e32) { \
    VFP_VF_PARAMS(32); \
    BODY; \
  } else if (P.VU.vsew == e64) { \
    VFP_VF_PARAMS(64); \
    BODY; \
  } \
  SE_VI_LOOP_END \
  V_HANDLE_TAIL(V_HANDLE_MASK_VF_MERGE, VFP_VF_PARAMS)
#endif

#define ELEMENT_SKIP ((insn.v_vm() == 0) && (((P.VU.elt<uint64_t>(0, i / 64) >> (i % 64)) & 0x1) == 0))

#ifdef VI_LD
#undef VI_LD
#define VI_LD(stride, offset, elt_width, is_mask_ldst)                                                                 \
  const reg_t nf = insn.v_nf() + 1;                                                                                    \
  VI_CHECK_LOAD(elt_width, is_mask_ldst);                                                                              \
  const reg_t vl = is_mask_ldst ? ((P.VU.vl->read() + 7) / 8) : P.VU.vl->read();                                       \
  const reg_t baseAddr = RS1;                                                                                          \
  const reg_t vd = insn.rd();                                                                                          \
  for (reg_t i = 0; i < vl; ++i) {                                                                                     \
    P.VU.vstart->write(i);                                                                                             \
    for (reg_t fn = 0; fn < nf; ++fn) {                                                                                \
      elt_width##_t val = MMU.load<elt_width##_t>(baseAddr + (stride) + (offset) * sizeof(elt_width##_t));             \
      VI_ELEMENT_SKIP;                                                                                                 \
      VI_STRIP(i);                                                                                                     \
      P.VU.elt<elt_width##_t>(vd + fn * emul, vreg_inx, true) = val;                                                   \
    }                                                                                                                  \
  }                                                                                                                    \
  P.VU.vstart->write(0);
#endif

#ifdef VI_LD_INDEX
#undef VI_LD_INDEX
#define VI_LD_INDEX(elt_width, is_seg)                                                                                 \
  const reg_t nf = insn.v_nf() + 1;                                                                                    \
  VI_CHECK_LD_INDEX(elt_width);                                                                                        \
  const reg_t vl = P.VU.vl->read();                                                                                    \
  const reg_t baseAddr = RS1;                                                                                          \
  const reg_t vd = insn.rd();                                                                                          \
  if (!is_seg)                                                                                                         \
    require(nf == 1);                                                                                                  \
  VI_DUPLICATE_VREG(insn.rs2(), elt_width);                                                                            \
  for (reg_t i = 0; i < vl; ++i) {                                                                                     \
    P.VU.vstart->write(i);                                                                                             \
    for (reg_t fn = 0; fn < nf; ++fn) {                                                                                \
      if (P.VU.vsew == e8) {                                                                                           \
        auto val = MMU.load<uint8_t>(baseAddr + index[i] + fn * 1);                                                    \
        VI_ELEMENT_SKIP;                                                                                               \
        VI_STRIP(i);                                                                                                   \
        P.VU.elt<uint8_t>(vd + fn * flmul, vreg_inx, true) = val;                                                      \
      }                                                                                                                \
      if (P.VU.vsew == e16) {                                                                                          \
        auto val = MMU.load<uint16_t>(baseAddr + index[i] + fn * 2);                                                   \
        VI_ELEMENT_SKIP;                                                                                               \
        VI_STRIP(i);                                                                                                   \
        P.VU.elt<uint16_t>(vd + fn * flmul, vreg_inx, true) = val;                                                     \
      }                                                                                                                \
      if (P.VU.vsew == e32) {                                                                                          \
        auto val = MMU.load<uint32_t>(baseAddr + index[i] + fn * 4);                                                   \
        VI_ELEMENT_SKIP;                                                                                               \
        VI_STRIP(i);                                                                                                   \
        P.VU.elt<uint32_t>(vd + fn * flmul, vreg_inx, true) = val;                                                     \
      }                                                                                                                \
      if (P.VU.vsew == e64) {                                                                                          \
        auto val = MMU.load<uint64_t>(baseAddr + index[i] + fn * 8);                                                   \
        VI_ELEMENT_SKIP;                                                                                               \
        VI_STRIP(i);                                                                                                   \
        P.VU.elt<uint64_t>(vd + fn * flmul, vreg_inx, true) = val;                                                     \
      }                                                                                                                \
    }                                                                                                                  \
  }                                                                                                                    \
  P.VU.vstart->write(0);
#endif

#ifdef VI_ST
#undef VI_ST
#define VI_ST(stride, offset, elt_width, is_mask_ldst)                                                                 \
  const reg_t nf = insn.v_nf() + 1;                                                                                    \
  VI_CHECK_STORE(elt_width, is_mask_ldst);                                                                             \
  const reg_t vl = is_mask_ldst ? ((P.VU.vl->read() + 7) / 8) : P.VU.vl->read();                                       \
  const reg_t baseAddr = RS1;                                                                                          \
  const reg_t vs3 = insn.rd();                                                                                         \
  for (reg_t i = 0; i < vl; ++i) {                                                                                     \
    VI_STRIP(i)                                                                                                        \
    P.VU.vstart->write(i);                                                                                             \
    for (reg_t fn = 0; fn < nf; ++fn) {                                                                                \
      elt_width##_t val = P.VU.elt<elt_width##_t>(vs3 + fn * emul, vreg_inx);                                          \
      MMU.store<elt_width##_t>(baseAddr + (stride) + (offset) * sizeof(elt_width##_t), val, xlate_flags_t(),           \
                               ELEMENT_SKIP);                                                                          \
    }                                                                                                                  \
  }                                                                                                                    \
  P.VU.vstart->write(0);
#endif

#ifdef VI_ST_INDEX
#undef VI_ST_INDEX
#define VI_ST_INDEX(elt_width, is_seg)                                                                                 \
  const reg_t nf = insn.v_nf() + 1;                                                                                    \
  VI_CHECK_ST_INDEX(elt_width);                                                                                        \
  const reg_t vl = P.VU.vl->read();                                                                                    \
  const reg_t baseAddr = RS1;                                                                                          \
  const reg_t vs3 = insn.rd();                                                                                         \
  if (!is_seg)                                                                                                         \
    require(nf == 1);                                                                                                  \
  VI_DUPLICATE_VREG(insn.rs2(), elt_width);                                                                            \
  for (reg_t i = 0; i < vl; ++i) {                                                                                     \
    VI_STRIP(i)                                                                                                        \
    P.VU.vstart->write(i);                                                                                             \
    for (reg_t fn = 0; fn < nf; ++fn) {                                                                                \
      switch (P.VU.vsew) {                                                                                             \
      case e8:                                                                                                         \
        MMU.store<uint8_t>(baseAddr + index[i] + fn * 1, P.VU.elt<uint8_t>(vs3 + fn * flmul, vreg_inx),                \
                           xlate_flags_t(), ELEMENT_SKIP);                                                             \
        break;                                                                                                         \
      case e16:                                                                                                        \
        MMU.store<uint16_t>(baseAddr + index[i] + fn * 2, P.VU.elt<uint16_t>(vs3 + fn * flmul, vreg_inx),              \
                            xlate_flags_t(), ELEMENT_SKIP);                                                            \
        break;                                                                                                         \
      case e32:                                                                                                        \
        MMU.store<uint32_t>(baseAddr + index[i] + fn * 4, P.VU.elt<uint32_t>(vs3 + fn * flmul, vreg_inx),              \
                            xlate_flags_t(), ELEMENT_SKIP);                                                            \
        break;                                                                                                         \
      default:                                                                                                         \
        MMU.store<uint64_t>(baseAddr + index[i] + fn * 8, P.VU.elt<uint64_t>(vs3 + fn * flmul, vreg_inx),              \
                            xlate_flags_t(), ELEMENT_SKIP);                                                            \
        break;                                                                                                         \
      }                                                                                                                \
    }                                                                                                                  \
  }                                                                                                                    \
  P.VU.vstart->write(0);
#endif

#ifdef VI_LDST_FF
#undef VI_LDST_FF
#define VI_LDST_FF(elt_width)                                                                                          \
  const reg_t nf = insn.v_nf() + 1;                                                                                    \
  VI_CHECK_LOAD(elt_width, false);                                                                                     \
  const reg_t vl = p->VU.vl->read();                                                                                   \
  const reg_t baseAddr = RS1;                                                                                          \
  const reg_t rd_num = insn.rd();                                                                                      \
  bool early_stop = false;                                                                                             \
  for (reg_t i = p->VU.vstart->read(); i < vl; ++i) {                                                                  \
    VI_STRIP(i);                                                                                                       \
    for (reg_t fn = 0; fn < nf; ++fn) {                                                                                \
      uint64_t val;                                                                                                    \
      try {                                                                                                            \
        val = MMU.load<elt_width##_t>(baseAddr + (i * nf + fn) * sizeof(elt_width##_t));                               \
        VI_ELEMENT_SKIP;                                                                                               \
      } catch (trap_t & t) {                                                                                           \
        if (i == 0)                                                                                                    \
          throw; /* Only take exception on zeroth element */                                                           \
        /* Reduce VL if an exception occurs on a later element */                                                      \
        early_stop = true;                                                                                             \
        P.VU.vl->write_raw(i);                                                                                         \
        break;                                                                                                         \
      }                                                                                                                \
      p->VU.elt<elt_width##_t>(rd_num + fn * emul, vreg_inx, true) = val;                                              \
    }                                                                                                                  \
                                                                                                                       \
    if (early_stop) {                                                                                                  \
      break;                                                                                                           \
    }                                                                                                                  \
  }                                                                                                                    \
  p->VU.vstart->write(0);
#endif

#ifdef VI_ST_WHOLE
#undef VI_ST_WHOLE
#define VI_ST_WHOLE                                                                                                    \
  require_vector_novtype(true);                                                                                        \
  const reg_t baseAddr = RS1;                                                                                          \
  const reg_t vs3 = insn.rd();                                                                                         \
  const reg_t len = insn.v_nf() + 1;                                                                                   \
  require_align(vs3, len);                                                                                             \
  const reg_t size = len * P.VU.vlenb;                                                                                 \
                                                                                                                       \
  if (P.VU.vstart->read() < size) {                                                                                    \
    reg_t i = P.VU.vstart->read() / P.VU.vlenb;                                                                        \
    reg_t off = P.VU.vstart->read() % P.VU.vlenb;                                                                      \
    if (off) {                                                                                                         \
      for (reg_t pos = off; pos < P.VU.vlenb; ++pos) {                                                                 \
        auto val = P.VU.elt<uint8_t>(vs3 + i, pos);                                                                    \
        MMU.store<uint8_t>(baseAddr + P.VU.vstart->read(), val, xlate_flags_t(), ELEMENT_SKIP);                        \
        P.VU.vstart->write(P.VU.vstart->read() + 1);                                                                   \
      }                                                                                                                \
      i++;                                                                                                             \
    }                                                                                                                  \
    for (; i < len; ++i) {                                                                                             \
      for (reg_t pos = 0; pos < P.VU.vlenb; ++pos) {                                                                   \
        auto val = P.VU.elt<uint8_t>(vs3 + i, pos);                                                                    \
        MMU.store<uint8_t>(baseAddr + P.VU.vstart->read(), val, xlate_flags_t(), ELEMENT_SKIP);                        \
        P.VU.vstart->write(P.VU.vstart->read() + 1);                                                                   \
      }                                                                                                                \
    }                                                                                                                  \
  }                                                                                                                    \
  P.VU.vstart->write(0);
#endif

#endif // _RISCV_V_EXT_MACROS_EXTENSION_H