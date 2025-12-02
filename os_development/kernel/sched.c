#include "os.h"

/* defined in entry.S */
extern void switch_to(ctx_t *next);
extern void schedule(void);
extern taskCB_t TCBRdy[];
extern taskCB_t * TCBRunning; 

/* Dedicated scheduler stack */
uint32_t sched_stack[8192];
uint32_t *sched_stack_top = &sched_stack[8192];

void sched_init()
{
	w_mscratch(0);
}

/* Called on scheduler stack */
void do_schedule(void)
{
	taskCB_t *nextTask;
	ctx_t *next;
	taskCB_t *readyQ=NULL;

	//get highest priority queue
	for(int i=0;i<PRIO_LEVEL;i++) {
		if (!list_isempty((list_t*)&TCBRdy[i])) {
			readyQ = &TCBRdy[i];
			break;
		}
	}
	if (readyQ==NULL) return;

	//get next task
	nextTask= (taskCB_t*)readyQ->node.next;
	next = &nextTask->ctx;
	list_remove((list_t*)nextTask);
	//current task into ready queue
	if (TCBRunning != NULL){//kernel
		taskCB_t *currentTask = TCBRunning;
		if (currentTask->priority < nextTask->priority)
			return;
		currentTask->state = TASK_READY;
		list_insert_before((list_t*)&TCBRdy[currentTask->priority], (list_t*)currentTask);
	} 
	//
	TCBRunning = nextTask;
	nextTask->state = TASK_RUNNING;
	switch_to(next);
}

