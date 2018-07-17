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

# int 0x80 --linux ϵͳ������ڵ�(�����ж�int 0x80��eax ���ǵ��ú�)
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
	cmpl $nr_system_calls-1,%eax	# ���ú����������Χ�Ļ�����eax ����-1 ���˳�
	ja bad_sys_call
	push %ds			# ����ԭ�μĴ���ֵ����ע�⣺�˴���һϵ�е�ѹջ�����������ж�ʱ��CPUӲ��ѹ���ss,esp,eflags								
	push %es			# ,cs,eip�����������ڳ�ʼ���ӽ��̡�����״̬��������TSS���е����ݡ�
	push %fs
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters	# ebx,ecx,edx�з���ϵͳ������Ӧ��C���Ժ����ĵ��ò���
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds			# ds,es ָ���ں����ݶ�(ȫ���������������ݶ�������)��
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs			# fs ָ��ֲ����ݶ�(�ֲ��������������ݶ�������)��
	# �������������ĺ����ǣ����õ�ַ = _sys_call_table + %eax * 4����ת��sys_fork���ִ�С�
	# ��Ӧ��C �����е�sys_call_table ��include/linux/sys.h �У����ж�����һ������72 ��
	# ϵͳ����C �������ĵ�ַ�����
	call _sys_call_table(,%eax,4)	# ��Intel����﷨��Ϊ��call [_sys_call_table+eax*4]
	pushl %eax
	movl _current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule
	# ������δ���ִ�д�ϵͳ����C �������غ󣬶��ź�������ʶ����
ret_from_sys_call:
	# �����б�ǰ�����Ƿ��ǳ�ʼ����task0���������ֻ���ڽ���0��������̶�Ҫ�����źţ���������򲻱ض�
	# ������ź�������Ĵ���ֱ�ӷ��ء�_task ��ӦC �����е�task[]���飬ֱ������task �൱������task[0]��
	movl _current,%eax		# task[0] cannot have signals
	cmpl _task,%eax
	je 3f					# ��ǰ(forward)��ת�����3
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

# int32 -- (int 0x20) ʱ���жϴ�������ж�Ƶ�ʱ�����Ϊ100Hz(include/linux/sched.h,5)��
# ��ʱоƬ8253/8254 ����(kernel/sched.c,406)����ʼ���ġ��������jiffies ÿ10 �����1��
# ��δ��뽫jiffies ��1�����ͽ����ж�ָ���8259 ��������Ȼ���õ�ǰ��Ȩ����Ϊ��������
# C ����do_timer(long CPL)�������÷���ʱתȥ��Ⲣ�����źš�
.align 2
_timer_interrupt:
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax		# ds,es ��Ϊָ���ں����ݶΣ�DGT���2�
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax		# fs ��Ϊָ��ֲ����ݶΣ������������ݶΣ���
	mov %ax,%fs
	incl _jiffies		# ��ʱ���������1
	# ���ڳ�ʼ���жϿ���оƬʱû�в����Զ�EOI������������Ҫ��ָ�������Ӳ���жϡ�
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	# ����3 ���ѡ�����ȡ����ǰ��Ȩ����(0 ��3)��ѹ���ջ����Ϊdo_timer �Ĳ�����
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)	# ѡ����ĵ�2λʱ��Ȩ����3���������Ƶ�11
	pushl %eax
	# do_timer(cpl)ִ�������л�����ʱ�ȹ�������kernel/shched.cʵ�֡�
	call _do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call	# ����ǰ���̽��յ��ĵ��ź�

.align 2
_sys_execve:
	lea EIP(%esp),%eax
	pushl %eax
	call _do_execve
	addl $4,%esp
	ret

# sys_fork()���ã����ڴ����ӽ��̣���system_call ����2��ԭ����include/linux/sys.h �С�
# ���ȵ���C ����find_empty_process()��ȡ��һ�����̺�pid�������ظ�����˵��Ŀǰ��������
# ������Ȼ�����copy_process()���ƽ��̡�
.align 2
_sys_fork:
	call _find_empty_process	# ����find_empty_process()(kernel/fork.c),����ֵ��eax��task�������λ�õ���������
	testl %eax,%eax
	js 1f						# �����ű�־λSFΪ1�����������ֵΪ������������ʧ��!!!
	push %gs					# ���Ĵ�����ֵ��ջ��Ϊ��һ����ʼ���ӽ��̵ġ�����״̬��������TSS���е�����
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call _copy_process			# ����C ����copy_process()(kernel/fork.c)��
	addl $20,%esp				# ������������ѹջ���ݡ�
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
