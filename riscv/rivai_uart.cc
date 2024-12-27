#include "devices_extension.h"

#include <sys/time.h>
#include <stdexcept>
#include <iostream>

#include "platform.h"
#include "term.h"
#include "dts_extension.h"
#include "tools_module.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// UART Z1 相关代码，资料链接 https://wkr4l3xyxk.feishu.cn/docx/R15rdzqACo2oaixdrjjcI5nYn0f
//////////////////////////////////////////////////////////////////////////////////////////////

// data寄存器。写入该寄存器发送数据。读取该寄存器接收数据
#define UART_DATA_REG 0x0

// state寄存器。用来表示发送和接收的状态
#define UART_STATE_REG         0x4
#define UART_STATE_TX_BUF_FULL 0x01 // 发送BUF是否满了，只有不满的时候才能写入data寄存器发送数据
#define UART_STATE_RX_BUF_FULL 0x02 // 接收BUF是否满了，只有满了的时候才能从data寄存器读取出接收的数据

// 初始化
uart_z1_t::uart_z1_t() : reg_status(0) {}

// 读取寄存器的值
// 入参 addr  是寄存器的偏移地址
// 入参 len   是寄存器的长度
// 出参 bytes 用来存放读取到的寄存器的值
bool uart_z1_t::load(reg_t addr, size_t len, uint8_t* bytes) {
    // 检查地址范围
    if ((addr + len) > UART_Z1_SIZE)
        return false;

    // 读取data寄存器
    if ((addr == UART_DATA_REG) && (len == 4)) {
        uint8_t ch;
        if (rx_queue.empty()) {
            // 如果没有收到任何数据，清空 UART_STATE_RX_BUF_FULL
            ch = 0;
            reg_status &= ~UART_STATE_RX_BUF_FULL;
        } else {
            // 如果当前有数据，读出1个字节的数据，如果剩余0个数据则清空 UART_STATE_RX_BUF_FULL
            ch = rx_queue.front();
            rx_queue.pop();
            if (rx_queue.empty()) {
                reg_status &= ~UART_STATE_RX_BUF_FULL;
            }
        }

        // 将寄存的值写入出参
        bytes[0] = ch;
        bytes[1] = 0;
        bytes[2] = 0;
        bytes[3] = 0;
    }

    // 读取state寄存器
    else if ((addr == UART_STATE_REG) && (len == 4)) {
        bytes[0] = reg_status;
        bytes[1] = 0;
        bytes[2] = 0;
        bytes[3] = 0;
    }

    // 暂时不支持其他寄存的读取
    return true;
}

// 向寄存器写入数据
// 入参 addr  是寄存器的偏移地址
// 入参 len   是寄存器的长度
// 入参 bytes 写入寄存器的值
bool uart_z1_t::store(reg_t addr, size_t len, const uint8_t* bytes) {
    // 检查地址范围
    if ((addr + len) > UART_Z1_SIZE)
        return false;

    // 写入data寄存器，发送数据到控制台
    if ((addr == UART_DATA_REG) && (len == 4)) {
        canonical_terminal_t::write(bytes[0]);
        roi_match->check(bytes[0]);
    }

    // 暂时不支持其他寄存的写入
    return true;
}

// spike 主逻辑会循环调用该函数，以实现将控制台的输入存放到uart的队列中
void uart_z1_t::tick(void) {
    int rc = canonical_terminal_t::read();
    if (rc < 0) {
        return;
    }

    rx_queue.push((uint8_t)rc);
    reg_status |= UART_STATE_RX_BUF_FULL;
}

size_t uart_z1_t::size() const { return UART_Z1_SIZE; }

std::string uart_z1_generate_dts(const sim_t* sim, const std::vector<std::string>& UNUSED sargs) {
  std::cout << "Warnning! Not defined uart_z1_generate_dts" << std::endl;
  return std::string();
}

uart_z1_t* uart_z1_parse_from_fdt(const void* fdt, const sim_t* sim, reg_t* base, const std::vector<std::string>& UNUSED sargs) {
  if (fdt_parse_rivai_uart_z1(fdt, base, "z1,uart0") == 0) {
    return new uart_z1_t();
  } else {
    return nullptr;
  }
}

REGISTER_DEVICE(uart_z1, uart_z1_parse_from_fdt, uart_z1_generate_dts)
