//Experiment 1 Part 2

#include <stdint.h> // Declarations of uint32_t etc.
#include "lpc824.h" // Declarations of LPC824 register names.
#include "pin_mux.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"
#include "fsl_power.h"

#define SYSTEM_CORE_CLOCK 15000000UL   //Declare system clock as 30MHz
// (The clock speed has been set in "init.c" file to 30MHz.)

static inline uint32_t SysTickConfig(uint32_t ticks);
void SysTick_Handler(void);  //our systick interrupt handler
void delay_ms(uint32_t ms);//delay (ms)
void clock_init(void);

volatile uint32_t delaytime; // This is decremented by SysTick_Handler.
volatile int state = 0;
volatile int B1Pressed = 0; // pin 24
volatile int B2Pressed = 0; // pin 15

int main(void)
{
    delaytime=0;

    SYSCON_SYSAHBCLKCTRL |= 0x400C0; // Enable IOCON, SWM & GPIO clocks.
    SYSCON_PRESETCTRL &= ~(0x400);  // Peripheral reset control to gpio/gpio int
    SYSCON_PRESETCTRL |=   0x400;   // AO: Check.

    GPIO_DIR0 &= (!(1<<25)); // pin 24 is B1

    SysTickConfig(SYSTEM_CORE_CLOCK/1000);  //setup systick clock interrupt @1ms

    sctimer_config_t sctimerConfig;
  uint32_t eventCounterL, eventCounterH; // The event number for counter L and H
  uint16_t matchValueL, matchValueH;

  InitPins();                           // Init board pins.
  clock_init();                         // Initialize processor clock.

  CLOCK_EnableClock(kCLOCK_Sct);        // Enable clock of SCTimer.

  // SCTimer in 16-bit mode: two independent 16-bit counters (L and H).
  // Counter 'L' controls OUT2 -> PIO0_27 (Green LED on Alakart).
  // Counter 'H' controls OUT4 -> PIO0_16 (Blue  LED on Alakart).

  SCTIMER_GetDefaultConfig(&sctimerConfig);

  // Timer config
  sctimerConfig.enableCounterUnify = false;                  // Use as two 16-bit counters.
  sctimerConfig.clockMode          = kSCTIMER_System_ClockMode; // System clock as SCT input
  sctimerConfig.enableBidirection_l = false;                 // Up-counting
  sctimerConfig.enableBidirection_h = false;                 // Up-counting
  // Prescaler is 8-bit (CTRL). Value+1 is used.
  sctimerConfig.prescale_l = 249; // -> 250
  sctimerConfig.prescale_h = 249; // -> 250

  SCTIMER_Init(SCT0, &sctimerConfig);    // Initialize SCTimer module

  matchValueL = 60000; // 16-bit match value for Counter L (1 second)
  matchValueH = 60000; // 16-bit match value for Counter H (1 second)

    while (1)
    {
        if (GPIO_B25 == 1) // when button 1 is pressed
        {
            // Turn ON LEDs immediately (first toggle: OFF->ON)
            SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 1,                  // Match at count=1 (immediate)
                                 0,
                                 kSCTIMER_Counter_L,
                                 &eventCounterL);
            SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_2, eventCounterL);
            SCTIMER_SetupEventActiveDirection(SCT0,
                                    kSCTIMER_ActiveIndependent,
                                    eventCounterL);

            // Turn ON Blue LED immediately
            uint32_t eventCounterH_on;
            SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 1,
                                 0,
                                 kSCTIMER_Counter_H,
                                 &eventCounterH_on);
            SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_4, eventCounterH_on);
            SCTIMER_SetupEventActiveDirection(SCT0,
                                    kSCTIMER_ActiveIndependent,
                                    eventCounterH_on);

            // Start both counters
            SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L | kSCTIMER_Counter_H);
            
            // Wait 1 second
            delay_ms(1000);
            
            // Turn OFF LEDs (second toggle: ON->OFF)
            SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 1,
                                 0,
                                 kSCTIMER_Counter_L,
                                 &eventCounterL);
            SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_2, eventCounterL);
            SCTIMER_SetupEventActiveDirection(SCT0,
                                    kSCTIMER_ActiveIndependent,
                                    eventCounterL);

            SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 1,
                                 0,
                                 kSCTIMER_Counter_H,
                                 &eventCounterH);
            SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_4, eventCounterH);
            SCTIMER_SetupEventActiveDirection(SCT0,
                                    kSCTIMER_ActiveIndependent,
                                    eventCounterH);

            // Trigger the toggle to turn OFF
            SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L | kSCTIMER_Counter_H);
            
            // Debounce delay
            delay_ms(200);
        }
    }
}

void clock_init(void) {    // Set up the clock source

  // Set up IRC
  POWER_DisablePD(kPDRUNCFG_PD_IRC_OUT);        // Turn ON IRC OUT
  POWER_DisablePD(kPDRUNCFG_PD_IRC);            // Turn ON IRC
  //POWER_DisablePD(kPDRUNCFG_PD_SYSOSC);       // In Alakart SYSOSC is not used.
  CLOCK_Select(kSYSPLL_From_Irc);               // Connect IRC to PLL input.

  clock_sys_pll_t config;
  config.src = kCLOCK_SysPllSrcIrc;             // Select PLL source as IRC.
  config.targetFreq = SYSTEM_CORE_CLOCK * 1;    // Set PLL target freq

  CLOCK_InitSystemPll(&config);                 // Apply PLL parameters
  CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll); // Main clock source = PLL
  CLOCK_Select(kCLKOUT_From_Irc);               // CLKOUT source = IRC
  CLOCK_SetCoreSysClkDiv(1U);

  // Update SystemCoreClock global
  SystemCoreClockUpdate ();

  // (Optional) Output main clock on Pin 26 for scope verification
  SYSCON->CLKOUTSEL = (uint32_t)3; // CLKOUT source to main clock
  SYSCON->CLKOUTUEN = 0UL;
  SYSCON->CLKOUTUEN = 1UL;
  SYSCON->CLKOUTDIV = 100;         // Divide for observable frequency

  // Route CLKOUT to P0_26 via SWM
  CLOCK_EnableClock(kCLOCK_Swm);     // Enable SWM clock
  SWM_SetMovablePinSelect(SWM0, kSWM_CLKOUT, kSWM_PortPin_P0_26);
  CLOCK_DisableClock(kCLOCK_Swm);    // Disable SWM clock (power save)
}


//The interrupt handler for SysTick system time-base timer.
void SysTick_Handler(void)
{
    if (delaytime!=0)
    { // If delaytime has been set somewhere in the program,
        --delaytime;     //  decrement it every time SysTick event occurs (1ms).
    }
}

void delay_ms(uint32_t ms)
{
    delaytime=ms;        // Set the delay time to the number of millisecs of wait
    while(delaytime!=0){}// Wait here until the delay time expires.
}

// System Tick Configuration:
// Initializes the System Timer and its interrupt, and
// Starts the System Tick Timer.
// ticks = Number of ticks between two interrupts.

static inline uint32_t SysTickConfig(uint32_t ticks)
{
    if (ticks > 0xFFFFFFUL) // Timer is only 24 bits wide.
    {
        return (1); //Reload value impossible
    }

    SYST_RVR = (ticks & 0xFFFFFFUL) - 1;  //Set reload register
    SYST_CVR = 0;   //Load the initial count value.
    SYST_CSR = 0x07;  // Counter ENABLE, INT ENABLE, CLK source=system clock.
    return (0);
}         // AO!: Check OK.
