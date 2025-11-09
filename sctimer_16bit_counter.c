//Experiment 1 Part 2

#include <stdint.h> // Declarations of uint32_t etc.
#include "lpc824.h" // Declarations of LPC824 register names.
#include "pin_mux.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"
#include "fsl_power.h"

#define SYSTEM_CORE_CLOCK 6000000UL   //Declare system clock as 6MHz
// (The clock speed has been set to 6MHz.)

static inline uint32_t SysTickConfig(uint32_t ticks);
void SysTick_Handler(void);  //our systick interrupt handler
void delay_ms(uint32_t ms);//delay (ms)
void clock_init(void);
void SCTimerL_init(void);

volatile uint32_t delaytime; // This is decremented by SysTick_Handler.
volatile int state = 0;
volatile int B1Pressed = 0; // pin 24
volatile int B2Pressed = 0; // pin 15
volatile int ledState = 0; // 0 = OFF, 1 = ON
uint32_t eventCounterL; // Event number for Counter L

int main(void)
{
    delaytime=0;

    SYSCON_SYSAHBCLKCTRL |= 0x400C0; // Enable IOCON, SWM & GPIO clocks.
    SYSCON_PRESETCTRL &= ~(0x400);  // Peripheral reset control to gpio/gpio int
    SYSCON_PRESETCTRL |=   0x400;   // AO: Check.

    GPIO_DIR0 &= (!(1<<25)); // pin 24 is B1

    SysTickConfig(SYSTEM_CORE_CLOCK/1000);  //setup systick clock interrupt @1ms

    InitPins();                           // Init board pins.
    clock_init();                         // Initialize processor clock.

    CLOCK_EnableClock(kCLOCK_Sct);        // Enable clock of SCTimer.

    SCTimerL_init();                      // Initialize SCTimer Counter L

    ledState = 0; // LEDs start in OFF state

    while (1)
    {
        if ((GPIO_PIN0 & (1 << 25)) == 0) // when button 1 is pressed (active low)
        {
            // Stop and reset counter L
            SCTIMER_StopTimer(SCT0, kSCTIMER_Counter_L);
            SCT0->COUNT = 0; // Reset counter (lower 16-bit for Counter L)
            
            // Start counter L (will toggle LED after 1 second)
            SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
            
            // Wait for button release (debounce)
            while ((GPIO_PIN0 & (1 << 25)) == 0) 
            {
                delay_ms(10);
            }
            delay_ms(50); // Additional debounce delay
        }
    }
}

void SCTimerL_init(void) {
    sctimer_config_t sctimerConfig;

    // SCTimer in 16-bit mode: two independent 16-bit counters (L and H).
    // Counter 'L' controls OUT2 -> PIO0_27 (Green LED on Alakart).

    SCTIMER_GetDefaultConfig(&sctimerConfig);

    // Timer config
    sctimerConfig.enableCounterUnify = false;                  // Use as two 16-bit counters.
    sctimerConfig.clockMode          = kSCTIMER_System_ClockMode; // System clock as SCT input
    sctimerConfig.enableBidirection_l = false;                 // Up-counting
    sctimerConfig.enableBidirection_h = false;                 // Up-counting
    // Prescaler is 8-bit (CTRL). Value+1 is used.
    // With 12MHz SCT clock and prescaler 250: 12MHz/250 = 48kHz
    sctimerConfig.prescale_l = 249; // -> 250
    sctimerConfig.prescale_h = 249; // -> 250

    SCTIMER_Init(SCT0, &sctimerConfig);    // Initialize SCTimer module

    // Configure one event for 1 second match
    SCTIMER_CreateAndScheduleEvent(SCT0,
                                   kSCTIMER_MatchEventOnly,
                                   48000,  // 1 second at 48kHz
                                   0,
                                   kSCTIMER_Counter_L,
                                   &eventCounterL);

    // Setup toggle action on OUT2
    SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_2, eventCounterL);

    // Event active direction
    SCTIMER_SetupEventActiveDirection(SCT0,
                                      kSCTIMER_ActiveIndependent,
                                      eventCounterL);

    // Stop counter after match (one-shot)
    SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventCounterL);

    // Set initial output to LOW (LED OFF at start - try active high)
    SCT0->OUTPUT = 0x00;  // All outputs LOW (LED OFF)
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
