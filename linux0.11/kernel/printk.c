/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
/*
 * 当处于内核模式时，我们不能使用printf，因为寄存器fs 指向其它不感
 * 兴趣的地方。自己编制一个printf 并在使用前保存fs，一切就解决了。
 */

/*
 * 本人注释：
 *
 *	1、	关于C语言可变参数的使用，请参见 C语言可变参数 的相关文档；
 *		关于vsprintf()的使用，参见linux-0.11/kernel/vsprintf.c。
 *
 *	2、	关于fs段寄存器的问题。网上查得的资料：“在进程进入内核态后，
 *		fs寄存器默认指向进程用户态的数据段。而ds,es寄存器则指向内核数据段。
 *		而在用户运行时，这些寄存器都执行用户数据段。”。
 *
 *	3、	正如linus的注释中说明的，printk()是用于内核态的printf()。
 *		printf()与printk()的区别：其实这两个函数的几乎是相同的，
 *		出现这种差异是因为tty_write函数需要使用fs指向的被显示的
 *		字符串，而fs是专门用于存放用户态数据段选择符的，因此，在
 *		内核态时，为了配合tty_write函数，printk会把fs修改为内核态
 *		数据段选择符ds中的值，这样才能正确指向内核的数据缓冲区，
 *		当然这个操作会先对fs进行压栈保存，调用tty_write完毕后再出栈
 *		恢复。总结说来，printk与printf的差异是由fs造成的，所以差异也
 *		是围绕对fs的处理。
 *		可参考：http://blog.csdn.net/wang6077160/article/details/6629252
 */

#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

// 内核使用的显示函数。
int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);	// 使用格式串fmt 将参数列表args 输出到
								// buf 中。返回值i 等于输出字符串的长度。
	va_end(args);
	__asm__("push %%fs\n\t"
		"push %%ds\n\t"
		"pop %%fs\n\t"			// 令fs=ds
		"pushl %0\n\t"
		"pushl $_buf\n\t"
		"pushl $0\n\t"
		"call _tty_write\n\t"	// tty_write()在kernel/chr_drv/tty_io.c
		"addl $8,%%esp\n\t"
		"popl %0\n\t"
		"pop %%fs"
		::"r" (i):"ax","cx","dx");
	return i;
}
