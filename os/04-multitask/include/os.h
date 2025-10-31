#ifndef __OS_H__
#define __OS_H__

#include "types.h"
#include "config.h"
#include "platform.h"
#include "task.h"
#include "list.h"
#include "riscv.h"

#include <stddef.h>
#include <stdarg.h>

/* uart */
extern int uart_putc(char ch);
extern void uart_puts(char *s);

/* printf */
extern int  kprintf(const char* s, ...);
extern void panic(char *s);

/* mem */
void *memset(void *ptr, int value, size_t num);
char* memcpy(void* dest,const void* src, size_t num);

/* lock */
int spin_lock();
int spin_unlock();

/* memory management */
extern void *page_alloc(int npages);
extern void page_free(void *p);
void *malloc(uint32_t size);
void free(void *);

/* task*/
void readyQ_init();
void InitTCBList(void);
taskCB_t * task_create(const char *name,
                  void (*taskFunc)(void *parameter),
                  void       *parameter,
                  uint32_t    stack_size,
                  uint16_t    priority);
void task_startup(taskCB_t * ptcb);
err_t task_resume(taskCB_t *ptcb);
err_t task_suspend(taskCB_t * ptcb);
err_t task_yield(void);


/* sched */
void sched_init();
void schedule();

/* user*/
void loadTasks(void);
#endif /* __OS_H__ */
