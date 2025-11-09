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
volatile int state = 0; // 0 = LED kapalı, sonraki toggle'da açılacak
                        // 1 = LED açık, sonraki toggle'da kapanacak
volatile int button_pressed = 0;
volatile int timer_running = 0;
uint32_t eventCounterL; // The event number for counter L
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
    
    // Initialize SCTimer - this is the ONLY place where LED control is set up
    SCTimerL_init();
    
    while (1) {
        // Main program only detects button press and starts the timer
        // LED manipulation is done entirely by SCTimer
        if ((GPIO_B15 == 1) && (button_pressed == 0) && (timer_running == 0)) {
            button_pressed = 1; // Debounce: mark as pressed
            timer_running = 1;  // Timer will start
            
            // Just start the timer - SCTimer handles everything else
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
    
    // Başlangıçta LED KAPALI olmalı
    // Alakart'ta LED mantığı: OUT2 = HIGH (1) ise LED KAPALI, OUT2 = LOW (0) ise LED AÇIK
    // OUT2'yi HIGH (1) yaparak LED'i kapatıyoruz
    SCT0->OUTPUT |= (1 << 2);  // Set bit 2 (OUT2) to HIGH - LED OFF
    
    // Create the match event for counter L
    SCTIMER_CreateAndScheduleEvent(SCT0,
                                 kSCTIMER_MatchEventOnly,
                                 matchValueL,
                                 0,
                                 kSCTIMER_Counter_L,
                                 &eventCounterL);
    
    // Configure to stop when reaching matchValL (one-shot)
    SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventCounterL);
    
    // Set event active direction
    SCTIMER_SetupEventActiveDirection(SCT0,
                                    kSCTIMER_ActiveIndependent,
                                    eventCounterL);
    
    // Initial state: LED is OFF, first button press will turn it ON after 1s
    // LED'i AÇMAK için OUT2'yi CLEAR (0) yapmalıyız
    SCTIMER_SetupOutputClearAction(SCT0, kSCTIMER_Out_2, eventCounterL);
    
    // Enable interrupt for this event
    SCTIMER_EnableInterrupts(SCT0, (1 << eventCounterL));
    
    // Enable SCTimer interrupt in NVIC
    EnableIRQ(SCT0_IRQn);
    
    // Note: The timer will be started when button is pressed in main()
}

// SCTimer interrupt handler
// This handler is called when the timer reaches matchValueL
// It toggles the state and prepares for the next button press
void SCT0_IRQHandler(void)
{
    // Check if our event triggered the interrupt
    if (SCTIMER_GetStatusFlags(SCT0) & (1 << eventCounterL))
    {
        // Clear the interrupt flag
        SCTIMER_ClearStatusFlags(SCT0, (1 << eventCounterL));
        
        // Timer has stopped (one-shot mode), mark it as not running
        timer_running = 0;
        
        // Toggle state for next button press
        state = 1 - state;
        
        // Reset counter to 0 for next time
        SCT0->CTRL |= (1 << 3);  // Set CLRCTR_L bit
        
        // Clear previous output actions
        SCT0->OUT[2].CLR = 0;
        SCT0->OUT[2].SET = 0;
        
        // Recreate the event
        SCTIMER_CreateAndScheduleEvent(SCT0,
                                     kSCTIMER_MatchEventOnly,
                                     matchValueL,
                                     0,
                                     kSCTIMER_Counter_L,
                                     &eventCounterL);
        
        // Configure to stop when reaching matchValL
        SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventCounterL);
        
        // Set event active direction
        SCTIMER_SetupEventActiveDirection(SCT0,
                                        kSCTIMER_ActiveIndependent,
                                        eventCounterL);
        
        // Setup the action for the NEXT button press
        if (state == 0) {
            // Next: LED will turn ON (CLEAR OUT2)
            SCTIMER_SetupOutputClearAction(SCT0, kSCTIMER_Out_2, eventCounterL);
        }
        else {
            // Next: LED will turn OFF (SET OUT2)
            SCTIMER_SetupOutputSetAction(SCT0, kSCTIMER_Out_2, eventCounterL);
        }
        
        // Re-enable interrupt for next time
        SCTIMER_EnableInterrupts(SCT0, (1 << eventCounterL));
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
