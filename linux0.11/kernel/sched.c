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
 * 'sched.c'是主要的内核文件。其中包括有关调度的基本函数(sleep_on、wakeup、schedule 等)以及
 * 一些简单的系统调用函数（比如getpid()，仅从当前任务中获取一个字段）。
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1<<((nr)-1))						// 取信号nr 在信号位图中对应位的二进制数值。信号编号1-32。
													// 比如信号5 的位图数值 = 1<<(5-1) = 16 = 00010000b。
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))	// 除了SIGKILL 和SIGSTOP 信号以外其它都是
													// 可阻塞的(…10111111111011111111b)。
// 显示任务号nr 的进程号、进程状态和内核堆栈空闲字节数（大约）。
void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);
	
	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

// 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数（大约）。
void show_stat(void)
{
	int i;
	
	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

// 定义每个时间片的滴答数。
#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

// 共用体所占的内存长度是其中最长成员的长度
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};	// 定义初始任务的数据(INIT_TASK在sched.h 中定义)。

long volatile jiffies=0;	// 从开机开始算起的滴答数时间值（10ms/滴答）。
							// 前面的限定符volatile，英文解释是易变、不稳定的意思。这里是要求gcc 
							// 不要对该变量进行优化处理，也不要挪动位置，因为也许别的程序会来修改它的值。
long startup_time=0;		// 开机时间。从1970:0:0:0 开始计时的秒数。
struct task_struct *current = &(init_task.task);			// 当前任务指针（初始化为初始任务）。
struct task_struct *last_task_used_math = NULL;				// 使用过协处理器任务的指针。

struct task_struct * task[NR_TASKS] = {&(init_task.task), };// 定义任务指针数组

long user_stack [ PAGE_SIZE>>2 ] ;							// 内核初始化堆栈，也是后来任务0的用户态堆栈

// 该结构用于设置堆栈ss:esp（数据段选择符，指针），见head.s，第23 行。
struct {
	long * a;
	short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
/*
 * 将当前协处理器内容保存到老协处理器状态数组中，并将当前任务的协处理器
 * 内容加载进协处理器。
 */
// 当任务被调度交换过以后，该函数用以保存原任务的协处理器状态（上下文）并恢复新调度进来的
// 当前任务的协处理器执行状态。
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
 * 'schedule()'是调度函数。这是个很好的代码！没有任何理由对它进行修改，因为它可以在所有的
 * 环境下工作（比如能够对IO-边界处理很好的响应等）。只有一件事值得留意，那就是这里的信号
 * 处理代码。
 * 注意！！任务0 是个闲置('idle')任务，只有当没有其它任务可以运行时才调用它。它不能被杀
 * 死，也不能睡眠。任务0 中的状态信息'state'是从来不用的。
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;
	
	/* check alarm, wake up any interruptible tasks that have got a signal */
	/* 检测alarm（进程的报警定时值），唤醒任何已得到信号的可中断任务 */
	
	// 从任务数组中最后一个任务开始检测alarm。
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			// 如果任务的alarm 时间已经过期(alarm<jiffies),则在信号位图中置SIGALRM 信号，然后清alarm。
			// jiffies 是系统从开机开始算起的滴答数（10ms/滴答）。定义在sched.h 。
			if ((*p)->alarm && (*p)->alarm < jiffies) {
				(*p)->signal |= (1<<(SIGALRM-1));
				(*p)->alarm = 0;
			}
			// 如果信号位图中除被阻塞的信号外还有其它信号，并且任务处于可中断状态，则置任务为就绪状态。
			// 其中'~(_BLOCKABLE & (*p)->blocked)'用于忽略被阻塞的信号，但SIGKILL 和SIGSTOP 不能被阻塞。
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
				(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}
		
		/* this is the scheduler proper: */
		/* 这里是调度程序的主要部分 */
		
