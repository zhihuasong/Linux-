/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__		// 定义该变量是为了包括定义在unistd.h中的内嵌汇编代码等信息
#include <unistd.h>		// 标准符号常数与类型文件。该文件中定义了各种符号常数和类型，并声明了各种函数。
						// 如果定义了__LIBRARY__，则还包括系统调用号和内嵌汇编_syscall0()等
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
 * 我们需要下面这些内嵌语句- 从内核空间创建进程(forking)将导致没有写时复
 * 制（COPY ON WRITE）!!!直到一个执行execve 调用。这对堆栈可能带来问题。处
 * 理的方法是在fork()调用之后不让main()使用任何堆栈。因此就不能有函数调
 * 用- 这意味着fork 也要使用内嵌的代码，否则我们在从fork()退出时就要使用堆栈了。
 *
 * 实际上只有pause 和fork 需要使用内嵌方式，以保证从main()中不会弄乱堆栈，
 * 但是我们同时还定义了其它一些函数。
 */
// 在include/unistd.h中的宏定义函数
static inline _syscall0(int,fork)	// int fork()系统调用：创建进程，syscall0中0表示无参数，1表示1个参数
static inline _syscall0(int,pause)	// int pause()系统调用：暂停进程执行
static inline _syscall1(int,setup,void *,BIOS)	// int setup(void * BIOS)系统调用
static inline _syscall0(int,sync)	// int sync()系统调用：更新文件系统

#include <linux/tty.h>				// tty,h定义了有关tty_io，串行通信方面的参数、常数
#include <linux/sched.h>			// 调度程序头文件，定义了任务结构task_struct、第一个初始化任务的数据。
									// 还有一些以宏的形式定义的有关描述符参数设置和获取的嵌入式汇编函数程序
#include <linux/head.h>				// 定义了段描述符的简单结构，和几个选择符常量
#include <asm/system.h>				// 系统头文件。以宏的形式定义了许多有关设置或修改描述符/中断门等的嵌入汇编子程序
#include <asm/io.h>					// 以宏的嵌入汇编程序形式定义了对io端口操作的函数

#include <stddef.h>					// 标准定义头文件。定义了NULL，offsetof(TYPE,MEMBER)
#include <stdarg.h>					// 标准参数头文件。以宏的形式定义了变量参数列表，主要说明了一个类型（va_list）
									// 和三个宏（va_start,va_arg和va_end）,vsprintf,vprintf,vfprintf
#include <unistd.h>					// 参见本文件头部
#include <fcntl.h>					// 文件控制头文件。用于文件及其描述符的操作控制常数符号的定义
#include <sys/types.h>				// 定义了基本的系统数据类型

#include <linux/fs.h>				// 文件系统头文件。定义文件表结构（file,buffer_head,m_inode等）

static char printbuf[1024];			// 静态字符串数组，用作内核显示信息的缓存

extern int vsprintf();				// 送格式化输出到一字符串中（在kernel/vsprintf.c）
extern void init(void);				// 初始化，在本文件中
extern void blk_dev_init(void);		// 块设备初始化子程序（kernel/blk_drv/ll_rw_blk.c）
extern void chr_dev_init(void);		// 字符设备初始化（kernel/chr_drv/tty_io.c）
extern void hd_init(void);			// 硬盘初始化程序（kernel/blk_drv/hd.c）
extern void floppy_init(void);		// 软驱初始化程序（kernel/blk_drv/floppy.c）
extern void mem_init(long start, long end);			// 内存管理初始化（mm/memory.c）
extern long rd_init(long mem_start, int length);	// 虚拟盘初始化（kernel/blk_drv/ramdisk.c）
extern long kernel_mktime(struct tm * tm);			// 建立内核时间（秒）
extern long startup_time;							// 内核启动时间（开机时间）（秒）

/*
 * This is set up by the setup-routine at boot-time
 */
// 以下这些数据是由setup.s程序在开机引导时已经设置的
#define EXT_MEM_K (*(unsigned short *)0x90002)		// 1MB以后的扩展内存大小（KB）
#define DRIVE_INFO (*(struct drive_info *)0x90080)	// 硬盘参数表基址
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)	// 根文件系统所在设备号

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
// 这段宏读取CMOS实时时钟信息,0x70是地址端口(可以写入地址),0x80|addr是要读取的CMOS内存地址
// ,0x71是数据端口(可以读、写数据)
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

// 将BCD 码转换成数字。
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

