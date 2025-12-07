

// This is an example program where the timer is used to directly initiate an
// ADC conversion sequence.
// At the end of the sequence, the ADC triggers the
//  "ADC0 Sequence A conversion complete interrupt" and the corresponding ISR
//  prints out the conversion result to the terminal.

// AO 2023

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "fsl_adc.h"
#include "fsl_sctimer.h"
#include "fsl_clock.h"
#include "fsl_power.h"
#include "fsl_swm.h"
#include "fsl_syscon.h"
#include <stdint.h>

#define ADC_CHANNEL 1U  // Channel 1 will be used in this example.

#define ADC_CLOCK_DIVIDER 1U // See Fig 52. ADC clocking in Ref Manual.

#define USART_INSTANCE   0U    // Use USART0 for PRINTF
#define USART_BAUDRATE 115200  // Set USART0 Baud rate to 115200.

#define CORE_CLOCK   30000000U  // Set CPU Core clock frequency (Hz)

#define PWM_FREQUENCY_HZ      10000U   // 10 kHz


// The pointer and flag are global so that ISR can manipulate them:
adc_result_info_t *volatile ADCResultPtr; 
volatile bool ADCConvCompleteFlag; //Global flag for signalling between main and ISR

void clock_init(void);
status_t uart_init(void);
void adc_init(void);
void ADC_Configuration(adc_result_info_t * ADCResultStruct);
void SCT_Configuration(void);
int result1 = 0;
int main(void) {
  
  uint32_t frequency = 0U;
  adc_result_info_t ADCResultStruct;

  // The global pointer is made to point to this local variable
  // &ADCResultStruct is the memory address where ADCResultStruct is kept.
  //  ADCResultPtr is the memory address.
  //  *ADCResultPtr is the content of the memory address where ADCResultStruct is kept.
  ADCResultPtr = &ADCResultStruct;

  InitPins();
  clock_init();
  uart_init();
  
  PRINTF("ADC interrupt example.\r\n");

  SCT_Configuration();  // Initialize SCT timer for periodic timing.

  adc_init();      // Power-on and calibration of ADC

  ADC_Configuration(&ADCResultStruct);    // Configure ADC and operation mode.
  
  // Enable the interrupt the for Sequence A Conversion Complete:
  ADC_EnableInterrupts(ADC0, kADC_ConvSeqAInterruptEnable); // Within ADC0
  NVIC_EnableIRQ(ADC0_SEQA_IRQn);                           // Within NVIC
  
  PRINTF("Configuration Done.\r\n\n");

  
    /*
     * The main loop is completely empty.
     * All ADC conversion is handled by the hardware.
     *
     * ADC0 conversion is triggered by the hardware: SCT OUTPUT 3 event
     * When SCT OUTPUT3 changes, the conversion of Sequence A starts.
     *
     * When the conversion is complete,
     *   SEQA_INT (Sequence A conversion complete INT) is triggered.
     * This calls ADC0_SEQA_IRQHandler function which finally prints out
     *  the conversion result to the serial port (and to the terminal screen.)
     *
     * This has two advantages:
     * 1. The main loop is free to do other tasks.
     * 2. The sampling time of the analog channels is precise.
     *
    */
    uint32_t sctimerClock = CORE_CLOCK;   // 30 MHz

    // 4) SCTimer konfig
    sctimer_config_t sctimerConfig;
    SCTIMER_GetDefaultConfig(&sctimerConfig);
    SCTIMER_Init(SCT0, &sctimerConfig);

    // 5) PWM parametreleri (tek çıkış)
    sctimer_pwm_signal_param_t ledPwmParam;
    ledPwmParam.output           = kSCTIMER_Out_4;        // OUT4 → LED pini
    ledPwmParam.level            = kSCTIMER_HighTrue;     // HIGH iken LED aktif
    ledPwmParam.dutyCyclePercent = result1;      // %10 duty

    uint32_t event0;

    // 6) PWM’i ayarla (edge-aligned veya center fark etmez, ben edge kullandım)
    SCTIMER_SetupPwm(SCT0,
                     &ledPwmParam,
                     kSCTIMER_EdgeAlignedPwm,
                     PWM_FREQUENCY_HZ,
                     sctimerClock,
                     &event0);

    // 7) Timer’ı başlat
    SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_U);
  while (1) {
    
  } 

  
} // END: main()



