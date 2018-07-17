#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

// ���ڽ���״̬�����飬�μ�linux���̿��������
#define TASK_RUNNING			0	// ����״̬
#define TASK_INTERRUPTIBLE		1	// ���ж�˯��״̬
#define TASK_UNINTERRUPTIBLE	2	// �����ж�˯��״̬
#define TASK_ZOMBIE				3	// ����״̬
#define TASK_STOPPED			4	// ��ͣ״̬

#ifndef NULL
#define NULL ((void *) 0)
#endif

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
extern void panic(const char * str);
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)();

struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

// TSS������״̬�Σ�����CPU��ȡ���޸ĵģ����ڱ������������ġ�����μ��ں�ѧϰ�ʼ�
struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

// ���������񣨽��̣����ݽṹ�����Ϊ������������[�������ȫ����Ա��ע��]
// ==========================
// long state ���������״̬��-1 �������У�0 ������(����)��>0 ��ֹͣ����
// long counter ��������ʱ�����(�ݼ�)���δ�����������ʱ��Ƭ��
// long priority ����������������ʼ����ʱcounter = priority��Խ������Խ����
// long signal �źš���λͼ��ÿ������λ����һ���źţ��ź�ֵ=λƫ��ֵ+1��
// struct sigaction sigaction[32] �ź�ִ�����Խṹ����Ӧ�źŽ�Ҫִ�еĲ����ͱ�־��Ϣ��
// long blocked �����ź������루��Ӧ�ź�λͼ����
// --------------------------
// int exit_code ����ִ��ֹͣ���˳��룬�丸���̻�ȡ��
// unsigned long start_code ����ε�ַ��
// unsigned long end_code ���볤�ȣ��ֽ�������
// unsigned long end_data ���볤�� + ���ݳ��ȣ��ֽ�������
// unsigned long brk �ܳ��ȣ��ֽ�������
// unsigned long start_stack ��ջ�ε�ַ��
// long pid ���̱�ʶ��(���̺�)��
// long father �����̺š�
// long pgrp ��������š�
// long session �Ự�š�
// long leader �Ự���졣
// unsigned short uid �û���ʶ�ţ��û�id����
// unsigned short euid ��Ч�û�id��
// unsigned short suid ������û�id��
// unsigned short gid ���ʶ�ţ���id����
// unsigned short egid ��Ч��id��
// unsigned short sgid �������id��
// long alarm ������ʱֵ���δ�������
// long utime �û�̬����ʱ�䣨�δ�������
// long stime ϵͳ̬����ʱ�䣨�δ�������
// long cutime �ӽ����û�̬����ʱ�䡣
// long cstime �ӽ���ϵͳ̬����ʱ�䡣
// long start_time ���̿�ʼ����ʱ�̡�
// unsigned short used_math ��־���Ƿ�ʹ����Э��������
// --------------------------
// int tty ����ʹ��tty �����豸�š�-1 ��ʾû��ʹ�á�
// unsigned short umask �ļ�������������λ��
// struct m_inode * pwd ��ǰ����Ŀ¼i �ڵ�ṹ��
// struct m_inode * root ��Ŀ¼i �ڵ�ṹ��
// struct m_inode * executable ִ���ļ�i �ڵ�ṹ��
// unsigned long close_on_exec ִ��ʱ�ر��ļ����λͼ��־�����μ�include/fcntl.h��
// struct file * filp[NR_OPEN] ����ʹ�õ��ļ����ṹ��
// --------------------------
// struct desc_struct ldt[3] ������ľֲ�����������0-�գ�1-�����cs��2-���ݺͶ�ջ��ds&ss��
// --------------------------
// struct tss_struct tss �����̵�����״̬����Ϣ�ṹ��
// ==========================
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	struct sigaction sigaction[32];
	long blocked;	/* bitmap of masked signals */
/* various fields */
	int exit_code;
	unsigned long start_code,end_code,end_data,brk,start_stack;
	long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	struct m_inode * executable;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
// INIT_TASK�������õ�1�����������Ӧ����task_struct�ĵ�1��(����0)��������Ϣ����ַ=0�����޳�=0x9ffff(=640KB)
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;

