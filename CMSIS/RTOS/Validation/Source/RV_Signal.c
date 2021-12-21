/*-----------------------------------------------------------------------------
 *      Name:         RV_Signal.c 
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

/* Definitions for signaling thread */
#define TST_SIG_CHILD 1                 /* Test child thread signaling        */
#define TST_SIG_ISR   2                 /* Test ISR signaling                 */

static osThreadId Var_ThreadId;


/* Definitions for TC_SignalChildThread */
void Th_ChildSignals (void const *arg);
osThreadDef (Th_ChildSignals, osPriorityNormal, 1, 0);


/* Definitions for TC_SignalChildToParent */
#define SIG_FLAG_MSK  ((1U << osFeature_Signals) - 1) /* Signal flag mask     */

void Th_Sig (void const *arg);          /* Signaling thread prototype         */
osThreadDef (Th_Sig, osPriorityNormal, 1, 200);
static osThreadId ThId_Sig;

void Th_Sig_Wakeup (void const *arg);   /* Signaling thread prototype         */
osThreadDef (Th_Sig_Wakeup, osPriorityBelowNormal, 1, 0);

/* Definitions for TC_SignalChildToChild */
void Th_Sig_Child_0 (void const *arg);
void Th_Sig_Child_1 (void const *arg);

osThreadDef (Th_Sig_Child_0, osPriorityBelowNormal, 1, 0);
osThreadDef (Th_Sig_Child_1, osPriorityBelowNormal, 1, 0);

/* Definitions for TC_SignalWaitTimeout */
void Th_SignalSet (void const *arg);

osThreadDef (Th_SignalSet, osPriorityNormal, 1, 0);


/* Definitions for signal management test in the ISR */

static volatile int32_t Sign_Isr;                       /* Signal flags obtained by the ISR   */
static volatile osEvent Evnt_Isr;                       /* Event  flags obtained by the ISR   */

void Signal_IRQHandler (void);

/*-----------------------------------------------------------------------------
 * Signaling thread
 *----------------------------------------------------------------------------*/
void Th_Sig (void const *arg) {
  uint32_t typ = *(uint32_t const *)arg;
  osEvent evt;
  int32_t flag;

  flag = 0x00000001;
  switch (typ) {
    case TST_SIG_CHILD:
      do {
        /* Wait 100ms for signals from the main thread */
        evt = osSignalWait (flag, 100);
        ASSERT_TRUE (evt.status == osEventSignal);
        
        if (evt.status == osEventSignal) {
          ASSERT_TRUE ((evt.value.signals & flag) != 0);
          
          if ((uint32_t)flag <= SIG_FLAG_MSK) {
            /* Send signal back to the main thread */
            ASSERT_TRUE (osSignalSet (Var_ThreadId, flag) != (int32_t)0x80000000);
            flag <<= 1;
          }
        }
      }
      while ((evt.status == osEventSignal) && ((uint32_t)flag <= SIG_FLAG_MSK));
      
      /* Wait until terminated */
      osSignalWait (SIG_FLAG_MSK, osWaitForever);
      break;
    
    case TST_SIG_ISR:
      while (flag);
      break;
  }
}

/*-----------------------------------------------------------------------------
 *      Wakeup thread
 *----------------------------------------------------------------------------*/
void Th_Sig_Wakeup (void const __attribute__((unused)) *arg)
{
  osDelay(10);
  /* Send signal back to the main thread */
  ASSERT_TRUE (osSignalSet (Var_ThreadId, 0x01) != (int32_t)0x80000000);
}

/*-----------------------------------------------------------------------------
 *      Default IRQ Handler
 *----------------------------------------------------------------------------*/
void Signal_IRQHandler (void) {
  switch (ISR_ExNum) {
    case 0: Sign_Isr = osSignalSet   (Var_ThreadId, 0x00000001);  break;
    case 1: Sign_Isr = osSignalClear (Var_ThreadId, 0x00000001);  break;
    case 2: Evnt_Isr = osSignalWait  (0x00000001,   0);           break;
    case 3: Evnt_Isr = osSignalWait  (0x00000001, 100);           break;
    case 4: Evnt_Isr = osSignalWait  (0x00000001, osWaitForever); break;
  }
}

/*-----------------------------------------------------------------------------
 * Signal setting thread
 *----------------------------------------------------------------------------*/
void Th_SignalSet (void const *arg) {
  int32_t flags = *(int32_t const *)arg;
  osSignalSet (Var_ThreadId, flags);
}

/*-----------------------------------------------------------------------------
 * Child signals thread
 *----------------------------------------------------------------------------*/
