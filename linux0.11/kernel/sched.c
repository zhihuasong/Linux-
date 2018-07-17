/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
/*
 * 'sched.c'����Ҫ���ں��ļ������а����йص��ȵĻ�������(sleep_on��wakeup��schedule ��)�Լ�
 * һЩ�򵥵�ϵͳ���ú���������getpid()�����ӵ�ǰ�����л�ȡһ���ֶΣ���
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1<<((nr)-1))						// ȡ�ź�nr ���ź�λͼ�ж�Ӧλ�Ķ�������ֵ���źű��1-32��
													// �����ź�5 ��λͼ��ֵ = 1<<(5-1) = 16 = 00010000b��
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))	// ����SIGKILL ��SIGSTOP �ź�������������
													// ��������(��10111111111011111111b)��
// ��ʾ�����nr �Ľ��̺š�����״̬���ں˶�ջ�����ֽ�������Լ����
void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);
	
	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

// ��ʾ�������������š����̺š�����״̬���ں˶�ջ�����ֽ�������Լ����
void show_stat(void)
{
	int i;
	
	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

// ����ÿ��ʱ��Ƭ�ĵδ�����
#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

// ��������ռ���ڴ泤�����������Ա�ĳ���
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};	// �����ʼ���������(INIT_TASK��sched.h �ж���)��

long volatile jiffies=0;	// �ӿ�����ʼ����ĵδ���ʱ��ֵ��10ms/�δ𣩡�
							// ǰ����޶���volatile��Ӣ�Ľ������ױ䡢���ȶ�����˼��������Ҫ��gcc 
							// ��Ҫ�Ըñ��������Ż�����Ҳ��ҪŲ��λ�ã���ΪҲ���ĳ�������޸�����ֵ��
long startup_time=0;		// ����ʱ�䡣��1970:0:0:0 ��ʼ��ʱ��������
struct task_struct *current = &(init_task.task);			// ��ǰ����ָ�루��ʼ��Ϊ��ʼ���񣩡�
struct task_struct *last_task_used_math = NULL;				// ʹ�ù�Э�����������ָ�롣

struct task_struct * task[NR_TASKS] = {&(init_task.task), };// ��������ָ������

long user_stack [ PAGE_SIZE>>2 ] ;							// �ں˳�ʼ����ջ��Ҳ�Ǻ�������0���û�̬��ջ

// �ýṹ�������ö�ջss:esp�����ݶ�ѡ�����ָ�룩����head.s����23 �С�
struct {
	long * a;
	short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
/*
 * ����ǰЭ���������ݱ��浽��Э������״̬�����У�������ǰ�����Э������
 * ���ݼ��ؽ�Э��������
 */
