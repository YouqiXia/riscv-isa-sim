#include "dts_extension.h"

#include "libfdt.h"

int fdt_parse_rivai_uart_z1(const void* fdt, reg_t* uart_z1_addr, const char* compatible) {
    int nodeoffset, rc;

    nodeoffset = fdt_node_offset_by_compatible(fdt, -1, compatible);
    if (nodeoffset < 0)
        return nodeoffset;

    rc = fdt_get_node_addr_size(fdt, nodeoffset, uart_z1_addr, NULL, "reg");
    if (rc < 0 || !uart_z1_addr)
        return -ENODEV;

    return 0;
}