void Th_ChildSignals (void const __attribute__((unused)) *arg) {
  int32_t flags, set, clr;
  osThreadId t_id = osThreadGetId();
  
  ASSERT_TRUE (t_id != NULL);
  
  if (t_id != NULL) {
    /* - Set all signal flags */
    flags = 0x00000000;
    set   = 0x00000001;
    do {
      ASSERT_TRUE (osSignalSet (t_id, set) == flags);
      flags = (flags << 1) | 1;
      set <<= 1;
    }
    while ((uint32_t)set <= SIG_FLAG_MSK);

    /* MSB flag cannot be set by user */
    ASSERT_TRUE (osSignalSet (t_id, (int32_t)0x80000000) == (int32_t)0x80000000);
    
    /* - Clear all signal flags */
    flags = SIG_FLAG_MSK;
    clr   = (1U << (osFeature_Signals - 1));
    do {
      ASSERT_TRUE (osSignalClear (t_id, clr) == flags);
      clr   >>= 1;
      flags >>= 1;
    }
    while (clr);
  }
}

/*-----------------------------------------------------------------------------
 * Signal to Child 0 thread
 *----------------------------------------------------------------------------*/
void Th_Sig_Child_0 (void const *arg) {
  int32_t     flags;
  osEvent     evt;
  osThreadId const *id = (osThreadId const *)arg;
  
  if (id != NULL) {
    flags = 1;
    do {
      osSignalSet (*id, flags);
      /* - Wait for signals sent from Child 1 */
      evt = osSignalWait (flags, 100);
      ASSERT_TRUE (evt.status == osEventSignal);

      if (evt.status == osEventSignal) {
        ASSERT_TRUE (evt.value.signals == flags);
      }
      flags <<= 1;
      flags  |= (flags & 2) ? (0) : (1);
    }
    while ((evt.status == osEventSignal) && ((uint32_t)flags <= SIG_FLAG_MSK));
  }
  osSignalSet (Var_ThreadId, 0x01);
}

/*-----------------------------------------------------------------------------
 * Signal to Child 1 thread
 *----------------------------------------------------------------------------*/
void Th_Sig_Child_1 (void const *arg) {
  int32_t     flags;
  osEvent     evt;
  osThreadId const *id = (osThreadId const *)arg;

  if (id != NULL) {
    flags = 1;
    do {
      /* - Wait for signals sent from sender */
      evt = osSignalWait (flags, 100);
      ASSERT_TRUE (evt.status == osEventSignal);
      
      if (evt.status == osEventSignal) {
        ASSERT_TRUE (evt.value.signals == flags);
      }
      
      /* - Send received signal mask back to sender */
      osSignalSet (*id, flags);
      
      flags <<= 1;
      flags  |= (flags & 2) ? (0) : (1);
    }
    while ((evt.status == osEventSignal) && ((uint32_t)flags <= SIG_FLAG_MSK));
  }
  osSignalSet (Var_ThreadId, 0x02);
}

/*-----------------------------------------------------------------------------
 *      Test cases
 *----------------------------------------------------------------------------*/

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\defgroup signal_funcs Signal Functions
\brief Signal Functions Test Cases
\details
The test cases check the osSignal* functions.

