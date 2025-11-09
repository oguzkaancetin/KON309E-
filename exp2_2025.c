// Experiment 2 – SCTimer controlled LED sequencer

#include <stdbool.h>
#include <stdint.h>

#include "lpc824.h"
#include "pin_mux.h"
#include "fsl_clock.h"
#include "fsl_power.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"

/*******************************************************************************
 * Configuration constants
 ******************************************************************************/

#define CORE_CLOCK_HZ        (6000000U)            /* Target system clock */
#define SCT_INPUT_CLOCK_HZ   (CORE_CLOCK_HZ * 2U)  /* Feed SCT from PLL -> 12 MHz */
#define SCT_PRESCALE_VALUE   (239U)                /* (Prescale + 1) => 240 */
#define SCT_TICKS_PER_SECOND (SCT_INPUT_CLOCK_HZ / (SCT_PRESCALE_VALUE + 1U))
#define SCT_MATCH_1S         ((uint16_t)(SCT_TICKS_PER_SECOND - 1U))

#define BUTTON_GPIO_MASK     (1UL << 25)           /* PIO0_25 (B1), avoids PIO0_26/27 */
#define BUTTON_ACTIVE_LEVEL  (1U)

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/

static inline uint32_t SysTickConfig(uint32_t ticks);
static void clock_init(void);
static void configure_button_input(void);
static bool button_pressed(void);
static void delay_ms(uint32_t ms);
static void start_led_transition(bool targetOn);
static void clear_pending_transition(void);

void SysTick_Handler(void);
void SCTimerL_init(uint16_t matchValue);

/*******************************************************************************
 * Module globals
 ******************************************************************************/

static volatile uint32_t s_delayTime;
static volatile bool s_ledOn;
static volatile bool s_transitionPending;
static bool s_pendingLedState;
static uint32_t s_eventCounterL;
static uint32_t s_eventMaskL;

/*******************************************************************************
 * Main application
 ******************************************************************************/

int main(void)
{
    /* Enable IOCON, SWM & GPIO clocks and reset GPIO block */
    SYSCON_SYSAHBCLKCTRL |= 0x400C0UL;
    SYSCON_PRESETCTRL &= ~(0x400UL);
    SYSCON_PRESETCTRL |= 0x400UL;

    configure_button_input();

    /* Configure SCTimer (also sets up system clocking) */
    SCTimerL_init(SCT_MATCH_1S);

    /* SysTick at 1 ms for lightweight delays / debounce */
    SysTickConfig(SystemCoreClock / 1000U);
    s_delayTime = 0U;

    bool lastButtonState = false;

    while (1)
    {
        const bool pressed = button_pressed();

        if (pressed && !lastButtonState && !s_transitionPending)
        {
            start_led_transition(!s_ledOn);
            delay_ms(20U); /* Simple debounce */
        }

        lastButtonState = pressed;

        if (s_transitionPending && (SCT0->EVFLAG & s_eventMaskL) != 0U)
        {
            SCT0->EVFLAG = s_eventMaskL;
            s_ledOn = s_pendingLedState;
            clear_pending_transition();
        }
    }
}

/*******************************************************************************
 * SCTimer configuration
 ******************************************************************************/

