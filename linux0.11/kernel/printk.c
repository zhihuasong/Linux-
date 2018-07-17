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
 * �������ں�ģʽʱ�����ǲ���ʹ��printf����Ϊ�Ĵ���fs ָ����������
 * ��Ȥ�ĵط����Լ�����һ��printf ����ʹ��ǰ����fs��һ�оͽ���ˡ�
 */

/*
 * ����ע�ͣ�
 *
 *	1��	����C���Կɱ������ʹ�ã���μ� C���Կɱ���� ������ĵ���
 *		����vsprintf()��ʹ�ã��μ�linux-0.11/kernel/vsprintf.c��
 *
 *	2��	����fs�μĴ��������⡣���ϲ�õ����ϣ����ڽ��̽����ں�̬��
 *		fs�Ĵ���Ĭ��ָ������û�̬�����ݶΡ���ds,es�Ĵ�����ָ���ں����ݶΡ�
 *		�����û�����ʱ����Щ�Ĵ�����ִ���û����ݶΡ�����
 *
 *	3��	����linus��ע����˵���ģ�printk()�������ں�̬��printf()��
 *		printf()��printk()��������ʵ�����������ļ�������ͬ�ģ�
 *		�������ֲ�������Ϊtty_write������Ҫʹ��fsָ��ı���ʾ��
 *		�ַ�������fs��ר�����ڴ���û�̬���ݶ�ѡ����ģ���ˣ���
 *		�ں�̬ʱ��Ϊ�����tty_write������printk���fs�޸�Ϊ�ں�̬
 *		���ݶ�ѡ���ds�е�ֵ������������ȷָ���ں˵����ݻ�������
 *		��Ȼ����������ȶ�fs����ѹջ���棬����tty_write��Ϻ��ٳ�ջ
 *		�ָ����ܽ�˵����printk��printf�Ĳ�������fs��ɵģ����Բ���Ҳ
 *		��Χ�ƶ�fs�Ĵ���
 *		�ɲο���http://blog.csdn.net/wang6077160/article/details/6629252
 */

#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

// �ں�ʹ�õ���ʾ������
int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);	// ʹ�ø�ʽ��fmt �������б�args �����
								// buf �С�����ֵi ��������ַ����ĳ��ȡ�
	va_end(args);
	__asm__("push %%fs\n\t"
		"push %%ds\n\t"
		"pop %%fs\n\t"			// ��fs=ds
		"pushl %0\n\t"
		"pushl $_buf\n\t"
		"pushl $0\n\t"
		"call _tty_write\n\t"	// tty_write()��kernel/chr_drv/tty_io.c
		"addl $8,%%esp\n\t"
		"popl %0\n\t"
		"pop %%fs"
		::"r" (i):"ax","cx","dx");
	return i;
}