//ISR for ADC conversion sequence A done.
void ADC0_SEQA_IRQHandler(void) {
    if (kADC_ConvSeqAInterruptFlag ==
	(kADC_ConvSeqAInterruptFlag & ADC_GetStatusFlags(ADC0))) {

      ADC_GetChannelConversionResult(ADC0, ADC_CHANNEL, ADCResultPtr);

      ADC_ClearStatusFlags(ADC0, kADC_ConvSeqAInterruptFlag);
	  result1 = 0.02442*(ADCResultPtr->result);
      PRINTF("Ch %d result = %d    \r", // See below for PRINTF usage in an ISR.
	     ADCResultPtr->channelNumber,
	     result1);

      /*
      // ignore this part. It is for demo using SerialPlot:
      PRINTF("%d\r\n", // See below for PRINTF usage in an ISR.
	     //ADCResultPtr->channelNumber,
	     ADCResultPtr->result);
      */
      
      // ADCConvCompleteFlag is not used in this program.
      //However if any task in the main loop must be informed of the
      //  ADC conversion, that task can check this flag;
      //  when the flag is true,  the task can use the conversion result:
      ADCConvCompleteFlag = true; 
    }
}

// Usage of long functions in an ISR:
// Note that in general an ISR must be written to complete and exit
// as quickly as possible.
// PRINTF is a function that may take a long time to execute.
// So it is not advisable to use PRINTF in an ISR.
// However, PRINTF is used in an ISR here to
//  emphasize that the main loop is not doing anything.




// ADC clock and power are turned on and initiali calibration is performed.
void adc_init(void){

  uint32_t frequency = 0U;

  CLOCK_EnableClock(kCLOCK_Adc);      // Enable ADC clock
  
  POWER_DisablePD(kPDRUNCFG_PD_ADC0); // Power on ADC0
    
  // Hardware calibration is required after each chip reset.
  // See: Sec. 21.3.4 Hardware self-calibration
  frequency = CLOCK_GetFreq(kCLOCK_Irc);

  if (true == ADC_DoSelfCalibration(ADC0, frequency)) {
    PRINTF("ADC Calibration Done.\r\n");
  } else {
    PRINTF("ADC Calibration Failed.\r\n");
  }
  
}


// Configure and initialize the ADC
void ADC_Configuration(adc_result_info_t * ADCResultStruct) {

  adc_config_t adcConfigStruct;
  adc_conv_seq_config_t adcConvSeqConfigStruct;
  
  adcConfigStruct.clockDividerNumber = ADC_CLOCK_DIVIDER; // Defined above.
  adcConfigStruct.enableLowPowerMode = false;
  // See Sec. 21.6.11 A/D trim register (voltage mode):
  adcConfigStruct.voltageRange = kADC_HighVoltageRange;
  
  ADC_Init(ADC0, &adcConfigStruct); // Initialize ADC0 with this structure.
  
  // Insert this channel in Sequence A, and set conversion properties:
  // See Sec: 21.6.2 A/D Conversion Sequence A Control Register
  adcConvSeqConfigStruct.channelMask = (1U << ADC_CHANNEL); 

  // Triggered by SCT OUT3 event. See Table 277. "ADC hardware trigger inputs":
  adcConvSeqConfigStruct.triggerMask      = 3U;
  
  adcConvSeqConfigStruct.triggerPolarity  = kADC_TriggerPolarityPositiveEdge;
  adcConvSeqConfigStruct.enableSingleStep = false;
  adcConvSeqConfigStruct.enableSyncBypass = false;
  adcConvSeqConfigStruct.interruptMode    = kADC_InterruptForEachSequence;
  
  // Initialize the ADC0 with the sequence defined above:
  ADC_SetConvSeqAConfig(ADC0, &adcConvSeqConfigStruct);
  
  ADC_EnableConvSeqA(ADC0, true); // Enable the conversion sequence A.
  
  // Make the first ADC conversion so that
  //  the result register has a sensible initial value.
  ADC_DoSoftwareTriggerConvSeqA(ADC0);
  
  while (!ADC_GetChannelConversionResult(ADC0, ADC_CHANNEL, ADCResultStruct))
    { }
  
  ADC_GetConvSeqAGlobalConversionResult(ADC0, ADCResultStruct);
}




