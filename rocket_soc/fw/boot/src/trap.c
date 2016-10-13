/******************************************************************************
 * @file
 * @copyright Copyright 2015 GNSS Sensor Ltd. All right reserved.
 * @author    Sergey Khabarov - sergeykhbr@gmail.com
 * @brief     General interrupt handler called from assembler.
******************************************************************************/

#include <string.h>
#include "axi_maps.h"
#include "encoding.h"

typedef union csr_mcause_type {
    struct bits_type {
        uint64_t code   : 63;   // 11 - Machine external interrupt
        uint64_t irq    : 1;
    } bits;
    uint64_t value;
} csr_mcause_type;

long handle_trap(long cause, long epc, long long regs[32]) {
    /**
     * Pending interrupt bit is cleared in the crt.S file by calling:
     *      csrc mip, MIP_MSIP
     * If we woudn't do it the interrupt handler will be called infinitly
     *
     * Rise interrupt from the software maybe done sending a self-IPI:
     *      csrwi mipi, 0
     */
    irqctrl_map *p_irqctrl = (irqctrl_map *)ADDR_NASTI_SLAVE_IRQCTRL;
    IsrEntryType *isr_table = (IsrEntryType *)p_irqctrl->isr_table;
    IRQ_HANDLER irq_handler;
    uint32_t pending;
    csr_mcause_type mcause;

    mcause.value = cause;
    p_irqctrl->dbg_cause = cause;
    p_irqctrl->dbg_epc = epc;

    p_irqctrl->irq_lock = 1;
    pending = p_irqctrl->irq_pending;
    p_irqctrl->irq_clear = pending;
    p_irqctrl->irq_lock = 0;

    if (mcause.bits.irq == 0x1 && mcause.bits.code == 11) {
        for (int i = 0; i < CFG_IRQ_TOTAL; i++) {
            if (pending & (0x1 << i)) {
                irq_handler = (IRQ_HANDLER)isr_table[i].handler;
                p_irqctrl->irq_cause_idx = i;

                irq_handler((void *)isr_table[i].arg);
            }
        }
    } else {
       /// Exception trap
       ((gpio_map *)ADDR_NASTI_SLAVE_GPIO)->led = 0xF0 ^ (uint32_t)cause;
       while (1) {}
    }

    return epc;
}
