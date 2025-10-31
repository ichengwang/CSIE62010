#include "os.h"

extern timerCB_t TIMCBTbl[];
uint8_t need_schedule=0;
/*!< Table use to save TCB pointer.              */
taskCB_t    TCBTbl[MAX_USER_TASKS+SYS_TASK_NUM];

/*!< The stack of IDLE task.                     */
uint32_t   idle_stk[SYS_STACK_SIZE] = {0};

taskCB_t *    TCBRunning         = NULL;        /*!< Pointer to TCB that current running task.  */
taskCB_t *    FreeTCB            = NULL;         /*!< pointer to free TCB                        */
taskCB_t      TCBRdy[PRIO_LEVEL];        /*!< READY list.                 */

taskCB_t * getCurrentTask() {
    return TCBRunning;
}

/**
 *******************************************************************************
 * @brief      Create a free TCB list.	  
 *******************************************************************************
 */
void InitTCBList(void)
{	
    uint16_t  i;
    taskCB_t * ptcb1;
    taskCB_t * ptcb0;
    
    FreeTCB = &TCBTbl[0];	/* Build the free TCB list            */
    FreeTCB->node.prev = NULL;
    FreeTCB->node.next = NULL;

    FreeTCB->state = TASK_INIT;
    FreeTCB->taskID = 0;
    ptcb1 = &TCBTbl[1];
    for(i=1;i< MAX_USER_TASKS+SYS_TASK_NUM;i++ )
    {
        ptcb1->taskID    = i;
        ptcb1->state     = TASK_INIT;
        ptcb1->node.next = NULL;
        ptcb1->node.prev = NULL;        
        ptcb0 = ptcb1-1;
        ptcb0->node.next = (list_t *)ptcb1;
        ptcb1++;
    }	
}

void readyQ_init() {
    for (int i=0;i<PRIO_LEVEL;i++)
        list_init((list_t *)&TCBRdy[i]);
}

static taskCB_t * _getFreeTCB(void)
{
    taskCB_t * ptcb;
    reg_t lock_status;

    lock_status = spin_lock();
    if(FreeTCB == NULL)                 /* Is there no free TCB               */
    {
        spin_unlock(lock_status);                  /* Yes,unlock schedule                */
        return NULL;                    /* Error return                       */
    }	
    ptcb    = FreeTCB;          /* Yes,assgin free TCB for this task  */    
    /* Set next item as the head of free TCB list                     */
    FreeTCB = (taskCB_t *)ptcb->node.next; 
    ptcb->node.next = NULL;
    ptcb->node.prev = NULL;
    spin_unlock(lock_status);
    return ptcb;        
}

taskCB_t * getNewTCB(uint8_t index) {
    return &TCBTbl[index];
}

/*
task delay wake up and insert into readyQ
then schedule
08
*/
void taskTimeOut(void *parameter) 
{
    taskCB_t *ptcb = (taskCB_t*) parameter;
    
    DEBUG("taskTimeout\n");
    if (task_resume(ptcb)!=ERROR) {
        need_schedule = 1;
    }
}

err_t task_init(taskCB_t *ptcb, const char *name,
                  void (*taskFunc)(void *parameter),
                  void       *parameter,
                  uint32_t    stack_size,
                  uint16_t    priority,
                  uint32_t    ticks)
{

    void *stack_start;
    stack_start = (void *)malloc(stack_size);
    if (stack_start == NULL)
    {
        /* allocate stack failure */
        return ERROR;
    }

    memcpy(ptcb->name, name, sizeof(ptcb->name));
    ptcb->entry = (void *)taskFunc;
    ptcb->parameter = parameter;

    /* init thread stack */
    memset(ptcb->stack_addr, 0, ptcb->stack_size);
    
    ptcb->ctx.ra = (reg_t)taskFunc;
    ptcb->ctx.sp = (reg_t)(stack_start + stack_size);
    ptcb->ctx.pc = (reg_t)taskFunc;

    ptcb->priority    = priority;
    ptcb->init_ticks = ticks;
    ptcb->remain_ticks = ticks;

    // 08
    int timerID = createTimer(TMR_ONE_SHOT, 0, 0, (void*)taskTimeOut, (void*)ptcb);   
    ptcb->timer = &TIMCBTbl[timerID]; 

    list_init((list_t*)ptcb);
    return OK;
}	

