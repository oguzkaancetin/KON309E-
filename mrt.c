// Copyright (c) 2016, Freescale Semiconductor, Inc.
// Copyright 2016-2017 NXP
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

// Heavily modified from Xpresso SDK driver_examples/mrt
// Ahmet Onat 2023

#include "fsl_mrt.h"
#include "fsl_power.h"
#include "fsl_gpio.h"
#include "fsl_clock.h"
#include "fsl_syscon.h"


#define CORE_CLOCK 30000000U  // Set CPU Core clock frequency (Hz)

#define DESIRED_INT_FREQ 2   // Desired number of INT's per second.

#define LED_PORT 0U
#define LED_PIN 16U          // On Alakart board, there is a LED on this pin.



void clock_init(void);


static volatile bool mrtIsrFlag = false; // MRT ISR sets this flag to true.




int main(void) {
  uint32_t mrt_clock;
  uint32_t mrt_count_val;
  
  mrt_config_t mrtConfig;   // Struct for configuring the MRT:
  gpio_pin_config_t led_pin_conf ={kGPIO_DigitalOutput, 0}; // struct for GPIO

  clock_init();  // Initialize CPU clock to 30MHz


  // Initialize the GPIO pin where the LED is connected:
  CLOCK_EnableClock(kCLOCK_Gpio0);
  GPIO_PinInit(GPIO, LED_PORT, LED_PIN, &led_pin_conf);


  CLOCK_EnableClock(kCLOCK_Mrt);  // Enable clock of mrt.

  MRT_GetDefaultConfig(&mrtConfig);
  
  MRT_Init(MRT0, &mrtConfig);  // Init MRT module to default configuration.

  
  // Setup Channel 0 to periodic INT mode (See Sec. 19.5.1)
  MRT_SetupChannelMode(MRT0, kMRT_Channel_0, kMRT_RepeatMode);

  
  // Query the input clock frequency of MRT
  mrt_clock = CLOCK_GetFreq(kCLOCK_CoreSysClk);

  
  // To get DESIRED_INT_FREQ number of INTs per second, Channel 0 of MRT
  // must count up to: (Input clock frequency)/ (desired int frequency).
  // (Input clock frequency in this case is 60,000,000. For 2 INT per second,
  //  the count value calculates as 30,000,000.)
  // CAUTION: MRT counter is only 24 bits wide. See Sec. 19.6.2 Timer register
  // and 19.6.5 Module Configuration register.
  // However, this still means we can write up to about 167,000,000 here.
  mrt_count_val= mrt_clock/DESIRED_INT_FREQ;
  
  MRT_StartTimer(MRT0, kMRT_Channel_0, mrt_count_val);
  

  // Finally enable MRT interrupts:
  // Enable MRT interrupts for channel 0 (See Sec. 19.6.3 Control register)
  MRT_EnableInterrupts(MRT0, kMRT_Channel_0, kMRT_TimerInterruptEnable);

  
  EnableIRQ(MRT0_IRQn);  // Enable MRT INTs in NVIC.
  // (See Sec. 4.4.1 Interrupt Set Enable Register 0 register, Bit 10)
  


  
  while (1) {

    if (mrtIsrFlag == true) { // This flag is set by the MRT ISR.
      // If set toggle the LED
      GPIO_PortToggle(GPIO, LED_PORT, 1U << LED_PIN);
      // and clear the flag
      mrtIsrFlag    = false;
    }
    
  } // END while (1)
  
} // END main()




///////////////////////////////////////////////////////////////////////
////////// This is the MRT interrupt service routine. /////////////////
///////////////////////////////////////////////////////////////////////

// It was declared in the file startup_LPC824.S
// as the 10th entry of the vector table.
// See Table 5 of Sec. 4.3.1 Interrupt sources

void MRT0_IRQHandler(void) {

  MRT_ClearStatusFlags(MRT0,      // Clear interrupt flag:
		       kMRT_Channel_0,
		       kMRT_TimerInterruptFlag);
  mrtIsrFlag = true;              // Set global variable to inform main().
}




// Setup processor clock source:
// Internal RC clock with the PLL set to 30MHz frequency.
void clock_init(void) {

  // Set up using Internal RC clock (IRC) oscillator:
  POWER_DisablePD(kPDRUNCFG_PD_IRC_OUT);        // Turn ON IRC OUT
  POWER_DisablePD(kPDRUNCFG_PD_IRC);            // Turn ON IRC

  CLOCK_Select(kSYSPLL_From_Irc);               // Connect IRC to PLL input.

  clock_sys_pll_t config;
  config.src = kCLOCK_SysPllSrcIrc;             // Select PLL source as IRC. 
  config.targetFreq = CORE_CLOCK*2;             // set pll target freq

  CLOCK_InitSystemPll(&config);                 // set parameters

  CLOCK_SetMainClkSrc(kCLOCK_MainClkSrcSysPll); // Select PLL as main clock source.
  CLOCK_Select(kCLKOUT_From_Irc);               // select IRC for CLKOUT
  CLOCK_SetCoreSysClkDiv(1U);

  // Check processor registers and calculate the
  // Actual clock speed. This is stored in the
  // global variable SystemCoreClock
  SystemCoreClockUpdate ();

  /*
  // The following is for convenience and not necessary. AO.
  // It outputs the system clock frequency on Pin 27
  //    so that we can check using an oscilloscope:
  
  // First activate the clock out function:
  SYSCON->CLKOUTSEL = (uint32_t)3; //set CLKOUT source to main clock.
  SYSCON->CLKOUTUEN = 0UL;
  SYSCON->CLKOUTUEN = 1UL;
  // Divide by a reasonable constant so that it is easy to view on an oscilloscope:
  //SYSCON->CLKOUTDIV = 100;
  SYSCON->CLKOUTDIV = 2000; 

  // Using the switch matrix, connect clock out to Pin 27:
  CLOCK_EnableClock(kCLOCK_Swm);     // Enables clock for switch matrix.
  SWM_SetMovablePinSelect(SWM0, kSWM_CLKOUT, kSWM_PortPin_P0_27);
  CLOCK_DisableClock(kCLOCK_Swm); // Disable clock for switch matrix.
  */
}