		while (1) {
			c = -1;
			next = 0;
			i = NR_TASKS;
			p = &task[NR_TASKS];
			// 这段代码也是从任务数组的最后一个任务开始循环处理，并跳过不含任务的数组槽。比较每个就绪
			// 状态任务的counter（任务运行时间的递减滴答计数）值，哪一个值大，运行时间还不长，next 就
			// 指向哪个的任务号。
			while (--i) {
				if (!*--p)
					continue;
				if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
					c = (*p)->counter, next = i;	// 逗号运算符优先级小于赋值运算符
													// ，故此处等价于c = (*p)->counter; next = i;
			}
			// 如果比较得出有counter 值大于0 的结果，则退出循环，执行任务切换
			if (c) break;
			// 否则就根据每个任务的优先权值，更新每一个任务的counter 值，然后回到125 行重新比较。
			// counter 值的计算方式为counter = counter /2 + priority。[右边counter=0[??]]
			for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
				if (*p)
					(*p)->counter = ((*p)->counter >> 1) +
					(*p)->priority;
		}
		switch_to(next);	// 切换到任务号为next 的任务，并运行之。
}

// pause()系统调用。转换当前任务的状态为可中断的等待状态，并重新调度。
// 该系统调用将导致进程进入睡眠状态，直到收到一个信号。该信号用于终止进程或者使进程调用
// 一个信号捕获函数。只有当捕获了一个信号，并且信号捕获处理函数返回，pause()才会返回。
// 此时pause()返回值应该是-1，并且errno 被置为EINTR。这里还没有完全实现（直到0.95 版）。
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

// 把当前任务置为不可中断的等待状态，并让睡眠队列头的指针指向当前任务。
// 只有明确地唤醒时才会返回。该函数提供了进程与中断处理程序之间的同步机制。
// 函数参数*p 是放置等待任务的队列头指针。（参见列表后的说明）。[??]
// 注意：tmp是当前任务的内核栈中的局部变量，任务切换时内核堆栈指针被保存到TSS里的
// ss和esp字段，任务切换回来时，CPU的寄存器又重新被加载为该任务的内核栈的ss和esp.
// sleep时，都处于内核态
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;
	
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;								// 让tmp 指向已经在等待队列上的任务(如果有的话)。
	*p = current;							// 将睡眠队列头的等待指针指向当前任务
	current->state = TASK_UNINTERRUPTIBLE;	// 将当前任务置为不可中断的等待状态
	schedule();								// 重新调度。
	// 只有当这个等待任务被唤醒时，调度程序才又返回到这里，则表示进程已被明确地唤醒。
	// 既然大家都在等待同样的资源，那么在资源可用时，就有必要唤醒所有等待该资源的进程。该函数
	// 嵌套调用，也会嵌套唤醒所有等待该资源的进程。然后系统会根据这些进程的优先条件，重新调度
	// 应该由哪个进程首先使用资源。也即让这些进程竞争上岗。
	if (tmp)								// 若还存在等待的任务，则也将其置为就绪状态（唤醒）
		tmp->state=0;
}

// 将当前任务置为可中断的等待状态，并放入*p 指定的等待队列中。
// sleep时，都处于内核态
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
	// 如果等待队列中还有等待任务，并且队列头指针所指向的任务不是当前任务时，则将该等待任务置为
	// 可运行的就绪状态，并重新执行调度程序。当指针*p 所指向的不是当前任务时，表示在当前任务被放
	// 入队列后，又有新的任务被插入等待队列中，因此，既然本任务是可中断的，就应该首先执行所有
	// 其它的等待任务。
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	// 下面一句代码有误[??]，应该是*p = tmp，让队列头指针指向其余等待任务，否则在当前任务之前插入
	// 等待队列的任务均被抹掉了。
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

// 唤醒指定任务*p。
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;		// 此语句错误[??]，同interruptible_sleep_on()中的注释。
	}
}