@{
*/

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalMainThread
\details
- Set all signal flags for the main thread
- Clear all signal flags for the main thread
*/
void TC_SignalMainThread (void) {
  int32_t flags, set, clr;
  osThreadId t_id = osThreadGetId();
  
  ASSERT_TRUE (t_id != NULL);
  
  if (t_id != NULL) {
    /* Clear signal flags */
    osSignalClear (t_id, SIG_FLAG_MSK);
    
    /* Set all signal flags */
    flags = 0x00000000;
    set   = 0x00000001;
    do {
      ASSERT_TRUE (osSignalSet (t_id, set) == flags);
      flags = (flags << 1) | 1;
      set <<= 1;
    }
    while ((uint32_t)set <= SIG_FLAG_MSK);
        
    /* Clear all signal flags */
    flags = SIG_FLAG_MSK;
    clr   = (1U << (osFeature_Signals - 1));
    do {
      ASSERT_TRUE (osSignalClear (t_id, clr) == flags);
      clr   >>= 1;
      flags >>= 1;
    }
    while (clr);
  }
}


/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalChildThread
\details
- Create a child thread
  Within child thread: 
- Verify that all signals are cleared
- Set all signal flags
- Verify that all signal flags are set
- Clear all signal flags
- Verify that all signal flags are cleared
*/
void TC_SignalChildThread (void) {
  ASSERT_TRUE (osThreadCreate (osThread(Th_ChildSignals), NULL) != NULL);
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalChildToParent
\details
- Create signaling thread
- Send signal to the signaling thread
*/
void TC_SignalChildToParent (void) {
  osEvent  evt;  
  int32_t  flag;
  uint32_t arg  = TST_SIG_CHILD;
  
  /* Get main thread ID */
  Var_ThreadId = osThreadGetId();
  ASSERT_TRUE (Var_ThreadId != NULL);
  
  if (Var_ThreadId != NULL) {
    /* Create signaling thread */
    ThId_Sig = osThreadCreate (osThread (Th_Sig), &arg);
    ASSERT_TRUE (ThId_Sig != NULL);
    
    if (ThId_Sig != NULL) {
      flag = 0x00000001;
      do {
        /* Send signal to the signaling thread */
        ASSERT_TRUE (osSignalSet (ThId_Sig, flag) != (int32_t)0x80000000);
        if ((uint32_t)flag <= SIG_FLAG_MSK) {
          evt = osSignalWait (flag, 100);
          ASSERT_TRUE (evt.status == osEventSignal);
        
          if (evt.status == osEventSignal) {
            ASSERT_TRUE ((evt.value.signals & flag) != 0);
          }
        }
        flag <<= 1;
      }
      while((uint32_t)flag <= SIG_FLAG_MSK);
      /* Terminate signaling thread */
      ASSERT_TRUE (osThreadTerminate (ThId_Sig) == osOK);
    }
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalChildToChild
\details
- Create two child threads
  Within child thread: 
- Verify that all signals are cleared
- Set all signal flags
- Verify that all signal flags are set
- Clear all signal flags
- Verify that all signal flags are cleared
*/
void TC_SignalChildToChild (void) {
  osEvent    evt;
  osThreadId id[2];
  
  Var_ThreadId = osThreadGetId();
  ASSERT_TRUE (Var_ThreadId != NULL);
  
  if (Var_ThreadId != NULL) {
    id[0] = osThreadCreate (osThread(Th_Sig_Child_0), &id[1]);
    id[1] = osThreadCreate (osThread(Th_Sig_Child_1), &id[0]);
    
    ASSERT_TRUE (id[0] != NULL);
    ASSERT_TRUE (id[1] != NULL);
    
    /* - Wait for signals from child threads */
    evt = osSignalWait (0x03, 100);
    ASSERT_TRUE (evt.status == osEventSignal);
    
    if (evt.status == osEventSignal) {
      ASSERT_TRUE ((evt.value.signals & 0x03) == 0x03);
    }
    
    osThreadTerminate(id[0]);
    osThreadTerminate(id[1]);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalWaitTimeout
\details
- Clear all signals for current thread
- Wait for any single signal without timeout
- Wait for any single signal with timeout
- Wait for all signals without timeout
- Wait for all signals with timeout
- Wait for various signal masks from a signaling thread
*/
void TC_SignalWaitTimeout (void) {
  osThreadId id;
  int32_t    flags;

  Var_ThreadId = osThreadGetId();
  ASSERT_TRUE (Var_ThreadId != NULL);
  
  if (Var_ThreadId != NULL) {
    /* - Clear all signals for current thread */
    osSignalClear (Var_ThreadId, SIG_FLAG_MSK);

    /* - Wait for any single signal without timeout */
    ASSERT_TRUE (osSignalWait (0, 0).status == osOK);
    /* - Wait for any single signal with timeout */
    ASSERT_TRUE (osSignalWait (0, 5).status == osEventTimeout);
    /* - Wait for all signals without timeout */
    ASSERT_TRUE (osSignalWait (SIG_FLAG_MSK, 0).status == osOK);
    /* - Wait for all signals with timeout */
    ASSERT_TRUE (osSignalWait (SIG_FLAG_MSK, 5).status == osEventTimeout);
    
    /* - Create a signal setting thread */
    flags = 3;
    id = osThreadCreate (osThread (Th_SignalSet), &flags);
    ASSERT_TRUE (id != NULL);
    
    if (id != NULL) {
      osDelay(5);
    
      /* - Wait for various signal masks from a signaling thread */
      ASSERT_TRUE (osSignalWait (1, 10).status == osEventSignal);
    }    
    osDelay(10);

    /* - Create a signal setting thread */
    flags = 5;
    id = osThreadCreate (osThread (Th_SignalSet), &flags);
    ASSERT_TRUE (id != NULL);
    
    if (id != NULL) {
      osDelay(5);
    
      /* - Wait for various signal masks from a signaling thread */
      ASSERT_TRUE (osSignalWait (2, 10).status == osEventSignal);
    }    
    osSignalClear (Var_ThreadId, SIG_FLAG_MSK);
    osDelay(10);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalCheckTimeout
\details
- Set time thresholds
- Create wakeup thread to set a signal after 10 ticks
- Wait for a signal with a defined timeout
- Check if the signal is raised between the minimum and maximum thresholds
- Wait for a signal with an infinite timeout
- Check if the signal is raised between the minimum and maximum thresholds
*/
void TC_SignalCheckTimeout (void) {
  osEvent  evt;  
  uint32_t t_min, t_max, t_10;
  
  /* Get main thread ID */
  Var_ThreadId = osThreadGetId();
  ASSERT_TRUE (Var_ThreadId != NULL);
  
  if (Var_ThreadId != NULL) {
    /* Set time thresholds */
    t_min = osKernelSysTick();
    osDelay(9);
    t_min = osKernelSysTick() - t_min;

    t_max = osKernelSysTick();
    osDelay(11);
    t_max = osKernelSysTick() - t_max;

    /* Create wakeup thread to set a signal after 10 ticks */
    ThId_Sig = osThreadCreate(osThread(Th_Sig_Wakeup), NULL);
    ASSERT_TRUE (ThId_Sig != NULL);

    if (ThId_Sig != NULL) {
        t_10 = osKernelSysTick();
        evt = osSignalWait (1, 100);
        t_10 = osKernelSysTick() - t_10;
        ASSERT_TRUE (evt.status == osEventSignal);
        ASSERT_TRUE (t_min < t_10);
        ASSERT_TRUE (t_10 < t_max);
    }
    osDelay(10);

    /* Create wakeup thread to set a signal after 10 ticks */
    ThId_Sig = osThreadCreate(osThread(Th_Sig_Wakeup), NULL);
    ASSERT_TRUE (ThId_Sig != NULL);

    if (ThId_Sig != NULL) {
        t_10 = osKernelSysTick();
        evt = osSignalWait (1, osWaitForever);
        t_10 = osKernelSysTick() - t_10;
        ASSERT_TRUE (evt.status == osEventSignal);
        ASSERT_TRUE (t_min < t_10);
        ASSERT_TRUE (t_10 < t_max);
    }
    osDelay(10);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalParam
\details
- Test signal management functions with invalid parameters
*/
void TC_SignalParam (void) {
  osThreadId id;

  ASSERT_TRUE (osSignalSet   (NULL, 0) == (int32_t)0x80000000);
  ASSERT_TRUE (osSignalClear (NULL, 0) == (int32_t)0x80000000);
  
  /* Cannot wait for signal mask with MSB set */
  ASSERT_TRUE (osSignalWait  ((int32_t)0x80000000, 0).status == osErrorValue);

  id = osThreadGetId();
  ASSERT_TRUE (id != NULL);
  if (id != NULL) {
    /* Flag mask with MSB set cannot be set by user */
    ASSERT_TRUE (osSignalSet (id, (int32_t)0x80000000) == (int32_t)0x80000000);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SignalInterrupts
\details
- Call all signal management functions from the ISR
*/
void TC_SignalInterrupts (void) {
  osEvent  event;
  
  TST_IRQHandler = Signal_IRQHandler;
  
  Var_ThreadId = osThreadGetId();
  ASSERT_TRUE (Var_ThreadId != NULL);
  
  if (Var_ThreadId != NULL) {
    /* Clear all main thread signal flags */
    osSignalClear (Var_ThreadId, SIG_FLAG_MSK);

    /* Test signaling between ISR and main thread */
    NVIC_EnableIRQ((IRQn_Type)0);
  
    ISR_ExNum = 0; /* Test: osSignalSet */
    Sign_Isr = (-1);
    SetPendingIRQ((IRQn_Type)0);
    event = osSignalWait (0x00000001, 100);
    ASSERT_TRUE (event.status == osEventSignal);
    ASSERT_TRUE (event.value.signals == 0x00000001);
    ASSERT_TRUE (Sign_Isr == 0x00000000);
    
    ISR_ExNum = 1; /* Test: osSignalClear */
    ASSERT_TRUE (osSignalSet (Var_ThreadId, 0x00000001) == 0);
    Sign_Isr = 0;
    SetPendingIRQ((IRQn_Type)0);
    ASSERT_TRUE (Sign_Isr == (int32_t)0x80000000);
    ASSERT_TRUE (osSignalClear (Var_ThreadId, 0x00000001) == 1);
    
    ISR_ExNum = 2; /* Test: osSignalWait (no timeout) */
    Evnt_Isr.status = osOK;
    SetPendingIRQ((IRQn_Type)0);
    ASSERT_TRUE (Evnt_Isr.status == osErrorISR);

    ISR_ExNum = 3; /* Test: osSignalWait (with timeout) */
    Evnt_Isr.status = osOK;
    SetPendingIRQ((IRQn_Type)0);
    ASSERT_TRUE (Evnt_Isr.status == osErrorISR);
    
    ISR_ExNum = 4; /* Test: osSignalWait (with infinite timeout) */
    Evnt_Isr.status = osOK;
    SetPendingIRQ((IRQn_Type)0);
    ASSERT_TRUE (Evnt_Isr.status == osErrorISR);
    
    
    NVIC_DisableIRQ((IRQn_Type)0);

  }
}

/**
@}
*/ 
// end of group signal_funcs

/*-----------------------------------------------------------------------------
 * End of file
 *----------------------------------------------------------------------------*/
