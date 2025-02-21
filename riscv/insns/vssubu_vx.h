// vssubu.vx vd, vs2, rs1
VI_CHECK_SSS(false);
SE_VI_LOOP_BASE(VEC_COMMON, VX_U_PARAMS) /*code ext*/
bool sat = false;

switch (sew) {
case e8: {
  VX_U_PARAMS(e8);
  vd = sat_subu<uint8_t>(vs2, rs1, sat);
  break;
}
case e16: {
  VX_U_PARAMS(e16);
  vd = sat_subu<uint16_t>(vs2, rs1, sat);
  break;
}
case e32: {
  VX_U_PARAMS(e32);
  vd = sat_subu<uint32_t>(vs2, rs1, sat);
  break;
}
default: {
  VX_U_PARAMS(e64);
  vd = sat_subu<uint64_t>(vs2, rs1, sat);
  break;
}
}
P_SET_OV(sat);
SE_VI_LOOP_END /*code ext*/
V_HANDLE_TAIL(VEC_COMMON, VX_U_PARAMS) /*code ext*/