#define CURRENT_TIME (startup_time+jiffies/HZ)

extern void add_timer(long jiffies, void (*fn)(void));
extern void sleep_on(struct task_struct ** p);
extern void interruptible_sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct ** p);

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))
#define str(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
/*
* switch_to(n)���л���ǰ��������nr����n�����ȼ������n �ǲ��ǵ�ǰ����
* �������ʲôҲ�����˳�����������л���������������ϴ����У�ʹ�ù���ѧ
* Э�������Ļ������踴λ���ƼĴ���cr0 �е�TS ��־��
*/
// ���룺%0 - ��TSS ��ƫ�Ƶ�ַ(*&__tmp.a)�����������ã���û�и�ֵ�� %1 - �����TSS ��ѡ���ֵ(*&__tmp.b)��
// dx - ������n ��TSSѡ�����ecx - ������ָ��task[n]��
// ������ʱ���ݽṹ__tmp �У�a ��ֵ��32 λƫ��ֵ�����ã���b Ϊ��TSS ��ѡ������������л�ʱ��a ֵ
// û���ã����ԣ������ж��������ϴ�ִ���Ƿ�ʹ�ù�Э������ʱ����ͨ����������״̬�εĵ�ַ��
// ������last_task_used_math �����е�ʹ�ù�Э������������״̬�εĵ�ַ���бȽ϶������ġ�
#define switch_to(n) {\
struct {long a,b;} __tmp;			/*��Աb�����洢TSSѡ������Ա�ljmp֮����������л�*/\
__asm__("cmpl %%ecx,_current\n\t"	/*����n �ǵ�ǰ������?(current ==task[n]?)*/\
	"je 1f\n\t" \
	"movw %%dx,%1\n\t"				/*���������TSSѡ��� --> __tmp.b*/\
	"xchgl %%ecx,_current\n\t"		/*xchgl��������������*/\
	"ljmp %0\n\t"					/*ִ�г���ת��*&__tmp����������л�*/\
	// ע�⣺�������л�������Ż����ִ����������!!!
	// ������������ת�ˣ���������ͨ����ת��������ת�Ĳ����������ǵ�ַ����ѡ������������ǡ���ת��ִ��
	// ���������ˣ����ٴε���ִ�б�����ʱ�ʹ���һ������ִ�С�
	"cmpl %%ecx,_last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" /*��CR0��TS��־��ÿ����������ת��ʱ��TS=1������ת����ϣ�TS=0��TS=1ʱ������Э������������*/\
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b),/*��һ��"m" --> IP�Ĵ��� �ڶ���"m" --> CS�Ĵ�������ַǰ��*�ţ���ʾ���Ե�ַ*/ \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}

// ҳ���ַ��׼�������ں˴�����û���κεط�����!!��
#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

// ����λ�ڵ�ַaddr ���������еĸ�����ַ�ֶ�(����ַ��base)��
// %0 - ��ַaddr ƫ��2��%1 - ��ַaddr ƫ��4��%2 - ��ַaddr ƫ��7��edx - ����ַbase��
#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t"	/*��ַbase ��16 λ(λ15-0)->[addr+2]*/ \
		"rorl $16,%%edx\n\t"/*edx �л�ַ��16 λ(λ31-16) -> dx��rorlΪѭ������˫��*/ \
		"movb %%dl,%1\n\t"	/*��ַ��16 λ�еĵ�8 λ(λ23-16)->[addr+4]��*/ \
		"movb %%dh,%2"		/*��ַ��16 λ�еĸ�8 λ(λ31-24)->[addr+7]��*/ \
	::"m" (*((addr)+2)), \
	  "m" (*((addr)+4)), \
	  "m" (*((addr)+7)), \
	  "d" (base) \
	:"dx")

#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1" \
	::"m" (*(addr)), \
	  "m" (*((addr)+6)), \
	  "d" (limit) \
	:"dx")

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
	"movb %2,%%dl\n\t" \
	"shll $16,%%edx\n\t" \
	"movw %1,%%dx" \
	:"=d" (__base) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7))); \
__base;})

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif