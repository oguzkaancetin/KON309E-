#include "pin_mux.h"

#include "fsl_clock.h"
#include "fsl_swm.h"
#include "fsl_swm_connections.h"

void InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_Swm);

    /* Route SCT OUT2 to PIO0_27 for the green LED. */
    SWM_SetMovablePinSelect(SWM0, kSWM_SCT0_OUT2, kSWM_PortPin_P0_27);

    CLOCK_DisableClock(kCLOCK_Swm);
}
