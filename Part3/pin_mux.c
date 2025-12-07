
#include "fsl_common.h"
#include "fsl_iocon.h"
#include "fsl_swm.h"
#include "pin_mux.h"

void InitPins(void) {

  uint32_t IOCON_PIO_config;// This is the struct for keeping the GPIO pin conf
                   // See: User Manual Section 8.4.1 Pin Configuration & Fig.10
  
  
  CLOCK_EnableClock(kCLOCK_Iocon);  // Enable IOCON clock
  
  CLOCK_EnableClock(kCLOCK_Swm);  // Enable SWM clock



    /////////////// USART RX and TX pins Configuration /////////////////////////////////

  // Configure PIO_0, (Alakart label: P0) as USART RXD
  // IC package physical pin is: 24
  // GPIO pin name: PIO0_0
  // Peripheral: USART, signal: RXD,
  
  // First configure the GPIO pin characteristics:
  IOCON_PIO_config = (IOCON_PIO_MODE_PULLUP |  // Select pull-up function
		      IOCON_PIO_HYS_DI |       // Disable hysteresis
		      IOCON_PIO_INV_DI |       // Do not invert input
		      IOCON_PIO_OD_DI |        // Disable open-drain function
		      IOCON_PIO_SMODE_BYPASS | // Bypass the input filter
		      IOCON_PIO_CLKDIV0);      // IOCONCLKDIV = 0

  // PIO0 PIN0 (pin no 24) is configured as USART0, RXD.
  IOCON_PinMuxSet(IOCON, IOCON_INDEX_PIO0_0, IOCON_PIO_config);
  
  // Connect USART0_RXD to P0_0
  SWM_SetMovablePinSelect(SWM0, kSWM_USART0_RXD, kSWM_PortPin_P0_0);

  

  IOCON_PIO_config = (IOCON_PIO_MODE_PULLUP |  // Select pull-up function
		      IOCON_PIO_HYS_DI |       // Disable hysteresis
		      IOCON_PIO_INV_DI |       // Do not invert input
		      IOCON_PIO_OD_DI |        // Disable open-drain function
		      IOCON_PIO_SMODE_BYPASS | // Bypass the input filter
		      IOCON_PIO_CLKDIV0);
  // PIO0_4 (pin no 4) is configured with these properties.
  IOCON_PinMuxSet(IOCON, IOCON_INDEX_PIO0_4, IOCON_PIO_config);

  // Connect USART0_TXD to PIO0_4 
  SWM_SetMovablePinSelect(SWM0, kSWM_USART0_TXD, kSWM_PortPin_P0_4);
  
  /////////////// ADC CH1 Configuration /////////////////////////////////

  // Configure PIO_6, (Alakart label: P6)  as : ADC CH 1
  // IC package physical pin is: 23,
  // GPIO pin name: PIO0_6
  // Peripheral: ADC, signal: CHN1
  
  IOCON_PIO_config = (//Deactivate pull-up
		  IOCON_PIO_HYS_DI |      // Disable hysteresis
		  IOCON_PIO_INV_DI |      // Do not invert if input
		  IOCON_PIO_OD_DI |       // Disable open-drain
		  IOCON_PIO_SMODE_BYPASS |// Bypass input filter
		  IOCON_PIO_CLKDIV0);     // IOCONCLKDIV0
  // Configure PIO0_6 (pin no 23)  with these properties.
  IOCON_PinMuxSet(IOCON, IOCON_INDEX_PIO0_6, IOCON_PIO_config);
  
  // Enable analog functionality on PIO0_6:
  SWM_SetFixedPinSelect(SWM0, kSWM_ADC_CHN1, true);


  
  // Disable SWM clock since configuration is complete
  CLOCK_DisableClock(kCLOCK_Swm);
}