void SCTimerL_init(uint16_t matchValue)
{
    sctimer_config_t sctimerConfig;

    InitPins();                /* Configure SWM routing (LED only, no PIO0_16) */
    clock_init();              /* System + PLL clock setup */

    CLOCK_EnableClock(kCLOCK_Sct);

    /* Feed SCT with PLL clock (12 MHz) */
    SYSCON->SCTCLKSEL = 1U;    /* Select PLL output */
    SYSCON->SCTCLKDIV = 1U;    /* No additional divide */

    SCTIMER_GetDefaultConfig(&sctimerConfig);
    sctimerConfig.enableCounterUnify   = false;
    sctimerConfig.clockMode            = kSCTIMER_System_ClockMode;
    sctimerConfig.enableBidirection_l  = false;
    sctimerConfig.enableBidirection_h  = false;
    sctimerConfig.prescale_l           = SCT_PRESCALE_VALUE;
    sctimerConfig.prescale_h           = SCT_PRESCALE_VALUE;

    SCTIMER_Init(SCT0, &sctimerConfig);

    /* Start with LED off – SCT OUT2 drives PIO0_27 */
    SCT0->OUTPUT = 0U;
    s_ledOn = false;

    /* Create a single-shot match event on counter L */
    SCTIMER_CreateAndScheduleEvent(SCT0,
                                   kSCTIMER_MatchEventOnly,
                                   matchValue,
                                   0U,
                                   kSCTIMER_Counter_L,
                                   &s_eventCounterL);

    s_eventMaskL = (1UL << s_eventCounterL);

    SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, s_eventCounterL);
    SCTIMER_SetupEventActiveDirection(SCT0,
                                      kSCTIMER_ActiveIndependent,
                                      s_eventCounterL);

    /* Make sure both SET and CLR actions are inactive initially */
    SCT0->OUT[kSCTIMER_Out_2].SET &= ~s_eventMaskL;
    SCT0->OUT[kSCTIMER_Out_2].CLR &= ~s_eventMaskL;

    /* Clear any stale flags and hold counter idle */
    SCT0->EVFLAG = s_eventMaskL;
    SCT0->COUNT_L = 0U;
    s_transitionPending = false;
}

/*******************************************************************************
 * Helper functions
 ******************************************************************************/

static void start_led_transition(bool targetOn)
{
    s_pendingLedState   = targetOn;
    s_transitionPending = true;

    /* Disable previous actions */
    SCT0->OUT[kSCTIMER_Out_2].SET &= ~s_eventMaskL;
    SCT0->OUT[kSCTIMER_Out_2].CLR &= ~s_eventMaskL;

    if (targetOn)
    {
        SCT0->OUT[kSCTIMER_Out_2].SET |= s_eventMaskL;
    }
    else
    {
        SCT0->OUT[kSCTIMER_Out_2].CLR |= s_eventMaskL;
    }

    /* Restart counter L from zero */
    SCT0->EVFLAG = s_eventMaskL;
    SCT0->COUNT_L = 0U;
    SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
}

static void clear_pending_transition(void)
{
    SCTIMER_StopTimer(SCT0, kSCTIMER_Counter_L);
    SCT0->COUNT_L = 0U;
    s_transitionPending = false;
}

static void configure_button_input(void)
{
    GPIO_DIR0 &= ~BUTTON_GPIO_MASK;
}

static bool button_pressed(void)
{
    return (GPIO_B25 == BUTTON_ACTIVE_LEVEL);
}

static void delay_ms(uint32_t ms)
{
    s_delayTime = ms;
    while (s_delayTime != 0U)
    {
        __NOP();
    }
}

void SysTick_Handler(void)
{
    if (s_delayTime != 0U)
    {
        --s_delayTime;
    }
}

static inline uint32_t SysTickConfig(uint32_t ticks)
{
    if (ticks > 0xFFFFFFUL)
    {
        return 1U;
    }

    SYST_RVR = (ticks & 0xFFFFFFUL) - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = 0x07UL;
    return 0U;
}

static void clock_init(void)
{
    POWER_DisablePD(kPDRUNCFG_PD_IRC_OUT);
    POWER_DisablePD(kPDRUNCFG_PD_IRC);
    CLOCK_Select(kSYSPLL_From_Irc);

    clock_sys_pll_t config;
    config.src        = kCLOCK_SysPllSrcIrc;
    config.targetFreq = SCT_INPUT_CLOCK_HZ;

    CLOCK_InitSystemPll(&config);
    CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll);
    CLOCK_Select(kCLKOUT_From_Irc);
    CLOCK_SetCoreSysClkDiv(2U);

    SystemCoreClockUpdate();

    /* Route main clock to PIO0_26 (optional measurement aid) */
    SYSCON->CLKOUTSEL = 3U;
    SYSCON->CLKOUTUEN = 0U;
    SYSCON->CLKOUTUEN = 1U;
    SYSCON->CLKOUTDIV = 100U;

    CLOCK_EnableClock(kCLOCK_Swm);
    SWM_SetMovablePinSelect(SWM0, kSWM_CLKOUT, kSWM_PortPin_P0_26);
    CLOCK_DisableClock(kCLOCK_Swm);
}
