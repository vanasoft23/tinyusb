/*
 * AM1802/AM1808 glue for the TinyUSB Mentor MUSB device controller.
 */

#ifndef TUSB_MUSB_AM1802_H_
#define TUSB_MUSB_AM1802_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "csl_interrupt.h"
#include "hw_types.h"
#include "soc_AM1808.h"

#include "hw_usbOtg_AM1808.h"
#include "hw_usbphyGS60.h"

#ifndef SystemCoreClock
#define SystemCoreClock 300000000u
#endif

#ifndef __NOP
#define __NOP() __asm volatile("nop")
#endif

#define MUSB_CFG_SHARED_FIFO 0
#define MUSB_CFG_DYNAMIC_FIFO 1
#define MUSB_CFG_DYNAMIC_FIFO_SIZE 4096
#define MUSB_DCD_PHY_INIT_RETURNS_BOOL 1
#define MUSB_DCD_INT_HANDLER_SNAPSHOT 1
#ifndef TUP_DCD_ENDPOINT_MAX
#define TUP_DCD_ENDPOINT_MAX 4
#endif

#ifndef MUSB_AM1802_RESET_WAIT_CYCLES
#define MUSB_AM1802_RESET_WAIT_CYCLES 3000000u
#endif

#ifndef CFGCHIP2_USB0PHYCLKMUX
#define CFGCHIP2_USB0PHYCLKMUX CFGCHIP2_USB2PHYCLKMUX
#endif

#ifndef MUSB_AM1802_PHY_WAIT_CYCLES
#define MUSB_AM1802_PHY_WAIT_CYCLES 3000000u
#endif

#ifndef MUSB_AM1802_PHY_RESET_CYCLES
#define MUSB_AM1802_PHY_RESET_CYCLES 50u
#endif

#ifndef MUSB_AM1802_USB0_REFFREQ
#define MUSB_AM1802_USB0_REFFREQ CFGCHIP2_REFFREQ_24MHZ
#endif

#ifndef MUSB_AM1802_USB0_PHYCLKMUX
#define MUSB_AM1802_USB0_PHYCLKMUX CFGCHIP2_USB0PHYCLKMUX
#endif

#ifndef MUSB_AM1802_PHYCLKMUX_FALLBACK
#define MUSB_AM1802_PHYCLKMUX_FALLBACK 0
#endif

#define MUSB_AM1802_CTRL_UINT 0x00000008u
#define MUSB_AM1802_CTRL_RNDIS 0x00000010u
#define MUSB_AM1802_INTR_TX_MASK 0x0000001fu
#define MUSB_AM1802_INTR_RX_MASK 0x00001e00u
#define MUSB_AM1802_INTR_RX_SHIFT 8u
#define MUSB_AM1802_INTR_USB_MASK 0x00ff0000u // was 0x01ff0000u  "prefer 0x00ff0000 unless you intentionally handle USBDRVVBUS separately."
#define MUSB_AM1802_INTR_USB_SHIFT 16u
#define MUSB_AM1802_INTR_MASK 0x01ff1e1fu
#define MUSB_DCD_INT_STATUS_PLATFORM 1

static const uintptr_t MUSB_BASES[] = {SOC_USB_0_BASE};

#if CFG_TUD_ENABLED

static volatile unsigned musb_am1802_irq_enabled;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_ctrl_before;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_ctrl_reset;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_ctrl_configured;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_ctrl_last;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_wait_count;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_attempt_count;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_selected_mux;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_fallback_used;
static TU_ATTR_USED volatile uint32_t musb_am1802_phy_timeout;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_count;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_bridge_src;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_bridge_masked;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_intr_usb;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_intr_tx;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_intr_rx;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_intr_usben;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_intr_txen;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_intr_rxen;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_power;
static TU_ATTR_USED volatile uint32_t musb_am1802_irq_devctl;

static inline void musb_am1802_otg_ack(uint32_t mask) {
    HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC_CLEAR) = mask;
    HWREG(SOC_USB_0_OTG_BASE + USB_0_END_OF_INTR) = 0u;
}

static inline void musb_am1802_otg_int_enable(void) {
    uint32_t ctrl = HWREG(SOC_USB_0_OTG_BASE + USB_0_CTRL);

    HWREG(SOC_USB_0_OTG_BASE + USB_0_CTRL) =
        ctrl & ~(MUSB_AM1802_CTRL_UINT | MUSB_AM1802_CTRL_RNDIS);
    HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_MASK_SET) =
        MUSB_AM1802_INTR_MASK;
}

static inline void musb_am1802_otg_int_disable(void) {
    HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_MASK_CLEAR) =
        MUSB_AM1802_INTR_MASK;
}

