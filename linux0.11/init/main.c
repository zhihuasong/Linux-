/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__		// ����ñ�����Ϊ�˰���������unistd.h�е���Ƕ���������Ϣ
#include <unistd.h>		// ��׼���ų����������ļ������ļ��ж����˸��ַ��ų��������ͣ��������˸��ֺ�����
						// ���������__LIBRARY__���򻹰���ϵͳ���úź���Ƕ���_syscall0()��
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
/*
 * ������Ҫ������Щ��Ƕ���- ���ں˿ռ䴴������(forking)������û��дʱ��
 * �ƣ�COPY ON WRITE��!!!ֱ��һ��ִ��execve ���á���Զ�ջ���ܴ������⡣��
 * ��ķ�������fork()����֮����main()ʹ���κζ�ջ����˾Ͳ����к�����
 * ��- ����ζ��fork ҲҪʹ����Ƕ�Ĵ��룬���������ڴ�fork()�˳�ʱ��Ҫʹ�ö�ջ�ˡ�
 *
 * ʵ����ֻ��pause ��fork ��Ҫʹ����Ƕ��ʽ���Ա�֤��main()�в���Ū�Ҷ�ջ��
 * ��������ͬʱ������������һЩ������
 */
// ��include/unistd.h�еĺ궨�庯��
static inline _syscall0(int,fork)	// int fork()ϵͳ���ã��������̣�syscall0��0��ʾ�޲�����1��ʾ1������
static inline _syscall0(int,pause)	// int pause()ϵͳ���ã���ͣ����ִ��
static inline _syscall1(int,setup,void *,BIOS)	// int setup(void * BIOS)ϵͳ����
static inline _syscall0(int,sync)	// int sync()ϵͳ���ã������ļ�ϵͳ

#include <linux/tty.h>				// tty,h�������й�tty_io������ͨ�ŷ���Ĳ���������
#include <linux/sched.h>			// ���ȳ���ͷ�ļ�������������ṹtask_struct����һ����ʼ����������ݡ�
									// ����һЩ�Ժ����ʽ������й��������������úͻ�ȡ��Ƕ��ʽ��ຯ������
#include <linux/head.h>				// �����˶��������ļ򵥽ṹ���ͼ���ѡ�������
#include <asm/system.h>				// ϵͳͷ�ļ����Ժ����ʽ����������й����û��޸�������/�ж��ŵȵ�Ƕ�����ӳ���
#include <asm/io.h>					// �Ժ��Ƕ���������ʽ�����˶�io�˿ڲ����ĺ���

#include <stddef.h>					// ��׼����ͷ�ļ���������NULL��offsetof(TYPE,MEMBER)
#include <stdarg.h>					// ��׼����ͷ�ļ����Ժ����ʽ�����˱��������б���Ҫ˵����һ�����ͣ�va_list��
									// �������꣨va_start,va_arg��va_end��,vsprintf,vprintf,vfprintf
#include <unistd.h>					// �μ����ļ�ͷ��
#include <fcntl.h>					// �ļ�����ͷ�ļ��������ļ������������Ĳ������Ƴ������ŵĶ���
#include <sys/types.h>				// �����˻�����ϵͳ��������

#include <linux/fs.h>				// �ļ�ϵͳͷ�ļ��������ļ���ṹ��file,buffer_head,m_inode�ȣ�

static char printbuf[1024];			// ��̬�ַ������飬�����ں���ʾ��Ϣ�Ļ���

extern int vsprintf();				// �͸�ʽ�������һ�ַ����У���kernel/vsprintf.c��
extern void init(void);				// ��ʼ�����ڱ��ļ���
extern void blk_dev_init(void);		// ���豸��ʼ���ӳ���kernel/blk_drv/ll_rw_blk.c��
extern void chr_dev_init(void);		// �ַ��豸��ʼ����kernel/chr_drv/tty_io.c��
extern void hd_init(void);			// Ӳ�̳�ʼ������kernel/blk_drv/hd.c��
extern void floppy_init(void);		// ������ʼ������kernel/blk_drv/floppy.c��
extern void mem_init(long start, long end);			// �ڴ�����ʼ����mm/memory.c��
extern long rd_init(long mem_start, int length);	// �����̳�ʼ����kernel/blk_drv/ramdisk.c��
extern long kernel_mktime(struct tm * tm);			// �����ں�ʱ�䣨�룩
extern long startup_time;							// �ں�����ʱ�䣨����ʱ�䣩���룩

/*
 * This is set up by the setup-routine at boot-time
 */
// ������Щ��������setup.s�����ڿ�������ʱ�Ѿ����õ�
#define EXT_MEM_K (*(unsigned short *)0x90002)		// 1MB�Ժ����չ�ڴ��С��KB��
#define DRIVE_INFO (*(struct drive_info *)0x90080)	// Ӳ�̲������ַ
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)	// ���ļ�ϵͳ�����豸��

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
// ��κ��ȡCMOSʵʱʱ����Ϣ,0x70�ǵ�ַ�˿�(����д���ַ),0x80|addr��Ҫ��ȡ��CMOS�ڴ��ַ
// ,0x71�����ݶ˿�(���Զ���д����)
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

// ��BCD ��ת�������֡�
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

// ���ӳ���ȡCMOSʱ�ӣ������ÿ���ʱ��startup_time��Ϊ��1970-1-1-0 ʱ�𵽿���ʱ��������
static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;					// �ڴ�ĩ��λ�ã�����ڴ棩
static long buffer_memory_end = 0;			// ������ĩ��λ��
static long main_memory_start = 0;			// ���ڴ�����ʼλ��

struct drive_info { char dummy[32]; } drive_info;	// ���ڱ��ݴ��̲�����2�ţ�

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
 	ROOT_DEV = ORIG_ROOT_DEV;				// ���ݸ��豸��
 	drive_info = DRIVE_INFO;				// ���ݴ��̲�����
	memory_end = (1<<20) + (EXT_MEM_K<<10);
	memory_end &= 0xfffff000;
	if (memory_end > 16*1024*1024)			// ���ڴ����16MB����16MB��
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024)			// ���ڴ����12MB�������û�����ĩ��Ϊ4MB 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)		// ���ڴ����6MB�������û�����ĩ��Ϊ2MB
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;	// �������û�����ĩ��Ϊ1MB
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK								// �������������̣����ʼ�������̡����������ڴ�
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);// ��kernel/blk_drv/ramdisk.c
#endif
	// �˴��Ƕ��ڴ����ṹ mem_map ���г�ʼ����--��mm/memory.c
	// ϵͳ��1MB�Ժ���ڴ涼�ǲ��÷�ҳ����ģ���Ϊ��������̶�1MB���µ��ں˴��������в�����������ϵͳ
	// ͨ��һ������ mem_map �������¼ÿһ��ҳ���ʹ�ô�����ϵͳ�Ժ��ʹ�ô���Ϊ0��ҳ����Ϊ����ҳ��
	mem_init(main_memory_start,memory_end);

	trap_init();			// �쳣�������жϷ������ҽ�--��kernel/traps.c
	blk_dev_init();			// ��ʼ�����豸������ṹ--��kernel/blk_drv/ll_rw_blk.c
	chr_dev_init();			// �ַ��豸��ʼ��--[�պ���]--��kernel/chr_drv/tty_io.c
	tty_init();				// �뽨���˻�����������ص�������жϷ������ҽ�--��kernel/chr_drv/tty_io.c
	time_init();			// ���ÿ�������ʱ�䣨startup_time��--main.c
	// ���ȳ����ʼ����--��kernel/sched.c
	// �˴���ʼ����Ϊ3�㣺
	// 1�����Ѿ���ƺõĽ���0����ṹtask_struct��ȫ������������ҽӣ�����ȫ�������������̲��Լ�����̵���
	//    ��صļĴ������г�ʼ������
	// 2����ʱ���жϽ������ã��Ա���л���ʱ��Ƭ�Ľ�����������
	// 3��ͨ��set_system_gate��system_call��ϵͳ��������ڣ���IDT��ҽӣ�ʹ����0�߱�����ϵͳ���õ�����
	sched_init();
	// ��ʼ������������ṹ��--��fs/buffer.c
	// ϵͳ�ֱ�ͨ�� ���б� �� ��ϣ�� �Ի��������й�������Ҫ�����������ݽṹ���г�ʼ�����ȶԿ��б����ã�
	// ʹ���Ϊ��˫�����������еĳ�Ա��������Ϊ0�����б��ڴ�ռ����ں�֮�����ں��ڴ�ռ��С���
	// hash_table����307���������ΪNULL
	buffer_init(buffer_memory_end);
	// ��Ӳ�̲�����ʽ���г�ʼ�����á�--��kernel/blk_drv/hd.c
	// 1����Ӳ�������������� do_hd_request �� blk_dev ���ƽṹ��ҽӣ�
	// 2����Ӳ���жϷ������ hd_interrupt �� IDT ��ҽӣ�
	// 3����λ��8259A����λ(int2)�������Ƭ�����ж�����
	// 4����λӲ���ж��ж���������λ���ڴ�Ƭ�ϣ�������Ӳ�̿����������ж�����
	hd_init();
	// �����̲�����ʽ���г�ʼ�����á�--��kernel/blk_drv/floppy.c
	// 1�������������������� do_fd_request �� blk_dev ���ƽṹ��ҽӣ�
	// 2���������жϷ������ floppy_interrupt �� IDT ��ҽӣ�
	// 3����λ���̵��ж���������λ���������̿����������ж�����
	// ��ע�⡿�˴��������жϷ��������IDT�ҽӣ��ǹ���32λ����ģʽ�жϷ�����ϵ�����һ������һ��
	//         ����ɱ�־��Linux-0.11����ģʽ�µ��жϷ�����ϵ���׹������!!!!!!
	floppy_init();
	sti();								// ���жϡ�����0(��δ����)������������׼���Ļ�������ʽ��ʼ������!!!!!!
	// �ƶ����û�ģʽ���С��ú�����iretָ��ʵ�ִ��ں�ģʽ�ƶ�����ʼ����0ȥִ��!!!!!!--��include/asm/system.h
	move_to_user_mode();
	if (!fork()) {		/* we count on this going ok */	// ��������1!!!!!! fork()Ϊ�궨�庯���������ڱ��ļ��ϲ�
		init();
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
/*
 * ע��!! �����κ�����������'pause()'����ζ�����Ǳ���ȴ��յ�һ���źŲŻ᷵
 * �ؾ�������̬��������0��task0����Ψһ������������μ�'schedule()'������Ϊ��
 * ��0 ���κο���ʱ���ﶼ�ᱻ�����û����������������ʱ����
 * ��˶�������0'pause()'����ζ�����Ƿ������鿴�Ƿ�����������������У����û
 * �еĻ����Ǿͻص����һֱѭ��ִ��'pause()'��
 */
	for(;;) pause();					// for(;;) �൱�� while(1),pause()Ϊ�궨�庯�����μ����ļ��ϲ�
}

// ������ʽ����Ϣ���������׼����豸stdout(1)��������ָ��Ļ����ʾ������'*fmt'
// ָ����������õĸ�ʽ���μ����ֱ�׼C �����鼮�����ӳ���������vsprintf ���ʹ
// �õ�һ�����ӡ�
// �ó���ʹ��vsprintf()����ʽ�����ַ�������printbuf ��������Ȼ����write()
// ���������������������׼�豸��1--stdout����
static int printf(const char *fmt, ...)	// ��������������1��
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };		// ����ִ�г���ʱ�������ַ������顣
static char * envp_rc[] = { "HOME=/", NULL };		// ����ִ�г���ʱ�Ļ����ַ������顣

