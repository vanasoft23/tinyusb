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
#define MUSB_AM1802_INTR_USB_MASK 0x00ff0000u
#define MUSB_AM1802_INTR_USB_SHIFT 16u
#define MUSB_AM1802_INTR_MASK 0x01ff1e1fu
#define MUSB_DCD_INT_STATUS_PLATFORM 1

static const uintptr_t MUSB_BASES[] = {SOC_USB_0_BASE};

#if CFG_TUD_ENABLED

static volatile unsigned s_irq_enabled;
static uint32_t s_irq_status;

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
                                               uint32_t phy_clk_mux) {
    HWREG(CFGCHIP2_USBPHYCTRL) = reg | CFGCHIP2_RESET;

    for (uint32_t wait = 0; wait < MUSB_AM1802_PHY_RESET_CYCLES; wait++) {
        __NOP();
    }

    reg &= ~(CFGCHIP2_RESET | CFGCHIP2_PHYPWRDN | CFGCHIP2_OTGPWRDN |
             CFGCHIP2_OTGMODE | CFGCHIP2_REFFREQ | CFGCHIP2_USB0PHYCLKMUX);
    reg |= CFGCHIP2_FORCE_DEVICE | CFGCHIP2_DATPOL | CFGCHIP2_SESENDEN |
           CFGCHIP2_VBDTCTEN | CFGCHIP2_PHY_PLLON |
           MUSB_AM1802_USB0_REFFREQ | phy_clk_mux;

    HWREG(CFGCHIP2_USBPHYCTRL) = reg;

    for (uint32_t wait = 0; wait < MUSB_AM1802_PHY_WAIT_CYCLES; wait++) {
        uint32_t current = HWREG(CFGCHIP2_USBPHYCTRL);

        if (current & CFGCHIP2_PHYCLKGD) {
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

    if (musb_am1802_phy_try_source(
            reg, MUSB_AM1802_USB0_PHYCLKMUX & CFGCHIP2_USB0PHYCLKMUX)) {
        return true;
    }

#if MUSB_AM1802_PHYCLKMUX_FALLBACK
    if (musb_am1802_phy_try_source(
            reg, (~MUSB_AM1802_USB0_PHYCLKMUX) & CFGCHIP2_USB0PHYCLKMUX)) {
        return true;
    }
#endif

    return false;
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_enable(uint8_t rhport) {
    (void)rhport;

    s_irq_enabled = 1u;
    musb_am1802_otg_int_enable();
    IntSystemEnable(SYS_INT_USB0);
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_disable(uint8_t rhport) {
    (void)rhport;

    s_irq_enabled = 0u;
    IntSystemDisable(SYS_INT_USB0);
    musb_am1802_otg_int_disable();
}

TU_ATTR_ALWAYS_INLINE static inline unsigned
musb_dcd_get_int_enable(uint8_t rhport) {
    (void)rhport;

    return s_irq_enabled;
}

TU_ATTR_ALWAYS_INLINE static inline void musb_dcd_int_clear(uint8_t rhport) {
    (void)rhport;

    IntSystemStatusClear(SYS_INT_USB0);
    musb_am1802_otg_ack(MUSB_AM1802_INTR_MASK);
}

static inline void musb_dcd_int_handler_enter(uint8_t rhport) {
    (void)rhport;

    s_irq_status = HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC_MASKED);

#if !NESTED_INTERRUPTS
    IntSystemStatusClear(SYS_INT_USB0);
#endif

    musb_am1802_otg_ack(s_irq_status & MUSB_AM1802_INTR_MASK);
}

static inline void musb_dcd_int_status_get(uint8_t rhport,
                                           uint_fast8_t *intr_usb,
                                           uint_fast8_t *intr_tx,
                                           uint_fast8_t *intr_rx) {
    (void)rhport;

    uint32_t status = s_irq_status;

    *intr_usb = (uint_fast8_t)((status & MUSB_AM1802_INTR_USB_MASK) >>
                               MUSB_AM1802_INTR_USB_SHIFT);
    *intr_tx = (uint_fast8_t)(status & MUSB_AM1802_INTR_TX_MASK);
    *intr_rx = (uint_fast8_t)((status & MUSB_AM1802_INTR_RX_MASK) >>
                              MUSB_AM1802_INTR_RX_SHIFT);
}

#endif // CFG_TUD_ENABLED

#ifdef __cplusplus
}
#endif

#endif // TUSB_MUSB_AM1802_H_
