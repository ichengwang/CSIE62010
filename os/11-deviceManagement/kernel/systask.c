/***********
 * idle task 08
*/

#include "os.h"

static void idle(void *p) 
{
    while(1) {
        //do nothing now
        //kprintf("                    idle task\n");
        //task_yield();
        //sometime, idle task set watchdog module
        //and when context switch to idle then reset watchdog
        //then when watchdog timeout, the system reboot.
        task_yield();
    }
}

err_t idleTask_init()
{
    taskCB_t *ptcb = task_create("idle", idle, NULL, 1024, PRIO_LEVEL-1,200);
    delTimer(ptcb->timer->timerID);
    ptcb->timer = NULL; //do not need timer   
    list_init((list_t*)ptcb);
    task_startup(ptcb);
    return OK;
}	