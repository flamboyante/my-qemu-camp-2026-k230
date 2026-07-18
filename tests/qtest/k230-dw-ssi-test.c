/*
 * K230 DWC SSI qtest 总入口
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    k230_ssi_register_reg_tests();
    k230_ssi_register_pio_tests();
    k230_ssi_register_flash_tests();
    k230_ssi_register_qspi_tests();
    k230_ssi_register_irq_tests();
    k230_ssi_register_plic_tests();
    k230_ssi_register_xip_tests();

    return g_test_run();
}
