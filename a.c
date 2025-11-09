//Experiment 2

#include <stdint.h> // Declarations of uint32_t etc.
#include "lpc824.h" // Declarations of LPC824 register names.
#include "pin_mux.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"
#include "fsl_power.h"

#define CORE_CLOCK   6000000U

// Global variables
volatile int state = 0; // 0 = LED will turn ON after delay, 1 = LED will turn OFF after delay
volatile int button_pressed = 0;
uint32_t eventCounterL; // The event number for counter L
uint16_t matchValueL;

// Function prototypes
void clock_init(void);
void SCTimerL_init(void);

int main(void)
{
    // Initialize everything
    InitPins();                           // Init board pins.
    clock_init();                         // Initialize processor clock.
    
    // Configure button pin PIO0_15 as input
    SYSCON_SYSAHBCLKCTRL |= 0x400C0;     // Enable IOCON, SWM & GPIO clocks.
    SYSCON_PRESETCTRL &= ~(0x400);       // Peripheral reset control to gpio/gpio int
    SYSCON_PRESETCTRL |=   0x400;
    GPIO_DIR0 &= ~(1<<15);               // PIO0_15 (pin 15) as input for button
    
    // Initialize SCTimer
    SCTimerL_init();
    
    // At power ON, green LED on PIO0_27 must be OFF
    // This is handled by the initial SCTimer output state
    
    while (1) {
        // Check if button on PIO0_15 is pressed (active high when pressed)
        if ((GPIO_B15 == 1) && (button_pressed == 0)) {
            button_pressed = 1; // Debounce: mark as pressed
            
            // Stop the timer if it's running
            SCTIMER_StopTimer(SCT0, kSCTIMER_Counter_L);
            
            // Reset counter to 0
            SCT0->CTRL |= (1 << 3);  // Set CLRCTR_L bit to clear counter L
            
            // Recreate the event with the match value for 1 second
            SCTIMER_CreateAndScheduleEvent(SCT0,
                                         kSCTIMER_MatchEventOnly,
                                         matchValueL,
                                         0,                  // Not used for "Match Only"
                                         kSCTIMER_Counter_L,
                                         &eventCounterL);
            
            // Configure to stop when reaching matchValL
            SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventCounterL);
            
            // Set event active direction
            SCTIMER_SetupEventActiveDirection(SCT0,
                                            kSCTIMER_ActiveIndependent,
                                            eventCounterL);
            
            if (state == 0) {
                // State 0: LED is OFF, will turn ON after 1s
                // Set OUTPUT 2 (turn LED ON) when counter reaches matchValL
                SCTIMER_SetupOutputSetAction(SCT0, kSCTIMER_Out_2, eventCounterL);
                state = 1; // Next time, LED will turn OFF
            }
            else { // state == 1
                // State 1: LED is ON, will turn OFF after 1s
                // Clear OUTPUT 2 (turn LED OFF) when counter reaches matchValL
                SCTIMER_SetupOutputClearAction(SCT0, kSCTIMER_Out_2, eventCounterL);
                state = 0; // Next time, LED will turn ON
            }
            
            // Start the timer
            SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
        }
        
        // Reset button_pressed flag when button is released
        if (GPIO_B15 == 0) {
            button_pressed = 0;
        }
    }
}

void SCTimerL_init(void)
{
    sctimer_config_t sctimerConfig;
    
    CLOCK_EnableClock(kCLOCK_Sct);        // Enable clock of sct.
    
    SCTIMER_GetDefaultConfig(&sctimerConfig);
    
    sctimerConfig.enableCounterUnify = false; // Use as two 16 bit timers.
    sctimerConfig.clockMode = kSCTIMER_System_ClockMode; // Use system clock as SCT input
    sctimerConfig.enableBidirection_l = false; // Use as single directional timer.
    sctimerConfig.enableBidirection_h = false; // Use as single directional timer.
    
    // Prescaler is 8 bit, in: CTRL. See: 16.6.3 SCT control register
    // System clock = 6MHz, SCT input = 12MHz (double of system clock)
    // With prescale_l = 249, effective frequency = 12MHz / 250 = 48kHz
    sctimerConfig.prescale_l = 249; // This value +1 is used, becomes 250.
    sctimerConfig.prescale_h = 249; // Not used but set anyway
    
    SCTIMER_Init(SCT0, &sctimerConfig);    // Initialize SCTimer module
    
    // For 1 second delay: 48kHz * 1s = 48000 counts
    matchValueL = 48000; // 1 second delay at 48kHz
    
    // Set initial output state: LED OFF (output LOW)
    // OUT2 is connected to PIO0_27 (Green LED)
    SCTIMER_SetupOutputSetAction(SCT0, kSCTIMER_Out_2, 0); // Clear on event 0 (never happens)
    
    // Note: The timer will be started when button is pressed in main()
}

void clock_init(void)     // Set up the clock source
{
    // Set up IRC
    POWER_DisablePD(kPDRUNCFG_PD_IRC_OUT);        // Turn ON IRC OUT
    POWER_DisablePD(kPDRUNCFG_PD_IRC);            // Turn ON IRC
    CLOCK_Select(kSYSPLL_From_Irc);               // Connect IRC to PLL input.
    
    clock_sys_pll_t config;
    config.src = kCLOCK_SysPllSrcIrc;             // Select PLL source as IRC.
    config.targetFreq = CORE_CLOCK * 2;           // set pll target freq (12MHz for SCT)
    
    CLOCK_InitSystemPll(&config);                 // set parameters
    CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll); // Select PLL as main clock source.
    CLOCK_Select(kCLKOUT_From_Irc);               // select IRC for CLKOUT
    CLOCK_SetCoreSysClkDiv(1U);
    
    // Check processor registers and calculate the
    // Actual clock speed. This is stored in the
    // global variable SystemCoreClock
    SystemCoreClockUpdate();
    
    // The following is for convenience and not necessary.
    // It outputs the system clock on Pin 26
    // so that we can check using an oscilloscope:
    // First activate the clock out function:
    SYSCON->CLKOUTSEL = (uint32_t)3; //set CLKOUT source to main clock.
    SYSCON->CLKOUTUEN = 0UL;
    SYSCON->CLKOUTUEN = 1UL;
    // Divide by a reasonable constant so that it is easy to view on an oscilloscope:
    SYSCON->CLKOUTDIV = 100; // Max is 255.
    
    // Using the switch matrix, connect clock out to Pin 26:
    CLOCK_EnableClock(kCLOCK_Swm);     // Enables clock for switch matrix.
    SWM_SetMovablePinSelect(SWM0, kSWM_CLKOUT, kSWM_PortPin_P0_26);
    CLOCK_DisableClock(kCLOCK_Swm); // Disable clock for switch matrix.
}
