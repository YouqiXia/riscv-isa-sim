#ifndef _RISCV_DEVICES_EXTENSION_H
#define _RISCV_DEVICES_EXTENSION_H

#include <queue>
#include <stdint.h>

#include "devices.h"

class uart_z1_t : public abstract_device_t {
public:
  uart_z1_t();
  bool load(reg_t addr, size_t len, uint8_t* bytes);
  bool store(reg_t addr, size_t len, const uint8_t* bytes);
  void tick(void);
  size_t size() const;

private:
  std::queue<uint8_t> rx_queue;
  uint32_t reg_status;
};

#endif