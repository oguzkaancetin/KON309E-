// Experiment 2 – SCTimer controlled LED with delayed button toggle

#include <stdbool.h>
#include <stdint.h>

#include "fsl_clock.h"
#include "fsl_power.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"
#include "lpc824.h"
#include "pin_mux.h"

#define CORE_CLOCK_HZ            (6000000UL)
#define SCT_INPUT_CLOCK_HZ       (12000000UL)
#define SCT_COUNTER_DIVIDER      (256UL)          /* prescale register value + 1 */
#define SCT_MATCH_TICKS_ONE_SEC  (SCT_INPUT_CLOCK_HZ / SCT_COUNTER_DIVIDER)

#define BUTTON_B1_MASK           (1UL << 25)      /* PIO0_25, Alakart push-button */

#define SCT_OUTPUT_INDEX         (2U)             /* OUT2 routed to PIO0_27 */

static uint32_t sctEventMatchL;
static bool     ledState;
static bool     delayActive;
static bool     pendingLedState;

static void clock_init(void);
static void board_init(void);
static void configure_button_input(void);
static bool  button_is_pressed(void);

static void SCTimerL_init(void);
static void SCTimerL_armDelayedTransition(bool turnLedOn);

int main(void)
{
    board_init();
    configure_button_input();
    SCTimerL_init();

    bool lastButton = false;

    while (1)
    {
        const bool pressed = button_is_pressed();

        /* Arm a new SCT one-shot delay on a rising edge when no delay is active. */
        if (pressed && !lastButton && !delayActive)
        {
            pendingLedState = !ledState;
            SCTimerL_armDelayedTransition(pendingLedState);
            delayActive = true;
        }

        /* Check if the programmed one-shot event has completed. */
        if (delayActive)
        {
            const uint32_t eventMask = 1UL << sctEventMatchL;

            if ((SCT0->EVFLAG & eventMask) != 0U)
            {
                /* Clear event flag before arming a new delay. */
                SCT0->EVFLAG = eventMask;

                ledState    = pendingLedState;
                delayActive = false;
            }
        }

        lastButton = pressed;
    }
}

static void board_init(void)
{
    /* Enable clocks for IOCON, SWM & GPIO (bits 16, 7, 6 in SYSAHBCLKCTRL). */
    SYSCON_SYSAHBCLKCTRL |= (0x1UL << 16) | (0x1UL << 7) | (0x1UL << 6);

    /* Reset the GPIO peripheral to a clean state. */
    SYSCON_PRESETCTRL &= ~(1UL << 10);
    SYSCON_PRESETCTRL |= (1UL << 10);

    InitPins();
    clock_init();
}

static void configure_button_input(void)
{
    /* Configure PIO0_25 as GPIO input (button B1). */
    GPIO_DIR0 &= ~BUTTON_B1_MASK;
}

static bool button_is_pressed(void)
{
    /* Button is active-high on PIO0_25. */
    return (GPIO_B25 & 0x1U) != 0U;
}

static void SCTimerL_init(void)
{
    sctimer_config_t config;

    CLOCK_EnableClock(kCLOCK_Sct);

    SCTIMER_GetDefaultConfig(&config);
    config.enableCounterUnify   = false;                  /* Use two 16-bit counters. */
    config.clockMode            = kSCTIMER_System_ClockMode;
    config.enableBidirection_l  = false;
    config.enableBidirection_h  = false;
    config.prescale_l           = (uint8_t)(SCT_COUNTER_DIVIDER - 1U);
    config.prescale_h           = (uint8_t)(SCT_COUNTER_DIVIDER - 1U);

    SCTIMER_Init(SCT0, &config);

    /* Force OUT2 low at reset so LED is OFF at power-up. */
    SCT0->OUTPUT &= ~(1UL << SCT_OUTPUT_INDEX);
    ledState      = false;
    delayActive   = false;

    /* Create one match event for Counter L at 1 second. */
    SCTIMER_CreateAndScheduleEvent(SCT0,
                                   kSCTIMER_MatchEventOnly,
                                   (uint16_t)SCT_MATCH_TICKS_ONE_SEC,
                                   0U,
                                   kSCTIMER_Counter_L,
                                   &sctEventMatchL);

    /* Stop counter L after the match event in order to create a one-shot delay. */
    SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, sctEventMatchL);
}

static void SCTimerL_armDelayedTransition(bool turnLedOn)
{
    const uint32_t eventMask = 1UL << sctEventMatchL;

    /* Reset counter and clear pending event flag before arming. */
    SCT0->COUNT_L = 0U;
    SCT0->EVFLAG  = eventMask;

    /* Remove any previous OUT2 actions, then apply the requested action. */
    SCT0->OUT[SCT_OUTPUT_INDEX].SET = 0U;
    SCT0->OUT[SCT_OUTPUT_INDEX].CLR = 0U;

    if (turnLedOn)
    {
        SCT0->OUT[SCT_OUTPUT_INDEX].SET = eventMask;
    }
    else
    {
        SCT0->OUT[SCT_OUTPUT_INDEX].CLR = eventMask;
    }

    /* Start one-shot counter L run. */
    SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
}

static void clock_init(void)
{
    /* Power up IRC and route it through the PLL. */
    POWER_DisablePD(kPDRUNCFG_PD_IRC_OUT);
    POWER_DisablePD(kPDRUNCFG_PD_IRC);

    CLOCK_Select(kSYSPLL_From_Irc);

    clock_sys_pll_t config;
    config.src        = kCLOCK_SysPllSrcIrc;
    config.targetFreq = SCT_INPUT_CLOCK_HZ; /* Generate 12 MHz from PLL. */
    CLOCK_InitSystemPll(&config);

    /* Use PLL clock as main clock source and divide by two for 6 MHz core. */
    CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll);
    const uint32_t coreDiv = SCT_INPUT_CLOCK_HZ / CORE_CLOCK_HZ;
    CLOCK_SetCoreSysClkDiv(coreDiv);

    SystemCoreClockUpdate();

    /* Route main clock to CLKOUT (PIO0_26) for frequency verification. */
    SYSCON->CLKOUTSEL = 0x03U;
    SYSCON->CLKOUTUEN = 0U;
    SYSCON->CLKOUTUEN = 1U;
    SYSCON->CLKOUTDIV = 100U;

    CLOCK_EnableClock(kCLOCK_Swm);
    SWM_SetMovablePinSelect(SWM0, kSWM_CLKOUT, kSWM_PortPin_P0_26);
    CLOCK_DisableClock(kCLOCK_Swm);
}