taskCB_t * task_create(const char *name,
                  void (*taskFunc)(void *parameter),
                  void       *parameter,
                  uint32_t    stack_size,
                  uint16_t    priority,
                  uint32_t    ticks)
{
    taskCB_t *ptcb = _getFreeTCB();
    if (ptcb == NULL) { return NULL; }
    err_t ret = task_init(ptcb, name, taskFunc, parameter, stack_size, priority, ticks);
    if (ret == OK)
        return ptcb;
    return NULL;
}	

void task_startup(taskCB_t * ptcb)
{

    /* change thread stat */
    ptcb->state = TASK_SUSPEND;
    /* then resume it */
    task_resume(ptcb);
}


err_t task_resume(taskCB_t *ptcb)
{
    reg_t lock_status;
    if (ptcb->state != TASK_SUSPEND)
    {
        return ERROR;
    }

    //timer_stop(&ptcb->timer);
    lock_status = spin_lock(); 
    /* remove from suspend list */
    list_remove((list_t*)ptcb);
    list_insert_before((list_t*)&TCBRdy[ptcb->priority], (list_t*)ptcb); 
    spin_unlock(lock_status);

    return OK;

}

err_t task_suspend(taskCB_t * ptcb)
{
    if (ptcb->state != TASK_READY)
    {
        return ERROR;
    }
    reg_t lock_status;
    lock_status = spin_lock();
    /* change thread stat */
    list_remove((list_t*)ptcb);
    ptcb->state = TASK_SUSPEND;

    spin_unlock(lock_status);
    return OK;
}


err_t task_yield(void)
{
    taskCB_t *ptcb;

    /* set to current task */
    ptcb = TCBRunning;

    /* if the task stat is READY and on ready queue list */
    if (ptcb->state == TASK_READY)
    {
        spin_lock();
        /* remove task from task list */
        list_remove((list_t*)ptcb);
        /* put task to end of ready queue */
        list_insert_before((list_t*)&TCBRdy[ptcb->priority], (list_t*)ptcb);
        spin_unlock();
    }
 	/* trigger a machine-level software interrupt */
	int id = r_mhartid(); //07-cooperation
	*(uint32_t*)CLINT_MSIP(id) = 1;
    return OK;
}


//08
static void taskDelayTicks(uint32_t ticks) 
{
    taskCB_t *task = getCurrentTask();
    task->timer->timerCnt = ticks;
    task->timer->timerType = TMR_ONE_SHOT;
    task->state = TASK_SUSPEND;
    reg_t lock_status = spin_lock();
    list_remove((list_t*)task);
    spin_unlock(lock_status);
    startTimer(task->timer->timerID);
    task_yield();
}

void taskDelay(uint32_t sec) 
{
    taskDelayTicks(TICK_PER_SECOND* sec);
}

/***********
 * idle task 08
*/
static void idle(void *p) 
{
    while(1) {
        //do nothing now
        //kprintf("                    idle task\n");
        //task_yield();
    }
}

err_t idleTask_init()
{
    taskCB_t *ptcb = _getFreeTCB();
    void *stack_start;
    stack_start = (void*)&idle_stk;
    memcpy(ptcb->name, "idle", sizeof(ptcb->name));
    ptcb->entry = (void *)idle;
    ptcb->parameter = NULL;
    memset(ptcb->stack_addr, 0, ptcb->stack_size);
    
    ptcb->ctx.ra = (reg_t)idle;
    ptcb->ctx.sp = (reg_t)(stack_start + SYS_STACK_SIZE);
    ptcb->ctx.pc = (reg_t)idle;

    ptcb->priority    = PRIO_LEVEL-1;
    ptcb->init_ticks = 0;
    ptcb->remain_ticks = 0;
    //ptcb->timer = NULL; //do not need timer   
    list_init((list_t*)ptcb);
    task_startup(ptcb);
    return OK;
}	