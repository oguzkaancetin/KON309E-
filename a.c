//Experiment 2

#include <stdint.h> // Declarations of uint32_t etc.
#include "lpc824.h" // Declarations of LPC824 register names.
#include "pin_mux.h"
#include "fsl_sctimer.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"
#include "fsl_power.h"




volatile int state = 0;


void clock_init(void);
uint32_t eventCounterL, eventCounterH; // The event number for counter L and H
uint16_t matchValueL, matchValueH;
#define CORE_CLOCK   6000000U
void SCTimerL_init(void);


void SCTimerL_init(void){
  sctimer_config_t sctimerConfig;
   InitPins();                           // Init board pins.
  clock_init();                         // Initialize processor clock.
  CLOCK_EnableClock(kCLOCK_Sct);        // Enable clock of sct.
  SCTIMER_GetDefaultConfig(&sctimerConfig);
  sctimerConfig.enableCounterUnify = false; // Use as two 16 bit timers.
  sctimerConfig.clockMode = kSCTIMER_System_ClockMode; // Use system clock as SCT input

  sctimerConfig.enableBidirection_l= false; // Use as single directional timer.
  sctimerConfig.enableBidirection_h= false; // Use as single directional timer.

  // Prescaler is 8 bit, in: CTRL. See: 16.6.3 SCT control register
  sctimerConfig.prescale_l = 249; // This value +1 is used.
  sctimerConfig.prescale_h = 249; // Becomes 250.

  SCTIMER_Init(SCT0, &sctimerConfig);    // Initialize SCTimer module
   matchValueL= 60000; // This is in: 16.6.20 SCT match registers 0 to 7
   matchValueH= 30000; // They are 16 bit values (for an 16 bit counter)

if (GPIO_B15 == 1) // when button 1 is pressed
    {
        SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
        SCTIMER_SetupCounterStopAction(SCT0, kSCTIMER_Counter_L, eventCounterL);

if (state==0){
            SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_2, eventCounterL);
}
        if(state==1){
            SCTIMER_SetupCounterLimitAction(SCT0, kSCTIMER_Counter_L, eventCounterL);
}
            state++;
            if (state==2){
                state=0;

            }


        }

while (1) {
    // SC Timer is doing all the work.
    // main() can simply do nothing.
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
  config.targetFreq = CORE_CLOCK*2;             // set pll target freq
  CLOCK_InitSystemPll(&config);                 // set parameters
  //CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcIrc);  // Select IRC as main clock source.
  CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll); // Select PLL as main clock source.
  CLOCK_Select(kCLKOUT_From_Irc);               // select IRC for CLKOUT
  CLOCK_SetCoreSysClkDiv(1U);

  // Check processor registers and calculate the
  // Actual clock speed. This is stored in the
  // global variable SystemCoreClock
  SystemCoreClockUpdate ();

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