static char * argv[] = { "-/bin/sh",NULL };			// ͬ��
static char * envp[] = { "HOME=/usr/root", NULL };

// init()��������������0�������ӽ��̣�����1���С������ȶԵ�һ��Ҫִ�еĳ���shell���Ļ������г�ʼ��
// ��Ȼ����ظĳ���ִ��֮��
// ע�⣺��main()��ʼ�����ڽ�����Ȩ����Ϊ3���䲻����ִ���ں˺����������е��ں˵���Ҫô��ϵͳ����(int 0x80)
// ��Ҫô�ǿ⺯������ʵ�⺯������ͨ��ϵͳ����ʵ�ֵ�,ֻ������ϵͳ���õ��������.
void init(void)
{
	int pid,i;

	// ��ȡӲ�̲���������������Ϣ�����������̺Ͱ�װ���ļ�ϵͳ�豸��
	// �ú������ڱ��ļ��ϲ��궨��ģ���Ӧ������sys_setup()����kernel/blk_drv/hd.c��
	setup((void *) &drive_info);
	// open()������ lib/open.c���ö�д���ʷ�ʽ���豸��/dev/tty0���������Ӧ�ն˿���̨��
	// ���صľ����0 -- stdin ��׼�����豸��
	(void) open("/dev/tty0",O_RDWR,0);
	// dup()�궨�庯������ lib/dup.c��
	(void) dup(0);					// ���ƾ�����������1 ��-- stdout ��׼����豸��
	(void) dup(0);					// ���ƾ�����������2 ��-- stderr ��׼��������豸��
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);		// ��ӡ���������������ֽ�����ÿ��1024 �ֽڡ�
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);	//�����ڴ��ֽ�����
	// ����fork()���ڴ���һ���ӽ���(������)�����ڱ��������ӽ��̣�fork()������0 ֵ��
	// ����ԭ����(������)�������ӽ��̵Ľ��̺š�����if (!(pid=fork())) {...} �����ӽ���ִ�е����ݡ�
	// ���ӽ��̹ر��˾��0(stdin)����ֻ����ʽ��/etc/rc �ļ�����ִ��/bin/sh ��������������
	// ���������ֱ���argv_rc ��envp_rc ���������
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);				// ������ļ�ʧ�ܣ����˳�(/lib/_exit.c)��
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);					// ��execve()ִ��ʧ�����˳�(������2,���ļ���Ŀ¼�����ڡ�)��
	}
	// �����Ǹ�����ִ�е���䡣wait()�ǵȴ��ӽ���ֹͣ����ֹ���䷵��ֵӦ���ӽ��̵�
	// ���̺�(pid)��������������Ǹ����̵ȴ��ӽ��̵Ľ�����&i �Ǵ�ŷ���״̬��Ϣ��
	// λ�á����wait()����ֵ�������ӽ��̺ţ�������ȴ���
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	// --
	// ���ִ�е����˵���մ������ӽ��̵�ִ����ֹͣ����ֹ�ˡ�����ѭ���������ٴ���
	// һ���ӽ��̣������������ʾ����ʼ�����򴴽��ӽ���ʧ�ܡ�����Ϣ������ִ�С���
	// �����������ӽ��̹ر�������ǰ�������ľ��(stdin, stdout, stderr)���´���һ��
	// �Ự�����ý�����ţ�Ȼ�����´�/dev/tty0 ��Ϊstdin�������Ƴ�stdout ��stderr��
	// �ٴ�ִ��ϵͳ���ͳ���/bin/sh�������ִ����ѡ�õĲ����ͻ���������ѡ��һ�ף������棩��
	// Ȼ�󸸽����ٴ�����wait()�ȴ�������ӽ�����ֹͣ��ִ�У����ڱ�׼�������ʾ������Ϣ
	//		���ӽ���pid ֹͣ�����У���������i����
	// Ȼ�����������ȥ�����γɡ�����ѭ����
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}
