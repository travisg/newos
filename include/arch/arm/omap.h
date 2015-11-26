#ifndef __OMAP_H
#define __OMAP_H

#include <newos/types.h>

#define REG(r)                  ((vulong *)(r))
#define REG_H(r)                ((vushort *)(r))
#define REG_B(r)                ((vuchar *)(r))

#define W_REG(r,v)              (*(vuint *)(r) = (uint)(v))
#define R_REG(r)                (*(vuint *)(r))

#define W_REG_H(r,v)    (*(vushort *)(r) = (ushort)(v))
#define R_REG_H(r)              (*(vushort *)(r))

#define W_REG_B(r,v)    (*(vuchar *)(r) = (uchar)(v))
#define R_REG_B(r)              (*(vuchar *)(r))

#define RMW_REG(r,v,m)          W_REG(r, (R_REG(r) & ~(m)) | (v))
#define RMW_REG_H(r,v,m)        W_REG_H(r, (R_REG_H(r) & ~(m)) | (v))
#define RMW_REG_B(r,v,m)        W_REG_B(r, (R_REG_B(r) & ~(m)) | (v))

/* OMAP 161x Regs */

#define ARM_CKCTL           0xFFFECE00
#define EN_DSPCK                                 ((uint16)0x2000)
#define ARM_TIMXO_CK_GEN1                        ((uint16)0x1000)
#define DSPMMUDIV_MASK                           ((uint16)0x0C00)
#define TC_CK_DIV_MASK                           ((uint16)0x0300)
#define DSP_DIV_MASK                             ((uint16)0x00C0)
#define ARM_DIV_MASK                             ((uint16)0x0030)
#define LCD_DIV_MASK                             ((uint16)0x000C)
#define PERIPH_DIV_MASK                          ((uint16)0x0003)

#define ARM_IDLECT1         0xFFFECE04
#define IDLE_CLKOUT_ARM                          ((uint16)0x1000)
#define WKUP_MODE_ANY                            ((uint16)0x0400)
#define IDLE_STOP_TIMER                          ((uint16)0x0200)
#define IDLE_API_ARM                             ((uint16)0x0100)
#define IDLE_DPLL_ARM                            ((uint16)0x0080)
#define IDLE_IF_ARM                              ((uint16)0x0040)
#define IDLE_PER_ARM                             ((uint16)0x0004)
#define IDLE_OS_CLK_ARM                          ((uint16)0x0002)
#define IDLE_WTD_ARM

#define ARM_IDLECT2         0xFFFECE08
#define EN_CLKOUT_ARM                            ((uint16)0x0800)
#define DMA_CLK_REQ                              ((uint16)0x0100)
#define EN_TIMER_CLK                             ((uint16)0x0080)
#define EN_API_CLK                               ((uint16)0x0040)
#define EN_LCD_CLK                               ((uint16)0x0008)
#define EN_PERIPH_CLK                            ((uint16)0x0004)
#define EN_OS_CLK                                ((uint16)0x0002)
#define EN_WDT_CLK                               ((uint16)0x0001)

#define ARM_IDLECT3         0xFFFECE24
#define IDLTC2_ARM                               ((uint16)0x0020)
#define EN_TC2_CLK                               ((uint16)0x0010)
#define IDLTC1_ARM                               ((uint16)0x0008)
#define EN_TC1_CLK                               ((uint16)0x0004)
#define IDLOCPI_ARM                              ((uint16)0x0002)
#define EN_OCPI_CLK                              ((uint16)0x0001)

#define ARM_EWUPCT          0xFFFECE0C
#define EN_EXT_PWR_CTRL                          ((uint16)0x0020)
#define EXT_PWR_DELAY_MASK                       ((uint16)0x001F)

#define ARM_RSTCT1          0xFFFECE10
#define SW_RESET                                 ((uint16)0x0008)
#define DSP_RESET                                ((uint16)0x0004)
#define DSP_EN                                   ((uint16)0x0002)
#define ARM_RESET                                ((uint16)0x0001)

#define ARM_RSTCT2          0xFFFECE14
#define PERIPH_EN                                ((uint16)0x0001)

#define ARM_SYSST           0xFFFECE18
// CLOCK_SELECT Bits (13:11) of ARM_SYSST
#define ARM_CLOCK_SCHEME_MASK                    ((uint16)0x3800
#define ARM_CLOCK_SCHEME_FULLY                   ((uint16)0x0000 // fully synchronous
#define ARM_CLOCK_SCHEME_SCALABLE                ((uint16)0x1000 // synchronous scalable
#define ARM_CLOCK_SCHEME_BYPASS                  ((uint16)0x2800 // bypass mode
#define ARM_CLOCK_SCHEME_MIX_3                   ((uint16)0x3000 // mix mode #3
#define ARM_CLOCK_SCHEME_MIX_4                   ((uint16)0x3800 // mix mode #4

#define ARM_CKOUT1          0xFFFECE1C
#define ARM_CKOUT2          0xFFFECE20
#define MPUI_CTRL           0xFFFEC900

