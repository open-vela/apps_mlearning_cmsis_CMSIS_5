/*-----------------------------------------------------------------------------
 *      Name:         RV_GenWait.c 
 *      Purpose:      CMSIS RTOS validation tests implementation
 *-----------------------------------------------------------------------------
 *      Copyright(c) KEIL - An ARM Company
 *----------------------------------------------------------------------------*/
#include <string.h>
#include "RV_Framework.h"
#include "cmsis_rv.h"

/*-----------------------------------------------------------------------------
 *      Test implementation
 *----------------------------------------------------------------------------*/
static volatile osStatus Stat_Isr;

void GenWait_IRQHandler (void);


/*-----------------------------------------------------------------------------
 *      Default IRQ Handler
 *----------------------------------------------------------------------------*/
void GenWait_IRQHandler (void) {
  
  switch (ISR_ExNum) {
    case 0: Stat_Isr = osDelay (10); break;
   #ifdef osFeatureWait
    case 1: Stat_Isr = osWait  (10); break;
   #endif
  }
}

/*-----------------------------------------------------------------------------
 *      Test cases
 *----------------------------------------------------------------------------*/

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\defgroup genwait_funcs Generic Wait Functions
\brief Generic Wait Functions Test Cases
\details
The Generic Wait function group in CMSIS-RTOS provides means for a time delay and allow to wait for unspecified events. The
test cases check the functions osDelay and osWait and call the generic wait functions from an ISR.

@{
*/

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_GenWaitBasic
\details
- Call osDelay and wait until delay is executed
- Call osWait  and wait until delay is executed
- Function calls must return osEventTimeout.
*/
void TC_GenWaitBasic (void) {
  ASSERT_TRUE (osDelay (10) == osEventTimeout);
 #ifdef osFeatureWait
  ASSERT_TRUE (osWait  (10) == osEventTimeout);
 #endif
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_GenWaitInterrupts
\details
- Call generic wait functions from the ISR
*/
void TC_GenWaitInterrupts (void) {
  
  TST_IRQHandler = GenWait_IRQHandler;
  
  NVIC_EnableIRQ((IRQn_Type)0);
  
  ISR_ExNum = 0; /* Test: osDelay */
  Stat_Isr = osOK;
  SetPendingIRQ((IRQn_Type)0);
  ASSERT_TRUE (Stat_Isr == osErrorISR);

 #ifdef osFeatureWait
  ISR_ExNum = 1; /* Test: osWait */
  Stat_Isr = osOK;
  SetPendingIRQ((IRQn_Type)0);
  ASSERT_TRUE (Stat_Isr == osErrorISR);
 #endif
  
  NVIC_DisableIRQ((IRQn_Type)0);
}

/**
@}
*/ 
// end of group genwait_funcs

/*-----------------------------------------------------------------------------
 * End of file
 *----------------------------------------------------------------------------*/