/*
* OK, here are some floppy things that shouldn't be in the kernel
* proper. They are here because the floppy needs a timer, and this
* was the easiest way of doing it.
*/
/*
 * 好了，从这里开始是一些有关软盘的子程序，本不应该放在内核的主要部分中的。将它们放在这里
 * 是因为软驱需要一个时钟，而放在这里是最方便的办法。
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;	// 数字输出寄存器(初值：允许DMA 和请求中断、启动FDC)。

// 指定软盘到正常运转状态所需延迟滴答数（时间）。
// nr -- 软驱号(0-3)，返回值为滴答数。
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

// 等待指定软驱马达启动所需时间。
void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

// 置关闭相应软驱马达停转定时器（3 秒）。
void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

// 软盘定时处理子程序。更新马达启动定时值和马达关闭停转计时值。该子程序是在时钟定时
// 中断中被调用，因此每一个滴答(10ms)被调用一次，更新马达开启或停转定时器的值。如果某
// 一个马达停转定时到，则将数字输出寄存器马达启动位复位。
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

#define TIME_REQUESTS 64	// 最多可有64 个定时器（64 个任务）。

// 定时器链表结构和定时器数组。
static struct timer_list {
	long jiffies;					// 定时滴答数。
	void (*fn)();					// 定时处理程序。
	struct timer_list * next;		// 下一个定时器的指针，使定时器连成链表
} timer_list[TIME_REQUESTS], * next_timer = NULL;	// next_timer 是定时器链表的头指针

// 添加定时器。输入参数为指定的定时值(滴答数)和相应的处理程序指针。
// jiffies – 以10 毫秒计的滴答数；*fn()- 定时时间到时执行的函数。
// 【注意：此函数没考虑周全，有修改】
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;
	
	if (!fn)			// 如果定时处理程序指针为空，则退出。
		return;
	cli();				// 关中断
	if (jiffies <= 0)	// 如果定时值<=0，则立刻调用其处理程序。并且该定时器不加入链表中。
		(fn)();
	else {
		// 从定时器数组中，找一个空闲项。
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)		// 若处理函数指针为空，则是空项
				break;
			
			// 如果已经用完了定时器数组，则系统崩溃。
			if (p >= timer_list + TIME_REQUESTS)
				panic("No more time requests free");	// 显示出错信息，并运行文件同步函数，然后进入死循环
			// 插入到定时器链表头部，next_timer 是定时器链表的头指针
			p->fn = fn;
			p->jiffies = jiffies;
			p->next = next_timer;
			next_timer = p;
			// 链表项按定时值从小到大排序。在排序时减去排在前面需要的滴答数，这样在处理定时器时只要
			// 查看链表头的第一项的定时是否到期即可。[ [??]错误!!! 这段程序好象没有考虑周全。如果新插入的定时
			// 器值 小于 原来头一个定时器值时，或者说新定时器只要最后不是被移动到链表尾部，就应该将新定时
			// 器后面的第一个定时器值均减去新定时器最终的定时值。修改参见下面注释的代码]
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
			// 修改：在此处添加如下代码
			// struct timer_list * temp = p;
			// if(p->next)
			// {
			// 	p->jiffies -= temp->jiffies;
			//	p = p->next;
			// }
	}
	sti();				// 开中断
}

// 时钟中断C 函数处理程序，在kernel/system_call.s 中的_timer_interrupt被调用。
// 参数cpl 是当前特权级0 或3，0 表示内核代码在执行。
// 对于一个进程由于执行时间片用完时，则进行任务切换。并执行一个计时更新工作。
void do_timer(long cpl)
{
	extern int beepcount;			// 扬声器发声时间滴答数(kernel/chr_drv/console.c)
	extern void sysbeepstop(void);	// 关闭扬声器(kernel/chr_drv/console.c)
	
	// 如果发声计数次数到，则关闭发声。(向0x61 口发送命令，复位位0 和1。位0 控制8253
	// 计数器2 的工作，位1 控制扬声器)。
	if (beepcount)
		if (!--beepcount)
			sysbeepstop();
	// 如果当前特权级(cpl)为0（最高，表示是内核程序在工作），则将超级用户运行时间stime 递增；
	// 如果cpl > 0，则表示是一般用户程序在工作，增加utime。
	if (cpl)
		current->utime++;
	else
		current->stime++;
	// 如果有用户的定时器存在，则将链表第1 个定时器的值减1。如果已等于0，则调用相应的处理
	// 程序，并将该处理程序指针置为空。然后去掉该项定时器。
	if (next_timer) {					// next_timer 是定时器链表的头指针
		next_timer->jiffies--;			// 每次时钟中断，只将定时器链表头部定时器的嘀嗒数减1
		while (next_timer && next_timer->jiffies <= 0) {	// 可能多个定时器同时到时，故循环
			void (*fn)(void);			// 这里插入了一个函数指针定义
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();						// 调用处理函数
		}
	}
	// 如果当前软盘控制器FDC 的数字输出寄存器中马达(电动机)启动位有置位的，则执行软盘定时程序。
	if (current_DOR & 0xf0)
		do_floppy_timer();

	if ((--current->counter)>0) return;	// 如果进程运行时间还没用完，则退出。
	current->counter=0;
	if (!cpl) return;					// 对于内核程序（超级用户程序），不依赖counter 值进行调度
	schedule();							// 进程调度
}

// 系统调用功能 - 设置报警定时时间值(秒)。
// 如果已经设置过alarm 值，则返回旧值，否则返回0。
int sys_alarm(long seconds)
{
	int old = current->alarm;
	
	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

// 取当前进程号pid。
int sys_getpid(void)
{
	return current->pid;
}

// 取父进程号ppid。
int sys_getppid(void)
{
	return current->father;
}

// 取用户号uid。
int sys_getuid(void)
{
	return current->uid;
}

// 取euid。
int sys_geteuid(void)
{
	return current->euid;
}

// 取组号gid。
int sys_getgid(void)
{
	return current->gid;
}

// 取egid。
int sys_getegid(void)
{
	return current->egid;
}

// 系统调用功能 -- 降低对CPU 的使用优先权（有人会用吗？?）。
// 应该限制increment 大于0，否则的话,可使优先权增大！！
int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

// 调度程序初始化。--被main()调用
// 此处初始化分为3点：
// 1、将已经设计好的进程0管理结构task_struct与全局描述符表相挂接，并对全局描述符表、进程槽以及与进程调度
//    相关的寄存器进行初始化设置
// 2、对时钟中断进行设置，以便进行基于时间片的进程轮流调度
// 3、通过set_system_gate将system_call（系统调用总入口）与IDT相挂接，使进程0具备处理系统调用的能力
void sched_init(void)
{
	int i;
	struct desc_struct * p;	// 全局或中断描述符结构指针 typedef struct desc_struct{unsigned long a,b;}desc_struct[256];
	
	if (sizeof(struct sigaction) != 16)	// 存放有关状态的结构
		panic("Struct sigaction MUST be 16 bytes");
	// 设置任务0的任务状态段描述符和局部数据表描述符--在include/asm/system.h
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	// 清除任务数组和描述符表项（i=1,不包含任务0）
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
	/* Clear NT, so that we won't have troubles with that later on */
	// 清除标志寄存器中的位NT。NT标志位用于控制程序的递归调用。当NT置位时，当前中断任务执行iret时就会引起任务切换。
	// NT指出TSS中的back_link字段是否有效
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	
	// 注意!!!是将GDT中相应的LDT描述符的选择符加载到ldtr。只明确加载这1次，以后任务的LDT的加载
	// ，是CPU根据TSS中的LDT项自动加载
	ltr(0);										// 将任务0的TSS加载到任务寄存器tr
	lldt(0);									// 将局部描述符表加载到局部描述符表寄存器
	
	// 初始化8253定时器
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */	// LATCH宏定义在sched.c中#define LATCH (1193180/HZ),每10毫秒一次时钟中断
	outb(LATCH >> 8 , 0x40);	/* MSB */
	// 时钟中断程序挂接
	set_intr_gate(0x20,&timer_interrupt);		// 挂接timer_interrupt()时钟中断函数
	outb(inb_p(0x21)&~0x01,0x21);				// 允许时钟中断
	// 将system_call（系统调用总入口）与IDT相挂接，使进程0具备处理系统调用的能力
	set_system_gate(0x80,&system_call);			// 设置系统调用中断门
}