// �����񱻵��Ƚ������Ժ󣬸ú������Ա���ԭ�����Э������״̬�������ģ����ָ��µ��Ƚ�����
// ��ǰ�����Э������ִ��״̬��
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
/*
 * 'schedule()'�ǵ��Ⱥ��������Ǹ��ܺõĴ��룡û���κ����ɶ��������޸ģ���Ϊ�����������е�
 * �����¹����������ܹ���IO-�߽紦��ܺõ���Ӧ�ȣ���ֻ��һ����ֵ�����⣬�Ǿ���������ź�
 * ������롣
 * ע�⣡������0 �Ǹ�����('idle')����ֻ�е�û�����������������ʱ�ŵ������������ܱ�ɱ
 * ����Ҳ����˯�ߡ�����0 �е�״̬��Ϣ'state'�Ǵ������õġ�
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;
	
	/* check alarm, wake up any interruptible tasks that have got a signal */
	/* ���alarm�����̵ı�����ʱֵ���������κ��ѵõ��źŵĿ��ж����� */
	
	// ���������������һ������ʼ���alarm��
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			// ��������alarm ʱ���Ѿ�����(alarm<jiffies),�����ź�λͼ����SIGALRM �źţ�Ȼ����alarm��
			// jiffies ��ϵͳ�ӿ�����ʼ����ĵδ�����10ms/�δ𣩡�������sched.h ��
			if ((*p)->alarm && (*p)->alarm < jiffies) {
				(*p)->signal |= (1<<(SIGALRM-1));
				(*p)->alarm = 0;
			}
			// ����ź�λͼ�г����������ź��⻹�������źţ����������ڿ��ж�״̬����������Ϊ����״̬��
			// ����'~(_BLOCKABLE & (*p)->blocked)'���ں��Ա��������źţ���SIGKILL ��SIGSTOP ���ܱ�������
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
				(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}
		
		/* this is the scheduler proper: */
		/* �����ǵ��ȳ������Ҫ���� */
		
		while (1) {
			c = -1;
			next = 0;
			i = NR_TASKS;
			p = &task[NR_TASKS];
			// ��δ���Ҳ�Ǵ�������������һ������ʼѭ�������������������������ۡ��Ƚ�ÿ������
			// ״̬�����counter����������ʱ��ĵݼ��δ������ֵ����һ��ֵ������ʱ�仹������next ��
			// ָ���ĸ�������š�
			while (--i) {
				if (!*--p)
					continue;
				if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
					c = (*p)->counter, next = i;	// ������������ȼ�С�ڸ�ֵ�����
													// ���ʴ˴��ȼ���c = (*p)->counter; next = i;
			}
			// ����Ƚϵó���counter ֵ����0 �Ľ�������˳�ѭ����ִ�������л�
			if (c) break;
			// ����͸���ÿ�����������Ȩֵ������ÿһ�������counter ֵ��Ȼ��ص�125 �����±Ƚϡ�
			// counter ֵ�ļ��㷽ʽΪcounter = counter /2 + priority��[�ұ�counter=0[??]]
			for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
				if (*p)
					(*p)->counter = ((*p)->counter >> 1) +
					(*p)->priority;
		}
		switch_to(next);	// �л��������Ϊnext �����񣬲�����֮��
}

// pause()ϵͳ���á�ת����ǰ�����״̬Ϊ���жϵĵȴ�״̬�������µ��ȡ�
// ��ϵͳ���ý����½��̽���˯��״̬��ֱ���յ�һ���źš����ź�������ֹ���̻���ʹ���̵���
// һ���źŲ�������ֻ�е�������һ���źţ������źŲ����������أ�pause()�Ż᷵�ء�
// ��ʱpause()����ֵӦ����-1������errno ����ΪEINTR�����ﻹû����ȫʵ�֣�ֱ��0.95 �棩��
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

// �ѵ�ǰ������Ϊ�����жϵĵȴ�״̬������˯�߶���ͷ��ָ��ָ��ǰ����
// ֻ����ȷ�ػ���ʱ�Ż᷵�ء��ú����ṩ�˽������жϴ������֮���ͬ�����ơ�
// ��������*p �Ƿ��õȴ�����Ķ���ͷָ�롣���μ��б���˵������[??]
// ע�⣺tmp�ǵ�ǰ������ں�ջ�еľֲ������������л�ʱ�ں˶�ջָ�뱻���浽TSS���
// ss��esp�ֶΣ������л�����ʱ��CPU�ļĴ��������±�����Ϊ��������ں�ջ��ss��esp.
// sleepʱ���������ں�̬
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;
	
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;								// ��tmp ָ���Ѿ��ڵȴ������ϵ�����(����еĻ�)��
	*p = current;							// ��˯�߶���ͷ�ĵȴ�ָ��ָ��ǰ����
	current->state = TASK_UNINTERRUPTIBLE;	// ����ǰ������Ϊ�����жϵĵȴ�״̬
	schedule();								// ���µ��ȡ�
	// ֻ�е�����ȴ����񱻻���ʱ�����ȳ�����ַ��ص�������ʾ�����ѱ���ȷ�ػ��ѡ�
	// ��Ȼ��Ҷ��ڵȴ�ͬ������Դ����ô����Դ����ʱ�����б�Ҫ�������еȴ�����Դ�Ľ��̡��ú���
	// Ƕ�׵��ã�Ҳ��Ƕ�׻������еȴ�����Դ�Ľ��̡�Ȼ��ϵͳ�������Щ���̵��������������µ���
	// Ӧ�����ĸ���������ʹ����Դ��Ҳ������Щ���̾����ϸڡ�
	if (tmp)								// �������ڵȴ���������Ҳ������Ϊ����״̬�����ѣ�
		tmp->state=0;
}