// 该子程序取CMOS时钟，并设置开机时间startup_time（为从1970-1-1-0 时起到开机时的秒数）
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

static long memory_end = 0;					// 内存末端位置（最大内存）
static long buffer_memory_end = 0;			// 缓冲区末端位置
static long main_memory_start = 0;			// 主内存区起始位置

struct drive_info { char dummy[32]; } drive_info;	// 用于备份磁盘参数表（2张）

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
 	ROOT_DEV = ORIG_ROOT_DEV;				// 备份根设备号
 	drive_info = DRIVE_INFO;				// 备份磁盘参数表
	memory_end = (1<<20) + (EXT_MEM_K<<10);
	memory_end &= 0xfffff000;
	if (memory_end > 16*1024*1024)			// 若内存大于16MB，则按16MB计
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024)			// 若内存大于12MB，则设置缓冲区末端为4MB 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)		// 若内存大于6MB，则设置缓冲区末端为2MB
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;	// 否则，设置缓冲区末端为1MB
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK								// 若定义了虚拟盘，则初始化虚拟盘。并减少主内存
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);// 在kernel/blk_drv/ramdisk.c
#endif
	// 此处是对内存管理结构 mem_map 进行初始化。--在mm/memory.c
	// 系统对1MB以后的内存都是采用分页管理的（因为不允许进程对1MB以下的内核代码区进行操作），于是系统
	// 通过一个叫做 mem_map 的数组记录每一个页面的使用次数。系统以后把使用次数为0的页面视为空闲页面
	mem_init(main_memory_start,memory_end);

	trap_init();			// 异常处理类中断服务程序挂接--在kernel/traps.c
	blk_dev_init();			// 初始化块设备请求项结构--在kernel/blk_drv/ll_rw_blk.c
	chr_dev_init();			// 字符设备初始化--[空函数]--在kernel/chr_drv/tty_io.c
	tty_init();				// 与建立人机交互界面相关的外设的中断服务程序挂接--在kernel/chr_drv/tty_io.c
	time_init();			// 设置开机启动时间（startup_time）--main.c
	// 调度程序初始化。--在kernel/sched.c
	// 此处初始化分为3点：
	// 1、将已经设计好的进程0管理结构task_struct与全局描述符表相挂接，并对全局描述符表、进程槽以及与进程调度
	//    相关的寄存器进行初始化设置
	// 2、对时钟中断进行设置，以便进行基于时间片的进程轮流调度
	// 3、通过set_system_gate将system_call（系统调用总入口）与IDT相挂接，使进程0具备处理系统调用的能力
	sched_init();
	// 初始化缓冲区管理结构。--在fs/buffer.c
	// 系统分别通过 空闲表 和 哈希表 对缓冲区进行管理，所以要对这两个数据结构进行初始化。先对空闲表设置，
	// 使其成为“双环链表”，其中的成员均被设置为0，空闲表内存空间在内核之后，与内核内存空间大小差不多
	// hash_table，共307项，均被设置为NULL
	buffer_init(buffer_memory_end);
	// 对硬盘操作方式进行初始化设置。--在kernel/blk_drv/hd.c
	// 1、将硬盘请求项服务程序 do_hd_request 与 blk_dev 控制结构相挂接；
	// 2、将硬盘中断服务程序 hd_interrupt 与 IDT 相挂接；
	// 3、复位主8259A屏蔽位(int2)，允许从片发出中断请求；
	// 4、复位硬盘中断中断请求屏蔽位（在从片上），允许硬盘控制器发出中断请求
	hd_init();
	// 对软盘操作方式进行初始化设置。--在kernel/blk_drv/floppy.c
	// 1、将软盘请求项服务程序 do_fd_request 与 blk_dev 控制结构相挂接；
	// 2、将软盘中断服务程序 floppy_interrupt 与 IDT 相挂接；
	// 3、复位软盘的中断请求屏蔽位，允许软盘控制器发送中断请求
	// 【注意】此处将软盘中断服务程序与IDT挂接，是构建32位保护模式中断服务体系的最后一步，这一步
	//         的完成标志着Linux-0.11保护模式下的中断服务体系彻底构建完毕!!!!!!
	floppy_init();
	sti();								// 开中断。进程0(还未激活)可以在上述所准备的环境下正式开始工作了!!!!!!
	// 移动到用户模式运行。该宏利用iret指令实现从内核模式移动到初始进程0去执行!!!!!!--在include/asm/system.h
	move_to_user_mode();
	if (!fork()) {		/* we count on this going ok */	// 创建进程1!!!!!! fork()为宏定义函数，定义在本文件上部
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
 * 注意!! 对于任何其它的任务，'pause()'将意味着我们必须等待收到一个信号才会返
 * 回就绪运行态，但任务0（task0）是唯一的意外情况（参见'schedule()'），因为任
 * 务0 在任何空闲时间里都会被激活（当没有其它任务在运行时），
 * 因此对于任务0'pause()'仅意味着我们返回来查看是否有其它任务可以运行，如果没
 * 有的话我们就回到这里，一直循环执行'pause()'。
 */
	for(;;) pause();					// for(;;) 相当于 while(1),pause()为宏定义函数，参见本文件上部
}

