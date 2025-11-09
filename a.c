//Experiment 2

#include <stdint.h> // Declarations of uint32_t etc.
#include "lpc824.h" // Declarations of LPC824 register names.
#include "pin_mux.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"
#include "fsl_power.h"

#define CORE_CLOCK   6000000U  // System clock: 6MHz (SCT input becomes 12MHz after PLL)

volatile int state = 0;  // 0: LED should turn ON after delay, 1: LED should turn OFF after delay
volatile int lastButtonState = 0;

uint32_t eventCounterL;  // The event number for counter L
uint16_t matchValueL;

void clock_init(void);
void SCTimerL_init(void);

int main(void)
{
    // Initialize GPIO for button (using PIO0_24, which is different from PIO0_26 and PIO0_27)
    SYSCON_SYSAHBCLKCTRL |= 0x400C0; // Enable IOCON, SWM & GPIO clocks.
    SYSCON_PRESETCTRL &= ~(0x400);   // Peripheral reset control to gpio/gpio int
    SYSCON_PRESETCTRL |= 0x400;      // Release reset
    
    // Configure PIO0_24 as input for button (not PIO0_26 or PIO0_27)
    // PIO0_24 is commonly used for buttons on Alakart
    GPIO_DIR0 &= ~(1 << 24);  // Set PIO0_24 as input
    
    // Initialize SCTimer
    SCTimerL_init();
    
    // At power ON, both LEDs must be OFF
    // Ensure OUT2 (PIO0_27) is initially LOW - set initial output state to LOW
    SCTIMER_SetupOutputState(SCT0, kSCTIMER_Out_2, false); // Set output LOW initially
    
    lastButtonState = GPIO_B24;  // Read initial button state (PIO0_24)
    
    while (1) {
        // Read button state (PIO0_24)
        int currentButtonState = GPIO_B24;
        
        // Detect button press (edge detection: low to high transition)
        if (currentButtonState == 1 && lastButtonState == 0) {
            // Button was just pressed
            
            // Reset counter to 0 before starting
            SCTIMER_StopTimer(SCT0, kSCTIMER_Counter_L);
            SCTIMER_SetCounterValue(SCT0, kSCTIMER_Counter_L, 0);
            
            // Create match event for 1 second delay
            // With 6MHz core clock, PLL×2 = 12MHz system clock
            // SCT input = 12MHz, prescaler 250: SCT counter clock = 12MHz/250 = 48kHz
            // For 1 second: matchValueL = 48000
            matchValueL = 48000;
            
            // Delete previous event if exists and create new one
            SCTIMER_CreateAndScheduleEvent(SCT0,
                                         kSCTIMER_MatchEventOnly,
                                         matchValueL,
                                         0,                  // Not used for "Match Only"
                                         kSCTIMER_Counter_L,
                                         &eventCounterL);
            
            // Configure counter to STOP when reaching matchValL (not reset)
            SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventCounterL);
            
            // Configure output based on state:
            // state == 0: LED stays OFF for 1s, then turns ON (SET output HIGH)
            // state == 1: LED stays ON for 1s, then turns OFF (CLEAR output LOW)
            if (state == 0) {
                // LED should turn ON after delay
                SCTIMER_SetupOutputSetAction(SCT0, kSCTIMER_Out_2, eventCounterL);
            } else {
                // LED should turn OFF after delay
                SCTIMER_SetupOutputClearAction(SCT0, kSCTIMER_Out_2, eventCounterL);
            }
            
            // Event active direction (independent)
            SCTIMER_SetupEventActiveDirection(SCT0,
                                            kSCTIMER_ActiveIndependent,
                                            eventCounterL);
            
            // Start the counter
            SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
            
            // Toggle state for next button press
            state = (state + 1) % 2;
        }
        
        lastButtonState = currentButtonState;
        
        // Small delay to debounce button
        for (volatile int i = 0; i < 1000; i++);
    }
}

void SCTimerL_init(void) {
    sctimer_config_t sctimerConfig;
    
    InitPins();                           // Init board pins.
    clock_init();                         // Initialize processor clock.
    CLOCK_EnableClock(kCLOCK_Sct);        // Enable clock of SCT.
    
    SCTIMER_GetDefaultConfig(&sctimerConfig);
    sctimerConfig.enableCounterUnify = false; // Use as two 16 bit timers.
    sctimerConfig.clockMode = kSCTIMER_System_ClockMode; // Use system clock as SCT input
    
    sctimerConfig.enableBidirection_l = false; // Use as single directional timer (up-counting).
    sctimerConfig.enableBidirection_h = false; // Use as single directional timer (up-counting).
    
    // Prescaler is 8 bit, in: CTRL. See: 16.6.3 SCT control register
    // Prescaler value: 249 means divide by 250
    sctimerConfig.prescale_l = 249; // This value +1 is used. Becomes 250.
    sctimerConfig.prescale_h = 249; // Becomes 250.
    
    SCTIMER_Init(SCT0, &sctimerConfig);    // Initialize SCTimer module
    
    // Note: matchValueL will be set dynamically in main() when button is pressed
    matchValueL = 48000; // Default: 1 second delay (12MHz/250 = 48kHz, so 48000 ticks = 1s)
}

void clock_init(void) {    // Set up the clock source
    
    // Set up IRC
    POWER_DisablePD(kPDRUNCFG_PD_IRC_OUT);        // Turn ON IRC OUT
    POWER_DisablePD(kPDRUNCFG_PD_IRC);            // Turn ON IRC
    //POWER_DisablePD(kPDRUNCFG_PD_SYSOSC);       // In Alakart SYSOSC is not used.
    CLOCK_Select(kSYSPLL_From_Irc);               // Connect IRC to PLL input.
    clock_sys_pll_t config;
    config.src = kCLOCK_SysPllSrcIrc;             // Select PLL source as IRC.
    config.targetFreq = CORE_CLOCK * 2;           // set pll target freq (12MHz for 6MHz core clock)
    CLOCK_InitSystemPll(&config);                 // set parameters
    //CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcIrc);  // Select IRC as main clock source.
    CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll); // Select PLL as main clock source.
    CLOCK_Select(kCLKOUT_From_Irc);               // select IRC for CLKOUT
    CLOCK_SetCoreSysClkDiv(1U);
    
    // Check processor registers and calculate the
    // Actual clock speed. This is stored in the
    // global variable SystemCoreClock
    SystemCoreClockUpdate();
    
    // The following is for convenience and not necessary. AO.
    // It outputs the system clock on Pin 26
    //    so that we can check using an oscilloscope:
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