// ����ǰ������Ϊ���жϵĵȴ�״̬��������*p ָ���ĵȴ������С�
// sleepʱ���������ں�̬
void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;
	
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	// ����ȴ������л��еȴ����񣬲��Ҷ���ͷָ����ָ��������ǵ�ǰ����ʱ���򽫸õȴ�������Ϊ
	// �����еľ���״̬��������ִ�е��ȳ��򡣵�ָ��*p ��ָ��Ĳ��ǵ�ǰ����ʱ����ʾ�ڵ�ǰ���񱻷�
	// ����к������µ����񱻲���ȴ������У���ˣ���Ȼ�������ǿ��жϵģ���Ӧ������ִ������
	// �����ĵȴ�����
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	// ����һ���������[??]��Ӧ����*p = tmp���ö���ͷָ��ָ������ȴ����񣬷����ڵ�ǰ����֮ǰ����
	// �ȴ����е��������Ĩ���ˡ�
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

// ����ָ������*p��
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;		// ��������[??]��ͬinterruptible_sleep_on()�е�ע�͡�
	}
}

/*
* OK, here are some floppy things that shouldn't be in the kernel
* proper. They are here because the floppy needs a timer, and this
* was the easiest way of doing it.
*/
/*
 * ���ˣ������￪ʼ��һЩ�й����̵��ӳ��򣬱���Ӧ�÷����ں˵���Ҫ�����еġ������Ƿ�������
 * ����Ϊ������Ҫһ��ʱ�ӣ����������������İ취��
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;	// ��������Ĵ���(��ֵ������DMA �������жϡ�����FDC)��

// ָ�����̵�������ת״̬�����ӳٵδ�����ʱ�䣩��
// nr -- ������(0-3)������ֵΪ�δ�����
int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;
	
	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

// �ȴ�ָ�����������������ʱ�䡣
void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

// �ùر���Ӧ�������ͣת��ʱ����3 �룩��
void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

// ���̶�ʱ�����ӳ��򡣸������������ʱֵ�����ر�ͣת��ʱֵ�����ӳ�������ʱ�Ӷ�ʱ
// �ж��б����ã����ÿһ���δ�(10ms)������һ�Σ�������￪����ͣת��ʱ����ֵ�����ĳ
// һ�����ͣת��ʱ��������������Ĵ����������λ��λ��
void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;
	
	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64	// ������64 ����ʱ����64 �����񣩡�

// ��ʱ������ṹ�Ͷ�ʱ�����顣
static struct timer_list {
	long jiffies;					// ��ʱ�δ�����
	void (*fn)();					// ��ʱ�������
	struct timer_list * next;		// ��һ����ʱ����ָ�룬ʹ��ʱ����������
} timer_list[TIME_REQUESTS], * next_timer = NULL;	// next_timer �Ƕ�ʱ�������ͷָ��

// ��Ӷ�ʱ�����������Ϊָ���Ķ�ʱֵ(�δ���)����Ӧ�Ĵ������ָ�롣
// jiffies �C ��10 ����Ƶĵδ�����*fn()- ��ʱʱ�䵽ʱִ�еĺ�����
// ��ע�⣺�˺���û������ȫ�����޸ġ�
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;
	
	if (!fn)			// �����ʱ�������ָ��Ϊ�գ����˳���
		return;
	cli();				// ���ж�
	if (jiffies <= 0)	// �����ʱֵ<=0�������̵����䴦����򡣲��Ҹö�ʱ�������������С�
		(fn)();
	else {
		// �Ӷ�ʱ�������У���һ�������
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)		// ��������ָ��Ϊ�գ����ǿ���
				break;
			
			// ����Ѿ������˶�ʱ�����飬��ϵͳ������
			if (p >= timer_list + TIME_REQUESTS)
				panic("No more time requests free");	// ��ʾ������Ϣ���������ļ�ͬ��������Ȼ�������ѭ��
			// ���뵽��ʱ������ͷ����next_timer �Ƕ�ʱ�������ͷָ��
			p->fn = fn;
			p->jiffies = jiffies;
			p->next = next_timer;
			next_timer = p;
			// �������ʱֵ��С��������������ʱ��ȥ����ǰ����Ҫ�ĵδ����������ڴ���ʱ��ʱֻҪ
			// �鿴����ͷ�ĵ�һ��Ķ�ʱ�Ƿ��ڼ��ɡ�[ [??]����!!! ��γ������û�п�����ȫ������²���Ķ�ʱ
			// ��ֵ С�� ԭ��ͷһ����ʱ��ֵʱ������˵�¶�ʱ��ֻҪ����Ǳ��ƶ�������β������Ӧ�ý��¶�ʱ
			// ������ĵ�һ����ʱ��ֵ����ȥ�¶�ʱ�����յĶ�ʱֵ���޸Ĳμ�����ע�͵Ĵ���]
			while (p->next && p->next->jiffies < p->jiffies) {
				p->jiffies -= p->next->jiffies;
				fn = p->fn;
				p->fn = p->next->fn;
				p->next->fn = fn;
				jiffies = p->jiffies;
				p->jiffies = p->next->jiffies;
				p->next->jiffies = jiffies;
				p = p->next;
			}
			// �޸ģ��ڴ˴�������´���
			// struct timer_list * temp = p;
			// if(p->next)
			// {
			// 	p->jiffies -= temp->jiffies;
			//	p = p->next;
			// }
	}
	sti();				// ���ж�
}

// ʱ���ж�C �������������kernel/system_call.s �е�_timer_interrupt�����á�
// ����cpl �ǵ�ǰ��Ȩ��0 ��3��0 ��ʾ�ں˴�����ִ�С�
// ����һ����������ִ��ʱ��Ƭ����ʱ������������л�����ִ��һ����ʱ���¹�����
void do_timer(long cpl)
{
	extern int beepcount;			// ����������ʱ��δ���(kernel/chr_drv/console.c)
	extern void sysbeepstop(void);	// �ر�������(kernel/chr_drv/console.c)
	
	// ���������������������رշ�����(��0x61 �ڷ��������λλ0 ��1��λ0 ����8253
	// ������2 �Ĺ�����λ1 ����������)��
	if (beepcount)
		if (!--beepcount)
			sysbeepstop();
	// �����ǰ��Ȩ��(cpl)Ϊ0����ߣ���ʾ���ں˳����ڹ��������򽫳����û�����ʱ��stime ������
	// ���cpl > 0�����ʾ��һ���û������ڹ���������utime��
	if (cpl)
		current->utime++;
	else
		current->stime++;
	// ������û��Ķ�ʱ�����ڣ��������1 ����ʱ����ֵ��1������ѵ���0���������Ӧ�Ĵ���
	// ���򣬲����ô������ָ����Ϊ�ա�Ȼ��ȥ�����ʱ����
	if (next_timer) {					// next_timer �Ƕ�ʱ�������ͷָ��
		next_timer->jiffies--;			// ÿ��ʱ���жϣ�ֻ����ʱ������ͷ����ʱ�����������1
		while (next_timer && next_timer->jiffies <= 0) {	// ���ܶ����ʱ��ͬʱ��ʱ����ѭ��
			void (*fn)(void);			// ���������һ������ָ�붨��
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();						// ���ô�����
		}
	}
	// �����ǰ���̿�����FDC ����������Ĵ��������(�綯��)����λ����λ�ģ���ִ�����̶�ʱ����
	if (current_DOR & 0xf0)
		do_floppy_timer();

	if ((--current->counter)>0) return;	// �����������ʱ�仹û���꣬���˳���
	current->counter=0;
	if (!cpl) return;					// �����ں˳��򣨳����û����򣩣�������counter ֵ���е���
	schedule();							// ���̵���
}

// ϵͳ���ù��� - ���ñ�����ʱʱ��ֵ(��)��
// ����Ѿ����ù�alarm ֵ���򷵻ؾ�ֵ�����򷵻�0��
int sys_alarm(long seconds)
{
	int old = current->alarm;
	
	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

// ȡ��ǰ���̺�pid��
int sys_getpid(void)
{
	return current->pid;
}

// ȡ�����̺�ppid��
int sys_getppid(void)
{
	return current->father;
}

// ȡ�û���uid��
int sys_getuid(void)
{
	return current->uid;
}

// ȡeuid��
int sys_geteuid(void)
{
	return current->euid;
}

// ȡ���gid��
int sys_getgid(void)
{
	return current->gid;
}

// ȡegid��
int sys_getegid(void)
{
	return current->egid;
}

// ϵͳ���ù��� -- ���Ͷ�CPU ��ʹ������Ȩ�����˻�����?����
// Ӧ������increment ����0������Ļ�,��ʹ����Ȩ���󣡣�
int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

// ���ȳ����ʼ����--��main()����
// �˴���ʼ����Ϊ3�㣺
// 1�����Ѿ���ƺõĽ���0����ṹtask_struct��ȫ������������ҽӣ�����ȫ�������������̲��Լ�����̵���
//    ��صļĴ������г�ʼ������
// 2����ʱ���жϽ������ã��Ա���л���ʱ��Ƭ�Ľ�����������
// 3��ͨ��set_system_gate��system_call��ϵͳ��������ڣ���IDT��ҽӣ�ʹ����0�߱�����ϵͳ���õ�����
void sched_init(void)
{
	int i;
	struct desc_struct * p;	// ȫ�ֻ��ж��������ṹָ�� typedef struct desc_struct{unsigned long a,b;}desc_struct[256];
	
	if (sizeof(struct sigaction) != 16)	// ����й�״̬�Ľṹ
		panic("Struct sigaction MUST be 16 bytes");
	// ��������0������״̬���������;ֲ����ݱ�������--��include/asm/system.h
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	// ���������������������i=1,����������0��
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
	/* Clear NT, so that we won't have troubles with that later on */
	// �����־�Ĵ����е�λNT��NT��־λ���ڿ��Ƴ���ĵݹ���á���NT��λʱ����ǰ�ж�����ִ��iretʱ�ͻ����������л���
	// NTָ��TSS�е�back_link�ֶ��Ƿ���Ч
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	
	// ע��!!!�ǽ�GDT����Ӧ��LDT��������ѡ������ص�ldtr��ֻ��ȷ������1�Σ��Ժ������LDT�ļ���
	// ����CPU����TSS�е�LDT���Զ�����
	ltr(0);										// ������0��TSS���ص�����Ĵ���tr
	lldt(0);									// ���ֲ�����������ص��ֲ���������Ĵ���
	
	// ��ʼ��8253��ʱ��
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */	// LATCH�궨����sched.c��#define LATCH (1193180/HZ),ÿ10����һ��ʱ���ж�
	outb(LATCH >> 8 , 0x40);	/* MSB */
	// ʱ���жϳ���ҽ�
	set_intr_gate(0x20,&timer_interrupt);		// �ҽ�timer_interrupt()ʱ���жϺ���
	outb(inb_p(0x21)&~0x01,0x21);				// ����ʱ���ж�
	// ��system_call��ϵͳ��������ڣ���IDT��ҽӣ�ʹ����0�߱�����ϵͳ���õ�����
	set_system_gate(0x80,&system_call);			// ����ϵͳ�����ж���
}