// 产生格式化信息并输出到标准输出设备stdout(1)，这里是指屏幕上显示。参数'*fmt'
// 指定输出将采用的格式，参见各种标准C 语言书籍。该子程序正好是vsprintf 如何使
// 用的一个例子。
// 该程序使用vsprintf()将格式化的字符串放入printbuf 缓冲区，然后用write()
// 将缓冲区的内容输出到标准设备（1--stdout）。
static int printf(const char *fmt, ...)	// 不定参数，至少1个
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };		// 调用执行程序时参数的字符串数组。
static char * envp_rc[] = { "HOME=/", NULL };		// 调用执行程序时的环境字符串数组。

static char * argv[] = { "-/bin/sh",NULL };			// 同上
static char * envp[] = { "HOME=/usr/root", NULL };

// init()函数运行在任务0创建的子进程（任务1）中。它首先对第一个要执行的程序（shell）的环境进行初始化
// ，然后加载改程序并执行之。
// 注意：从main()开始，由于进程特权级都为3，其不可能执行内核函数。故其中的内核调用要么是系统调用(int 0x80)
// ，要么是库函数，其实库函数还是通过系统调用实现的,只不过是系统调用的外包而已.
void init(void)
{
	int pid,i;

	// 读取硬盘参数包括分区表信息并建立虚拟盘和安装根文件系统设备。
	// 该函数是在本文件上部宏定义的，对应函数是sys_setup()，在kernel/blk_drv/hd.c。
	setup((void *) &drive_info);
	// open()定义在 lib/open.c。用读写访问方式打开设备“/dev/tty0”，这里对应终端控制台。
	// 返回的句柄号0 -- stdin 标准输入设备。
	(void) open("/dev/tty0",O_RDWR,0);
	// dup()宏定义函数，在 lib/dup.c。
	(void) dup(0);					// 复制句柄，产生句柄1 号-- stdout 标准输出设备。
	(void) dup(0);					// 复制句柄，产生句柄2 号-- stderr 标准出错输出设备。
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);		// 打印缓冲区块数和总字节数，每块1024 字节。
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);	//空闲内存字节数。
	// 下面fork()用于创建一个子进程(子任务)。对于被创建的子进程，fork()将返回0 值，
	// 对于原进程(父进程)将返回子进程的进程号。所以if (!(pid=fork())) {...} 内是子进程执行的内容。
	// 该子进程关闭了句柄0(stdin)，以只读方式打开/etc/rc 文件，并执行/bin/sh 程序，所带参数和
	// 环境变量分别由argv_rc 和envp_rc 数组给出。
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);				// 如果打开文件失败，则退出(/lib/_exit.c)。
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);					// 若execve()执行失败则退出(出错码2,“文件或目录不存在”)。
	}
	// 下面是父进程执行的语句。wait()是等待子进程停止或终止，其返回值应是子进程的
	// 进程号(pid)。这三句的作用是父进程等待子进程的结束。&i 是存放返回状态信息的
	// 位置。如果wait()返回值不等于子进程号，则继续等待。
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	// --
	// 如果执行到这里，说明刚创建的子进程的执行已停止或终止了。下面循环中首先再创建
	// 一个子进程，如果出错，则显示“初始化程序创建子进程失败”的信息并继续执行。对
	// 于所创建的子进程关闭所有以前还遗留的句柄(stdin, stdout, stderr)，新创建一个
	// 会话并设置进程组号，然后重新打开/dev/tty0 作为stdin，并复制成stdout 和stderr。
	// 再次执行系统解释程序/bin/sh。但这次执行所选用的参数和环境数组另选了一套（见上面）。
	// 然后父进程再次运行wait()等待。如果子进程又停止了执行，则在标准输出上显示出错信息
	//		“子进程pid 停止了运行，返回码是i”，
	// 然后继续重试下去…，形成“大”死循环。
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