static inline bool musb_am1802_phy_try_source(uint32_t reg,
                                              uint32_t phy_clk_mux,
                                              uint32_t attempt) {
    HWREG(CFGCHIP2_USBPHYCTRL) = reg | CFGCHIP2_RESET;
    musb_am1802_phy_ctrl_reset = HWREG(CFGCHIP2_USBPHYCTRL);

    for (uint32_t wait = 0; wait < MUSB_AM1802_PHY_RESET_CYCLES; wait++) {
        __NOP();
    }

    reg &= ~(CFGCHIP2_RESET | CFGCHIP2_PHYPWRDN | CFGCHIP2_OTGPWRDN |
             CFGCHIP2_OTGMODE | CFGCHIP2_REFFREQ | CFGCHIP2_USB0PHYCLKMUX);
    reg |= CFGCHIP2_FORCE_DEVICE | CFGCHIP2_DATPOL | CFGCHIP2_SESENDEN |
           CFGCHIP2_VBDTCTEN | CFGCHIP2_PHY_PLLON |
           MUSB_AM1802_USB0_REFFREQ | phy_clk_mux;

    musb_am1802_phy_ctrl_configured = reg;
    HWREG(CFGCHIP2_USBPHYCTRL) = reg;

    for (uint32_t wait = 0; wait < MUSB_AM1802_PHY_WAIT_CYCLES; wait++) {
        uint32_t current = HWREG(CFGCHIP2_USBPHYCTRL);

        musb_am1802_phy_ctrl_last = current;
        musb_am1802_phy_wait_count = wait + 1u;
        musb_am1802_phy_attempt_count = attempt;

        if (current & CFGCHIP2_PHYCLKGD) {
            musb_am1802_phy_selected_mux =
                current & CFGCHIP2_USB0PHYCLKMUX;
            musb_am1802_otg_int_disable();
            musb_am1802_otg_ack(MUSB_AM1802_INTR_MASK);
            return true;
        }
        __NOP();
    }

    return false;
}

static inline bool musb_dcd_phy_init(uint8_t rhport) {
    (void)rhport;

    // Reset USB controller
    HWREG(SOC_USB_0_OTG_BASE + USB_0_CTRL) |= 1;
    while (HWREG(SOC_USB_0_OTG_BASE + USB_0_CTRL));

    // Phy reset
    uint32_t reg = HWREG(CFGCHIP2_USBPHYCTRL);

    musb_am1802_phy_ctrl_before = reg;
    musb_am1802_phy_ctrl_last = reg;
    musb_am1802_phy_wait_count = 0u;
    musb_am1802_phy_attempt_count = 0u;
    musb_am1802_phy_selected_mux = 0xffffffffu;
    musb_am1802_phy_fallback_used = 0u;
    musb_am1802_phy_timeout = 0u;

    if (musb_am1802_phy_try_source(
            reg, MUSB_AM1802_USB0_PHYCLKMUX & CFGCHIP2_USB0PHYCLKMUX, 1u)) {
        return true;
    }

#if MUSB_AM1802_PHYCLKMUX_FALLBACK
    musb_am1802_phy_fallback_used = 1u;

    if (musb_am1802_phy_try_source(
            reg, (~MUSB_AM1802_USB0_PHYCLKMUX) & CFGCHIP2_USB0PHYCLKMUX,
            2u)) {
        return true;
    }
#endif

    musb_am1802_phy_timeout = 1u;

    return false;
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_enable(uint8_t rhport) {
    (void)rhport;

    musb_am1802_irq_enabled = 1u;
    musb_am1802_otg_int_enable();
    IntSystemEnable(SYS_INT_USB0);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_disable(uint8_t rhport) {
    (void)rhport;

    musb_am1802_irq_enabled = 0u;
    IntSystemDisable(SYS_INT_USB0);
    musb_am1802_otg_int_disable();
}

TU_ATTR_ALWAYS_INLINE static inline unsigned
musb_dcd_get_int_enable(uint8_t rhport) {
    (void)rhport;

    return musb_am1802_irq_enabled;
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_clear(uint8_t rhport) {
    (void)rhport;

    IntSystemStatusClear(SYS_INT_USB0);
    musb_am1802_otg_ack(MUSB_AM1802_INTR_MASK);
}

static inline void musb_dcd_int_handler_enter(uint8_t rhport) {
    musb_regs_t *musb_regs = MUSB_REGS(rhport);

    musb_am1802_irq_count++;
    musb_am1802_irq_bridge_src =
        HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC);
    musb_am1802_irq_bridge_masked =
        HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC_MASKED);
    musb_am1802_irq_intr_usben = musb_regs->intr_usben;
    musb_am1802_irq_intr_txen = musb_regs->intr_txen;
    musb_am1802_irq_intr_rxen = musb_regs->intr_rxen;
    musb_am1802_irq_power = musb_regs->power;
    musb_am1802_irq_devctl = musb_regs->devctl;

#if !NESTED_INTERRUPTS
    IntSystemStatusClear(SYS_INT_USB0);
#endif

    musb_am1802_otg_ack(musb_am1802_irq_bridge_masked &
                        MUSB_AM1802_INTR_MASK);
}

static inline void musb_dcd_int_status_get(uint8_t rhport,
                                           uint_fast8_t *intr_usb,
                                           uint_fast8_t *intr_tx,
                                           uint_fast8_t *intr_rx) {
    (void)rhport;

    uint32_t status = musb_am1802_irq_bridge_masked;

    *intr_usb = (uint_fast8_t)((status & MUSB_AM1802_INTR_USB_MASK) >>
                               MUSB_AM1802_INTR_USB_SHIFT);
    *intr_tx = (uint_fast8_t)(status & MUSB_AM1802_INTR_TX_MASK);
    *intr_rx = (uint_fast8_t)((status & MUSB_AM1802_INTR_RX_MASK) >>
                              MUSB_AM1802_INTR_RX_SHIFT);
}

static inline void musb_dcd_int_handler_snapshot(uint8_t rhport,
                                                 uint32_t intr_usb,
                                                 uint32_t intr_tx,
                                                 uint32_t intr_rx) {
    (void)rhport;

    musb_am1802_irq_intr_usb = intr_usb;
    musb_am1802_irq_intr_tx = intr_tx;
    musb_am1802_irq_intr_rx = intr_rx;
}

#endif // CFG_TUD_ENABLED

#ifdef __cplusplus
}
#endif

#endif // TUSB_MUSB_AM1802_H_