/* clock control */
#define ARM_DPLL1_CTL_REG   0xFFFECF00
#define ARM_CLK_SYNCHRONOUS_48MHz                ((uint16)0x2213)
#define ARM_CLK_SYNCHRONOUS_60MHz                ((uint16)0x2293)
#define ARM_CLK_SYNCHRONOUS_72MHz                ((uint16)0x2313)
#define ARM_CLK_SYNCHRONOUS_84MHz                ((uint16)0x2393)
#define ARM_CLK_SYNCHRONOUS_120MHz               ((uint16)0x2513)
#define ARM_CLK_SYNCHRONOUS_192MHz               ((uint16)0x2813)

#define CLOCK_CTRL_REG          0xFFFE0830
#define ARM_SW_CLOCK_REQUEST    0xFFFE0834
#define ARM_STATUS_REQ_REG      0xFFFE0840
#define ULPD_PLL_CTRL_STATUS    0xFFFE084C
#define ARM_SW_CLOCK_DISABLE    0xFFFE0868
#define USB_CLOCK_ENABLE                         ((uint16)0x0008)
#define BT_MCLK_ENABLE                           ((uint16)0x0004)
#define COM_MCLK_ENABLE                          ((uint16)0x0002)
#define PLL_48MHZ_ENABLE                         ((uint16)0x0001)
#define USB_48MHZ_ENABLE                         ((uint16)0x0100)
#define UART1_48MHZ_ENABLE                       ((uint16)0x0200)
#define UART2_48MHZ_ENABLE                       ((uint16)0x0400)
#define UART3_48MHZ_ENABLE                       ((uint16)0x0800)
#define MMC_48MHZ_ENABLE                         ((uint16)0x1000)
#define MMC2_48MHZ_ENABLE                        ((uint16)0x2000)
#define ARM_POWER_CTRL_REG      0xFFFE0850

/* EMIFF */
#define EMIFF_PRIOR             0xFFFECC08
#define EMIFF_CONFIG            0xFFFECC20
#define EMIFF_MRS               0xFFFECC24
#define EMIFF_CONFIG2           0xFFFECC3C
#define EMIFF_DLL_WRD_CTRL      0xFFFECC64
#define EMIFF_DLL_WRD_STAT      0xFFFECC68
#define EMIFF_MRS_NEW           0xFFFECC70
#define EMIFF_EMRS0             0xFFFECC74
#define EMIFF_EMRS1             0xFFFECC78
#define EMIFF_OP                0xFFFECC80
#define EMIFF_CMD               0xFFFECC84
#define EMIFF_PTOR1             0xFFFECC8C
#define EMIFF_PTOR2             0xFFFECC90
#define EMIFF_PTOR3             0xFFFECC94
#define EMIFF_AADDR             0xFFFECC98
#define EMIFF_ATYPER            0xFFFECC9C
#define EMIFF_DLL_LRD_STAT      0xFFFECCBC
#define EMIFF_DLL_URD_CTRL      0xFFFECCC0
#define EMIFF_DLL_URD_STAT      0xFFFECCC4
#define EMIFF_EMRS2             0xFFFECCC8
#define EMIFF_DLL_LRD_CTRL      0xFFFECCCC

/* mux control */
#define FUNC_MUX_CTRL_0         0xFFFE1000
#define FUNC_MUX_CTRL_1         0xFFFE1004
#define FUNC_MUX_CTRL_2         0xFFFE1008
#define COMP_MODE_CTRL_0        0xFFFE100C
#define FUNC_MUX_CTRL_3         0xFFFE1010
#define FUNC_MUX_CTRL_4         0xFFFE1014
#define FUNC_MUX_CTRL_5         0xFFFE1018
#define FUNC_MUX_CTRL_6         0xFFFE101C
#define FUNC_MUX_CTRL_7         0xFFFE1020
#define FUNC_MUX_CTRL_8         0xFFFE1024
#define FUNC_MUX_CTRL_9         0xFFFE1028
#define FUNC_MUX_CTRL_A         0xFFFE102C
#define FUNC_MUX_CTRL_B         0xFFFE1030
#define FUNC_MUX_CTRL_C         0xFFFE1034
#define FUNC_MUX_CTRL_D         0xFFFE1038
#define PULL_DWN_CTRL_0         0xFFFE1040
#define PULL_DWN_CTRL_1         0xFFFE1044
#define PULL_DWN_CTRL_2         0xFFFE1048
#define PULL_DWN_CTRL_3         0xFFFE104C
#define GATE_INH_CTRL_0         0xFFFE1050
#define CONF_REV                0xFFFE1058
#define VOLTAGE_CTRL_0          0xFFFE1060
#define USB_TRANSCEIVER_CTRL    0xFFFE1064
#define LDO_PWRDN_CTRL          0xFFFE1068
#define TEST_DBG_CTRL_0         0xFFFE1070
#define MOD_CONF_CTRL_0         0xFFFE1080
#define FUNC_MUX_CTRL_E         0xFFFE1090
#define FUNC_MUX_CTRL_F         0xFFFE1094
#define FUNC_MUX_CTRL_10        0xFFFE1098
#define FUNC_MUX_CTRL_11        0xFFFE109C
#define FUNC_MUX_CTRL_12        0xFFFE10A0
#define PULL_DWN_CTRL_4         0xFFFE10AC
#define PU_PD_SEL_0             0xFFFE10B4
#define PU_PD_SEL_1             0xFFFE10B8
#define PU_PD_SEL_2             0xFFFE10BC
#define PU_PD_SEL_3             0xFFFE10C0
#define PU_PD_SEL_4             0xFFFE10C4
#define MOD_CONF_CTRL_1         0xFFFE1110
#define SECCTRL                 0xFFFE1120
#define CONF_STATUS             0xFFFE1130
#define RESET_CONTROL           0xFFFE1140
#define CONF_1611_CTRL          0xFFFE1150