void SCT_Configuration(void){

  
  sctimer_config_t sctimerConfig;
  uint32_t eventCounterL;
  uint16_t matchValueL;
  
  CLOCK_EnableClock(kCLOCK_Sct);      // Enable clock of sct.

  SCTIMER_GetDefaultConfig(&sctimerConfig);
  
  // Set the configuration struct for the timer:
  // For more information, see:  Xpresso_SDK/devices/LPC824/drivers/fsl_sctimer.h
  sctimerConfig.enableCounterUnify = false; // Use as two 16 bit timers.
  
  sctimerConfig.clockMode = kSCTIMER_System_ClockMode; // Use system clock as SCT input


  matchValueL= 5000; // This is in: 16.6.20 SCT match registers 0 to 7
  sctimerConfig.enableBidirection_l= false; // Use as single directional register.
  // Prescaler is 8 bit, in: CTRL. See: 16.6.3 SCT control register
  sctimerConfig.prescale_l = 249; // Thi value +1 is used.

  
  SCTIMER_Init(SCT0, &sctimerConfig);    // Initialize SCTimer module
  

  // Configure the low side counter.
  // Schedule a match event for the 16-bit low counter:
  SCTIMER_CreateAndScheduleEvent(SCT0,
				 kSCTIMER_MatchEventOnly,
				 matchValueL,
				 0,    // Not used for "Match Only"
				 kSCTIMER_Counter_L,
				 &eventCounterL);

  // TODO: Rather than toggle, it should set the output:
  // Toggle output_3 when the 16-bit low counter event occurs:
  SCTIMER_SetupOutputToggleAction(SCT0, kSCTIMER_Out_3, eventCounterL);
  
  // Reset Counter L when the 16-bit low counter event occurs
  SCTIMER_SetupCounterLimitAction(SCT0, kSCTIMER_Counter_L, eventCounterL);
  
  // Setup the 16-bit low counter event active direction
  //  See fsl_sctimer.h
  SCTIMER_SetupEventActiveDirection(SCT0,
				    kSCTIMER_ActiveIndependent,
				    eventCounterL);
  
  
  // Start the 16-bit low counter
  SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_L);
}





// Configure and initialize UART0.
// Also configures and initializes the PRINTF function
//   to use the serial port as an output device (there is no screen.)
status_t uart_init(void) {

  uint32_t uart_clock_freq;
  status_t result;

  uart_clock_freq=CLOCK_GetMainClkFreq();//Determine UART input clock frequency.

  CLOCK_EnableClock(kCLOCK_Uart0);               // Enable clock of UART0.
  CLOCK_SetClkDivider(kCLOCK_DivUsartClk, 1U);   // Set prescaler of UART0.
  RESET_PeripheralReset(kUART0_RST_N_SHIFT_RSTn);// Reset UART0

  // See:
  //Xpresso_SDK/devices/LPC824/utilities/debug_console_lite/fsl_debug_console.c
  result = DbgConsole_Init(USART_INSTANCE,
			   USART_BAUDRATE,
			   kSerialPort_Uart,
			   uart_clock_freq);
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

  /*
  // The following is for convenience and not necessary. AO.
  // It outputs the system clock on Pin 26
  //    so that we can check using an oscilloscope:

  // First activate the clock out function:
  SYSCON->CLKOUTSEL = (uint32_t)3; //set CLKOUT source to main clock.
  SYSCON->CLKOUTUEN = 0UL;
  SYSCON->CLKOUTUEN = 1UL;
  // Divide by a reasonable constant so that it is easy to view on an oscilloscope:
  SYSCON->CLKOUTDIV = 100;

  // Using the switch matrix, connect clock out to Pin 26:
  CLOCK_EnableClock(kCLOCK_Swm);     // Enables clock for switch matrix.
  SWM_SetMovablePinSelect(SWM0, kSWM_CLKOUT, kSWM_PortPin_P0_26);
  CLOCK_DisableClock(kCLOCK_Swm); // Disable clock for switch matrix.
  */
}
