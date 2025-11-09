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
volatile int state = 0; // 0 = Next action is LED ON, 1 = Next action is LED OFF
volatile int button_pressed = 0;
volatile int start_timer = 0; // Flag to start timer
uint32_t eventLedOn, eventLedOff;  // Two events: one for LED ON, one for LED OFF
uint16_t matchValueL;

// Function prototypes
void clock_init(void);
void SCTimerL_init(void);
void SCT0_IRQHandler(void);

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
    
    // Initialize SCTimer - ALL LED control is configured here
    SCTimerL_init();
    
    while (1) {
        // Main program ONLY detects button press
        // It does NOT manipulate LED or SCTimer configuration
        // It only sets a flag that the interrupt handler will use
        if ((GPIO_B15 == 1) && (button_pressed == 0) && (start_timer == 0)) {
            button_pressed = 1;
            start_timer = 1;  // Signal that timer should start
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
    
    // Initial setup: LED must be OFF at power on
    // Alakart LED logic: OUT2 = HIGH (1) means LED OFF, OUT2 = LOW (0) means LED ON
    // Set OUT2 to HIGH (1) to turn LED OFF
    SCT0->OUTPUT |= (1 << 2);  // Set bit 2 (OUT2) to HIGH - LED OFF
    
    // Create TWO events - one for turning LED ON, one for turning LED OFF
    // Both are configured here at initialization time
    
    // Event for LED ON: CLEAR OUT2 (OUT2=0 means LED ON)
    SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 matchValueL,
                                 0,
                                 kSCTIMER_Counter_L,
                                 &eventLedOn);
    SCTIMER_SetupOutputClearAction(SCT0, kSCTIMER_Out_2, eventLedOn);
    SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventLedOn);
    SCTIMER_SetupEventActiveDirection(SCT0, kSCTIMER_ActiveIndependent, eventLedOn);
    
    // Event for LED OFF: SET OUT2 (OUT2=1 means LED OFF)
    SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 matchValueL,
                                 0,
                                 kSCTIMER_Counter_L,
                                 &eventLedOff);
    SCTIMER_SetupOutputSetAction(SCT0, kSCTIMER_Out_2, eventLedOff);
    SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventLedOff);
    SCTIMER_SetupEventActiveDirection(SCT0, kSCTIMER_ActiveIndependent, eventLedOff);
    
    // Initially, only LED ON event is enabled (first button press will turn LED ON)
    SCTIMER_EnableInterrupts(SCT0, (1 << eventLedOn));
    
    // Enable SCTimer interrupt in NVIC
    EnableIRQ(SCT0_IRQn);
    
    // Note: The timer will be started by the interrupt handler when start_timer flag is set
}

// SCTimer interrupt handler
// This monitors the start_timer flag and starts the appropriate timer
// When timer completes, it toggles between the two pre-configured events
void SCT0_IRQHandler(void)
{
    uint32_t flags = SCTIMER_GetStatusFlags(SCT0);
    
    // Check if eventLedOn triggered
    if (flags & (1 << eventLedOn))
    {
        SCTIMER_ClearStatusFlags(SCT0, (1 << eventLedOn));
        
        // LED is now ON, next button press should turn it OFF
        // Disable LED ON event, enable LED OFF event
        SCTIMER_DisableInterrupts(SCT0, (1 << eventLedOn));
        SCTIMER_EnableInterrupts(SCT0, (1 << eventLedOff));
        
        state = 1;  // Next action is LED OFF
        start_timer = 0;  // Timer completed
    }
    
    // Check if eventLedOff triggered
    if (flags & (1 << eventLedOff))
    {
        SCTIMER_ClearStatusFlags(SCT0, (1 << eventLedOff));
        
        // LED is now OFF, next button press should turn it ON
        // Disable LED OFF event, enable LED ON event
        SCTIMER_DisableInterrupts(SCT0, (1 << eventLedOff));
        SCTIMER_EnableInterrupts(SCT0, (1 << eventLedOn));
        
        state = 0;  // Next action is LED ON
        start_timer = 0;  // Timer completed
    }
    
    // Check if we need to start the timer (button was pressed)
    if (start_timer == 1)
    {
        // Stop timer if running
        SCTIMER_StopTimer(SCT0, kSCTIMER_Counter_L);
        
        // Clear counter
        SCT0->CTRL |= (1 << 3);  // CLRCTR_L
        
        // Start timer - the appropriate event (ON or OFF) is already enabled
        SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
    }
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
