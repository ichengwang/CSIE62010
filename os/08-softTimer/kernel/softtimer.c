#include "os.h"
#define MAP_SIZE sizeof(uint32_t)
extern uint8_t need_schedule;
/*!< Table use to save TCB pointer.              */
timerCB_t    TIMCBTbl[MAX_TIMERS];
uint32_t     TimerMap[MAX_TIMERS/MAP_SIZE];
timerCB_t     *TimerList;

/* 
static functions
*/

static void insertTimerList(uint16_t timerID)
{
    timerCB_t * pTimer = &TIMCBTbl[timerID];
    timerCB_t * pTimerList;
    int deltaTicks;
    uint32_t timerCount=TIMCBTbl[timerID].timerCnt;

    if (timerCount > 0) {// if timerCount == 0 do nothing
        reg_t lock_status = spin_lock();
        if (list_isempty((list_t*)TimerList)) {
            list_insert_before((list_t*)TimerList, (list_t*)pTimer);
        } else {
            pTimerList = (timerCB_t*)TimerList->node.next;
            deltaTicks = timerCount;
            /* find correct place*/
            while(pTimerList != TimerList) {
                deltaTicks -= pTimerList->timerCnt;
                if (deltaTicks < 0) {
                    /* get correct place*/
                    list_insert_before((list_t*)pTimerList, (list_t*)pTimer);
                    pTimer->timerCnt = pTimerList->timerCnt + deltaTicks;
                    pTimerList->timerCnt = deltaTicks*-1;  
                    break;
                }
                pTimerList = (timerCB_t*)pTimerList->node.next;
                if (pTimerList == TimerList) {//add to last 
                    list_insert_before((list_t*)TimerList, (list_t*)pTimer);
                    pTimer->timerCnt = deltaTicks;
                }
            }
        }
        spin_unlock(lock_status);
    }
}

/*
remove from TimerList
if last node then remove directly
if not last node then remove and change timerCount
*/
static void removeTimerList(uint16_t timerID)
{
    timerCB_t * pTimer = &TIMCBTbl[timerID];
    timerCB_t * pNextTimer;
    // TimerList is critical
    reg_t lock_status = spin_lock();
    list_remove((list_t*)pTimer);
    if (!list_isempty((list_t*)TimerList)) {
        pNextTimer = (timerCB_t *)(pTimer->node.next);
        if (pNextTimer != TimerList){//not last node then change timerCnt
            pNextTimer->timerCnt += pTimer->timerCnt;
        } // last node remove direct
    }
    spin_unlock(lock_status);
}





void softTimer_init() {
    TimerList = (timerCB_t *)malloc(sizeof(timerCB_t));
    list_init((list_t*)TimerList);
}

/*
return timer_id or fail 
*/
err_t createTimer(uint8_t timerType,  
                  uint32_t timerCount,
                  uint32_t timerReload,
                  void(*callback)(void *parameter),
                  void *parameter
                  )
{
    reg_t lock_status;
    lock_status = spin_lock();
    for (int i=0; i<MAX_TIMERS;i++) {
        int mapIndex = i / MAP_SIZE;
        int mapOffset = i % MAP_SIZE;
        if ((TimerMap[mapIndex] & (1<< mapOffset)) == 0)
        {
            TimerMap[mapIndex] |= (1<< mapOffset);
            TIMCBTbl[i].timerID = i;
            TIMCBTbl[i].timerType = timerType;
            TIMCBTbl[i].timerState = TMR_STOPPED;
            TIMCBTbl[i].timerReload = timerReload;
            TIMCBTbl[i].timerCnt  = timerCount;
            TIMCBTbl[i].timerCallBack = callback;
            TIMCBTbl[i].parameter = parameter;
            list_init((list_t*)&TIMCBTbl[i].node);
            return i;
        }
    }
    spin_unlock(lock_status);
    return E_CREATE_FAIL;
}


/* timer operation*/

err_t startTimer(uint16_t timerID) 
{
    if(TIMCBTbl[timerID].timerState == TMR_RUNNING)   /* Is timer running?    */
    {
        return E_OK;                              /* Yes,do nothing,return OK */
    }
    
    /* No,set timer status as TMR_RUNNING */
    reg_t lock_status = spin_lock();
    TIMCBTbl[timerID].timerState = TMR_RUNNING; 
    spin_unlock(lock_status);
    insertTimerList(timerID);               /* Insert this timer into timer list  */
    return E_OK;                        /* Return OK                          */
}

err_t stopTimer(uint16_t timerID)
{
    if(TIMCBTbl[timerID].timerState == TMR_STOPPED)/* Does timer stop running?*/
    {
        return E_OK;                    /* Yes,do nothing,return OK           */
    }
    removeTimerList(timerID);             /* No,remove this timer from timer list */
    
    /* Set timer status as TMR_STATE_STOPPED  */
    reg_t lock_status = spin_lock();
    TIMCBTbl[timerID].timerState = TMR_STOPPED;	
    spin_unlock(lock_status);
    return E_OK;                        /* Return OK                          */
}

/*
    free a timer
*/
err_t delTimer(uint16_t timerID)
{
    if(TIMCBTbl[timerID].timerState == TMR_RUNNING) /* Is timer running?      */
    {
        removeTimerList(timerID);         /* Yes,remove this timer from timer list*/
    }
    int mapIndex = timerID / MAP_SIZE;
    int mapOffset = timerID % MAP_SIZE;
    reg_t lock_status = spin_lock();
    TimerMap[mapIndex] &=~(1<<mapOffset);   
    spin_unlock(lock_status);     /* Release resource that this timer hold*/
    return E_OK;                      /* Return OK                            */
}

uint32_t getCurTimerCnt(uint16_t timerID)
{
    return TIMCBTbl[timerID].timerCnt;
}


/*
reset timerCnt and timerReload
*/
err_t setCurTimerCnt(uint16_t timerID,
                     uint32_t timerCount,
                     uint32_t timerReload)
{
    reg_t lock_status;
    lock_status = spin_lock();
    TIMCBTbl[timerID].timerCnt    = timerCount; /* Reset timer counter and reload value */
    TIMCBTbl[timerID].timerReload = timerReload;
    spin_unlock(lock_status);							
    if(TIMCBTbl[timerID].timerState == TMR_RUNNING)   /* Is timer running?    */
    {
        removeTimerList(timerID);           /* Yes,reorder timer in timer list    */
        insertTimerList(timerID);	
    }
    return E_OK;                        /* Return OK                          */
}


/*
Timer dispose
*/
void timerDispose(void) 
{
    timerCB_t * pTimer;
    reg_t lock_status;

    lock_status = spin_lock();
    pTimer = (timerCB_t *)TimerList->node.next;
    while(pTimer != TimerList && pTimer->timerCnt <= 0) {
        switch(pTimer->timerType) {
            case TMR_ONE_SHOT:
                removeTimerList(pTimer->timerID);
                pTimer->timerState = TMR_STOPPED;
                (pTimer->timerCallBack)(pTimer->parameter);
                break;
            case TMR_PERIOD:
                removeTimerList(pTimer->timerID);
                pTimer->timerCnt = pTimer->timerReload;
                insertTimerList(pTimer->timerID);
                (pTimer->timerCallBack)(pTimer->parameter);
            break;
        }
        pTimer = (timerCB_t*)TimerList->node.next;
    }
    spin_unlock(lock_status);
    if (need_schedule) {
        lock_status = spin_lock();
        need_schedule = 0;
        spin_unlock(lock_status);
        schedule(); //now in ISR
    }
}