/* OMAP Device ID Regs */
#define OMAP_DIE_ID_0           0xFFFE1800
#define OMAP_DIE_ID_1           0xFFFE1804
#define OMAP_PRODUCTION_ID_0    0xFFFE2000
#define OMAP_PRODUCTION_ID_1    0xFFFE2004
#define OMAP32_ID               0xFFFED400

/* Interrupt Controller Regs */
#define ITR1                    0xFFFECB00
#define MIR1                    0xFFFECB04
#define SIR_IRQ_CODE1           0xFFFECB10
#define SIR_FIQ_CODE1           0xFFFECB14
#define CONTROL_REG1            0xFFFECB18
#define ILR1_BASE               0xFFFECB1C
#define SISR1                   0xFFFECB9C
#define GMR1                    0xFFFECBA0

#define ITR2                    0xFFFE0000
#define MIR2                    0xFFFE0004
#define SIR_IRQ_CODE2           0xFFFE0010
#define SIR_FIQ_CODE2           0xFFFE0014
#define CONTROL_REG2            0xFFFE0018
#define ILR2_BASE               0xFFFE001C
#define SISR2                   0xFFFE009C
#define STATUS2                 0xFFFE00A0
#define OCP_CFG2                0xFFFE00A0
#define INTH_REV2               0xFFFE00A0

/* DSP */
#define DSP_CKCTL               0xE1008000
#define DSP_IDLECT1             0xE1008004
#define DSP_IDLECT2             0xE1008008

/* USB Client */
/* all regs 16-bits wide */
#define USBC_REV                0xFFFB4000
#define USBC_EP_NUM             0xFFFB4004
#define USBC_DATA               0xFFFB4008
#define USBC_CTRL               0xFFFB400C
#define USBC_STAT_FLG           0xFFFB4010
#define USBC_RXFSTAT            0xFFFB4014
#define USBC_SYSCON1            0xFFFB4018
#define USBC_SYSCON2            0xFFFB401C
#define USBC_DEVSTAT            0xFFFB4020
#define USBC_SOF                0xFFFB4024
#define USBC_IRQ_EN             0xFFFB4028
#define USBC_DMA_IRQ_EN         0xFFFB402C
#define USBC_IRQ_SRC            0xFFFB4030
#define USBC_EPN_STAT           0xFFFB4034
#define USBC_DMAN_STAT          0xFFFB4038
#define USBC_RXDMA_CFG          0xFFFB4040
#define USBC_TXDMA_CFG          0xFFFB4044
#define USBC_DATA_DMA           0xFFFB4048
#define USBC_TXDMA0             0xFFFB4050
#define USBC_TXDMA1             0xFFFB4054
#define USBC_TXDMA2             0xFFFB4058
#define USBC_RXDMA0             0xFFFB4060
#define USBC_RXDMA1             0xFFFB4064
#define USBC_RXDMA2             0xFFFB4068
#define USBC_EP0                0xFFFB4080
#define USBC_EPn_RX_BASE        0xFFFB4080
#define USBC_EPn_TX_BASE        0xFFFB40C0

/* USB OTG */
#define OTG_REV                 0xFFFB0400
#define OTG_SYSCON_1            0xFFFB0404
#define OTG_SYSCON_2            0xFFFB0408
#define OTG_CTRL                0xFFFB040C
#define OTG_IRQ_EN              0xFFFB0410
#define OTG_IRQ_SRC             0xFFFB0414
#define OTG_OUTCTRL             0xFFFB0418
#define OTG_TEST                0xFFFB0420

/* I2C */
#define I2C_REV                 0xFFFB3800
#define I2C_IE                  0xFFFB3804
#define I2C_STAT                0xFFFB3808
#define I2C_SYSS                0xFFFB3810
#define I2C_BUF                 0xFFFB3814
#define I2C_CNT                 0xFFFB3818
#define I2C_DATA                0xFFFB381C
#define I2C_SYSC                0xFFFB3820
#define I2C_CON                 0xFFFB3824
#define I2C_OA                  0xFFFB3828
#define I2C_SA                  0xFFFB382C
#define I2C_PSC                 0xFFFB3830
#define I2C_SCLL                0xFFFB3834
#define I2C_SCLH                0xFFFB3838
#define I2C_SYSTEST             0xFFFB383C

#endif
