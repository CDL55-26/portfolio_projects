#include <stdint.h>

typedef struct TCB {
    uint64_t *stack_pointer; // Pointer to the top of the task's stack
} TCB_t;

extern TCB_t tasks[2]; //initially just have 2 tasks
extern uint32_t current_task_idx;

void task1(void);
void task2(void);
void task_init(void);

void scheduler(void);