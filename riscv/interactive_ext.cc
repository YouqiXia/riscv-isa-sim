#include "sim.h"

#include "events.h"

#include "sim_event_ctrl.h"

void sim_t::interactive_mtint(const std::string& cmd, const std::vector<std::string>& args) {
  mtint_t int_arg;
  int_arg.core_id = strtoul(args[0].c_str(), NULL, 10);
  int_arg.is_timer = strtoul(args[1].c_str(), NULL, 10);
  int_arg.clr_ip = strtoul(args[2].c_str(), NULL, 10);
  int_arg.int_grnt = strtoul(args[3].c_str(), NULL, 10);
  int_arg.timestamp = strtoul(args[4].c_str(),NULL,10);

  sim_event_ctrl_t(*this).set_tint(int_arg);
}

void sim_t::interactive_mccosim_init(const std::string& cmd, const std::vector<std::string>& args) {
  memunit_init_arg_t mem_init_arg; 

  mem_init_arg.rob_num = strtoul(args[0].c_str(), NULL, 10);
  mem_init_arg.stb_num = strtoul(args[1].c_str(), NULL, 10);
  mem_init_arg.stb_data_byte = strtoul(args[2].c_str(), NULL, 10);

  memunit_init_arg = mem_init_arg;
}

void sim_t::interactive_mccosim_initcore(const std::string& cmd, const std::vector<std::string>& args) {
  auto core_id = strtoul(args[0].c_str(), NULL, 10);
  sim_event_ctrl_t(*this).init_core_memunit(core_id);
}

void sim_t::interactive_mccosim_event(const std::string& cmd, const std::vector<std::string>& args) {
  mem_event_t mcc_event_arg(mem_event_t::NONE, mem_event_t::INVALID, false); 

  mcc_event_arg.core_id = strtoul(args[0].c_str(), NULL, 10);
  mcc_event_arg.timestamp = strtoul(args[1].c_str(),NULL,10);
  mcc_event_arg.set_state(strtoul(args[2].c_str(),NULL,16));
  mcc_event_arg.set_insn_flag(strtoul(args[3].c_str(),NULL,16));
  mcc_event_arg.paddr = strtoul(args[4].c_str(),NULL,16);
  mcc_event_arg.data = strtoul(args[5].c_str(),NULL,16);
  mcc_event_arg.len = strtoul(args[6].c_str(),NULL,10);
  mcc_event_arg.rob_idx = strtoul(args[7].c_str(),NULL,10);
  mcc_event_arg.union_arg.stb_idx = strtoul(args[8].c_str(),NULL,16);

  sim_event_ctrl_t(*this).mem_event_step(mcc_event_arg);
}

void sim_t::interactive_mccosim_gbl(const std::string& cmd, const std::vector<std::string>& args) {
  sim_event_ctrl_t(*this).do_tick();
}