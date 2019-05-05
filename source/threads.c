#include "threads.h"
#include "main.h"

void PendSV_Handler( void ) __attribute__ (( naked ));

#define THREAD_PSP	0xFFFFFFFD

/* Thread Control Block */
typedef struct {
	void *stack;
	void *orig_stack;
	uint8_t in_use;
} tcb_t;

static tcb_t tasks[MAX_TASKS];
static int lastTask;
static int first = 1;

/* FIXME: Without naked attribute, GCC will corrupt r7 which is used for stack
 * pointer. If so, after restoring the tasks' context, we will get wrong stack
 * pointer.
 */
void PendSV_Handler( void )
{
	/* Save the old task's context */
	asm volatile("");
	/* save user state */
	__asm("MRS     R0, PSP \n");
	__asm("STMDB   R0!, {R4, R5, R6, R7, R8, R9, R10, R11, LR} \n");

	/* To get the task pointer address from result r0 */
	asm volatile("MOV      %0, R0\n" : "=r" (tasks[lastTask].stack));

	/* Find a new task to run */
	while (1) {
		lastTask++;
		if (lastTask == MAX_TASKS)
			lastTask = 0;
		if (tasks[lastTask].in_use) {
			/* Move the task's stack pointer address into r0 */
			asm volatile("MOV     R0, %0\n" : : "r" (tasks[lastTask].stack));

			/* Restore the new task's context and jump to the task */
			asm volatile("");
			__asm("POP     {R4, R5, R6, R7, R8, R9, R10, R11, IP, LR}  \n");
			__asm("MSR     PSR_NZCVQ, IP \n");

			asm volatile("BX      LR\n");
		}
	}
}


void SysTick_Handler(void)
{
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void thread_start()
{
	lastTask = 0;

	/* Save kernel context */
	__asm ("MRS    IP, PSR \n");          //ip and/or IP - Intra procedure call scratch register. This is a synonym for R12.
	__asm ("PUSH   {R4, R5, R6, R7, R8, R9, R10, R11, IP, LR} \n");

	/* Load user task's context from the stack */
	asm volatile("MOV     R0, %0\n" : : "r" (tasks[lastTask].stack));
	__asm ("LDMIA  R0!, {R4, R5, R6, R7, R8, R9, R10, R11, LR} \n");


	/*
	 * Put stack into PSP
	 * I don't know why, but usertask's stack is left out from previous instruction.
	 * Fortunately, the LDMIA instruction leaves in R0 the next memory address
	 * which in this case has the usertask's stack.
	 * So we'll put it in PSP.
	 * Note: It should be on Register 10. What happened?
	 * */
	__asm ("MSR    PSP, R0 \n");


	/* Set SPSEL to PSP */
	__asm ("MRS R0, CONTROL \n");
	__asm ("ORRS R0, R0, #0x2 \n");
	__asm ("MSR  CONTROL, R0 \n");
	__asm ("ISB \n");

	/* Load into R0 the usertask's stack
	 * R0 is the parameter that task1 will get.
	 * The memory pointing to usertask's stack  in in PSP, so we put it
	 * in R10 in order to load the value in that memory into R0.
	 * */
	__asm ("MRS R10, PSP \n");
	__asm ("LDMIA  R10!, {R0} \n");

	/* Jump to LR */
	asm volatile("BX      LR\n");
}

int thread_create(void (*run)(void *), void *userdata)
{
	/* Find a free thing */
	int threadId = 0;
	uint32_t *stack;

	for (threadId = 0; threadId < MAX_TASKS; threadId++) {
		if (tasks[threadId].in_use == 0)
			break;
	}

	if (threadId == MAX_TASKS)
		return -1;

	/* Create the stack */
	stack = (uint32_t *) malloc(STACK_SIZE * sizeof(uint32_t));
	tasks[threadId].orig_stack = stack;
	if (stack == 0)
		return -1;

	stack += STACK_SIZE - 32; /* End of stack, minus what we are about to push */
	if (first) {
		stack[8] =  (unsigned int) run;
		stack[9] =  (unsigned int) userdata;
		first = 0;
	} else {
		stack[8] =  (unsigned int) THREAD_PSP;
		stack[9] =  (unsigned int) userdata;
		stack[14] = (unsigned) &thread_self_terminal;
		stack[15] = (unsigned int) run;
		stack[16] = (unsigned int) 0x01000000; /* PSR Thumb bit */
	}

	/* Construct the control block */
	tasks[threadId].stack = stack;
	tasks[threadId].in_use = 1;

	return threadId;
}

void thread_kill(int thread_id)
{
	tasks[thread_id].in_use = 0;

	/* Free the stack */
	free(tasks[thread_id].orig_stack);
}

void thread_self_terminal()
{
	/* This will kill the stack.
	 * For now, disable context switches to save ourselves.
	 */
	asm volatile("CPSID   I\n");
	thread_kill(lastTask);
	asm volatile("CPSIE   I\n");

	/* And now wait for death to kick in */
	while (1);
}
