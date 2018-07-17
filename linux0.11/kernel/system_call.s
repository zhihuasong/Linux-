/*
 *  linux/kernel/system_call.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  system_call.s  contains the system-call low-level handling routines.
 * This also contains the timer-interrupt handler, as some of the code is
 * the same. The hd- and flopppy-interrupts are also here.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call. Ordinary interrupts
 * don't handle signal-recognition, as that would clutter them up totally
 * unnecessarily.
 *
 * Stack layout in 'ret_from_system_call':
 *
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */

SIG_CHLD	= 17

EAX		= 0x00
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28
OLDSS		= 0x2C

state	= 0		# these are offsets into the task-struct.
counter	= 4
priority = 8
signal	= 12
sigaction = 16		# MUST be 16 (=len of sigaction)
blocked = (33*16)

# offsets within sigaction
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12

nr_system_calls = 72

/*
 * Ok, I get parallel printer interrupts while using the floppy for some
 * strange reason. Urgel. Now I just ignore them.
 */
.globl _system_call,_sys_fork,_timer_interrupt,_sys_execve
.globl _hd_interrupt,_floppy_interrupt,_parallel_interrupt
.globl _device_not_available, _coprocessor_error

# int 0x80 --linux 系统调用入口点(调用中断int 0x80，eax 中是调用号)
.align 2
bad_sys_call:
	movl $-1,%eax
	iret
.align 2
reschedule:
	pushl $ret_from_sys_call
	jmp _schedule
.align 2
_system_call:
	cmpl $nr_system_calls-1,%eax	# 调用号如果超出范围的话就在eax 中置-1 并退出
	ja bad_sys_call
	push %ds			# 保存原段寄存器值。【注意：此处这一系列的压栈动作（包括中断时，CPU硬件压入的ss,esp,eflags								
	push %es			# ,cs,eip），则是用于初始化子进程“任务状态描述符表TSS”中的数据】
	push %fs
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters	# ebx,ecx,edx中放着系统调用相应的C语言函数的调用参数
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds			# ds,es 指向内核数据段(全局描述符表中数据段描述符)。
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs			# fs 指向局部数据段(局部描述符表中数据段描述符)。
	# 下面这句操作数的含义是：调用地址 = _sys_call_table + %eax * 4，即转到sys_fork标号执行。
	# 对应的C 程序中的sys_call_table 在include/linux/sys.h 中，其中定义了一个包括72 个
	# 系统调用C 处理函数的地址数组表。
	call _sys_call_table(,%eax,4)	# 用Intel汇编语法则为：call [_sys_call_table+eax*4]
	pushl %eax
	movl _current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule
	# 以下这段代码执行从系统调用C 函数返回后，对信号量进行识别处理。
ret_from_sys_call:
	# 首先判别当前任务是否是初始任务task0（这种情况只限于进程0，其余进程都要处理信号），如果是则不必对
	# 其进行信号量方面的处理，直接返回。_task 对应C 程序中的task[]数组，直接引用task 相当于引用task[0]。
	movl _current,%eax		# task[0] cannot have signals
	cmpl _task,%eax
	je 3f					# 向前(forward)跳转到标号3
	cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
	jne 3f
	cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
	jne 3f
	movl signal(%eax),%ebx
	movl blocked(%eax),%ecx
	notl %ecx
	andl %ebx,%ecx
	bsfl %ecx,%ecx
	je 3f
	btrl %ecx,%ebx
	movl %ebx,signal(%eax)
	incl %ecx
	pushl %ecx
	call _do_signal
	popl %eax
3:	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

.align 2
_coprocessor_error:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	jmp _math_error

.align 2
_device_not_available:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	clts				# clear TS so that we can use math
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)
	je _math_state_restore
	pushl %ebp
	pushl %esi
	pushl %edi
	call _math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret

# int32 -- (int 0x20) 时钟中断处理程序。中断频率被设置为100Hz(include/linux/sched.h,5)，
# 定时芯片8253/8254 是在(kernel/sched.c,406)处初始化的。因此这里jiffies 每10 毫秒加1。
# 这段代码将jiffies 增1，发送结束中断指令给8259 控制器，然后用当前特权级作为参数调用
# C 函数do_timer(long CPL)。当调用返回时转去检测并处理信号。
.align 2
_timer_interrupt:
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax		# ds,es 置为指向内核数据段（DGT表第2项）
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax		# fs 置为指向局部数据段（出错程序的数据段）。
	mov %ax,%fs
	incl _jiffies		# 定时器嘀嗒数加1
	# 由于初始化中断控制芯片时没有采用自动EOI，所以这里需要发指令结束该硬件中断。
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	# 下面3 句从选择符中取出当前特权级别(0 或3)并压入堆栈，作为do_timer 的参数。
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)	# 选择符的低2位时特权级别，3看做二进制的11
	pushl %eax
	# do_timer(cpl)执行任务切换、计时等工作，在kernel/shched.c实现。
	call _do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call	# 处理当前进程接收到的的信号

.align 2
_sys_execve:
	lea EIP(%esp),%eax
	pushl %eax
	call _do_execve
	addl $4,%esp
	ret

# sys_fork()调用，用于创建子进程，是system_call 功能2。原形在include/linux/sys.h 中。
# 首先调用C 函数find_empty_process()，取得一个进程号pid。若返回负数则说明目前任务数组
# 已满。然后调用copy_process()复制进程。
.align 2
_sys_fork:
	call _find_empty_process	# 调用find_empty_process()(kernel/fork.c),返回值在eax（task数组空闲位置的索引）中
	testl %eax,%eax
	js 1f						# 若符号标志位SF为1，则表明返回值为负，创建进程失败!!!
	push %gs					# 将寄存器数值入栈，为进一步初始化子进程的“任务状态描述符表TSS”中的数据
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call _copy_process			# 调用C 函数copy_process()(kernel/fork.c)。
	addl $20,%esp				# 丢弃这里所有压栈内容。
1:	ret

_hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	xorl %edx,%edx
	xchgl _do_hd,%edx
	testl %edx,%edx
	jne 1f
	movl $_unexpected_hd_interrupt,%edx
1:	outb %al,$0x20
	call *%edx		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

_floppy_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0x20		# EOI to interrupt controller #1
	xorl %eax,%eax
	xchgl _do_floppy,%eax
	testl %eax,%eax
	jne 1f
	movl $_unexpected_floppy_interrupt,%eax
1:	call *%eax		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

_parallel_interrupt:
	pushl %eax
	movb $0x20,%al
	outb %al,$0x20
	popl %eax
	iret
