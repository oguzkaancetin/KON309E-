/* Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017, 2020 NXP
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "fsl_pint.h"
#include "fsl_power.h"
#include "fsl_clock.h"
#include "fsl_syscon.h"


#define USART_INSTANCE   0U
#define USART_BAUDRATE 115200

#define CORE_CLOCK   30000000U  // Set CPU Core clock frequency (Hz)

void clock_init(void);
status_t uart_init(void);

uint8_t led_state;


///////////  This is the ISR callback for PIN INTERRUPT: ////////////
// The actual ISR is in file fsl_pint.c and has no arguments. 

// The arguments are defined in fsl_pint.c file around line 794
// within the declaration:
//  void PIN_INT0_DriverIRQHandler(void)
// Check that file for more details.
void pint_intr_callback(pint_pin_int_t pintr, uint32_t pmatch_status) {
  PRINTF("\f\r\nPINT Pin Interrupt %d event detected.", pintr);
}


int main(void) {

  CLOCK_EnableClock(kCLOCK_Uart0);               // Enable clock of uart0
  CLOCK_SetClkDivider(kCLOCK_DivUsartClk, 1U);   // Ser DIV of uart0.
  
  InitPins();
  clock_init();
  uart_init();

  
  // Connect PIO_12 as a source to PIN INT 1:
  SYSCON_AttachSignal(SYSCON, kPINT_PinInt1, kSYSCON_GpioPort0Pin12ToPintsel);
  
  PINT_Init(PINT);  // Initialize PIN Interrupts
  
  // Setup Pin Interrupt 1:
  //  falling edge triggers the INT
  //  register the name of the callback function (as the last argument).
  PINT_PinInterruptConfig(PINT,                // Pin INT base register address
			  kPINT_PinInt1,       // Use Pin INT 1
			  kPINT_PinIntEnableFallEdge, // At falling edge.
			  pint_intr_callback); // Name of the callback function
  
  // Enable callbacks for PINT0 by Index:
  // This clears the pending INT flags
  //  and enables the interrupts for this PIN INT.
  PINT_EnableCallbackByIndex(PINT, kPINT_PinInt1);
  
  PRINTF("PINT Pin Interrupt events are configured\r\n");
  PRINTF("Press SW2 to generate events\r\n");
  
  
  while (1) {
    __WFI();  // Wait for interrupt and do nothing.
    // Processor sleeps here.
  }
}


status_t uart_init(void) {

  uint32_t uart_clock_freq;
  status_t result;

  uart_clock_freq=CLOCK_GetMainClkFreq(); // Read UART clock frequency. 

  CLOCK_EnableClock(kCLOCK_Uart0);               // Enable clock of UART0.
  CLOCK_SetClkDivider(kCLOCK_DivUsartClk, 1U);   // Set prescaler of UART0.
  RESET_PeripheralReset(kUART0_RST_N_SHIFT_RSTn);// Reset UART0

  // See:
  //Xpresso_SDK/devices/LPC824/utilities/debug_console_lite/fsl_debug_console.c
  result = DbgConsole_Init(USART_INSTANCE,
			   USART_BAUDRATE,
			   kSerialPort_Uart,
			   uart_clock_freq);
  // assert(kStatus_Success == result);
  return result;

}



// Setup clock source: Internal RC clock with the PLL set to 30MHz frequency:
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

  // The following is for convenience and not necessary. AO.
  // It outputs the system clock on Pin 27
  //    so that we can check using an oscilloscope:

  /*
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
