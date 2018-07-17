/*[passed]
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>			// �����ͷ�ļ�������ϵͳ�и��ֳ���š�(Linus ��minix ��������)��

#include <linux/sched.h>	// ���ȳ���ͷ�ļ�������������ṹtask_struct����ʼ����0 �����ݣ�
							// ����һЩ�й��������������úͻ�ȡ��Ƕ��ʽ��ຯ������䡣
#include <linux/kernel.h>	// �ں�ͷ�ļ�������һЩ�ں˳��ú�����ԭ�ζ��塣
#include <asm/segment.h>	// �β���ͷ�ļ����������йضμĴ���������Ƕ��ʽ��ຯ����
#include <asm/system.h>		// ϵͳͷ�ļ������������û��޸�������/�ж��ŵȵ�Ƕ��ʽ���ꡣ

extern void write_verify(unsigned long address);

long last_pid=0;

// ���̿ռ�����дǰ��֤������
// �Ե�ǰ���̵ĵ�ַaddr ��addr+size ��һ�ν��̿ռ���ҳΪ��λִ��д����ǰ�ļ�������
// ��ҳ����ֻ���ģ���ִ�й������͸���ҳ�������дʱ���ƣ���
void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	// ����ʼ��ַstart ����Ϊ������ҳ����߽翪ʼλ�ã�ͬʱ��Ӧ�ص�����֤�����С��
	// ��ʱstart �ǵ�ǰ���̿ռ��е����Ե�ַ��
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);	// ��ʱstart ���ϵͳ�������Կռ��еĵ�ַλ�á�
	while (size>0) {
		size -= 4096;
		// дҳ����֤����ҳ�治��д������ҳ�档��mm/memory.c��
		write_verify(start);
		start += 4096;
	}
}

// ����������Ĵ�������ݶλ�ַ(���Ե�ַ)���޳�������ҳ���½�����ʱʹ�ø����̵��ڴ�ҳ�棩��
// nr Ϊ������ţ�p �����������ݽṹ��ָ�롣
int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

	code_limit=get_limit(0x0f);						// ȡ�ֲ����������д�������������ж��޳���
	data_limit=get_limit(0x17);						// ȡ�ֲ��������������ݶ����������ж��޳���
	old_code_base = get_base(current->ldt[1]);		// ȡԭ����λ�ַ��
	old_data_base = get_base(current->ldt[2]);		// ȡԭ���ݶλ�ַ��
	if (old_data_base != old_code_base)				// 0.11 �治֧�ִ�������ݶη����������
		panic("We don't support separate I&D");
	if (data_limit < code_limit)					// ������ݶγ��� < ����γ���Ҳ���ԡ�
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;	// �»�ַ(���Ե�ַ)=�����*64Mb(�����С)��
	p->start_code = new_code_base;
	set_base(p->ldt[1],new_code_base);				// ���ô�����������л�ַ��
	set_base(p->ldt[2],new_data_base);				// �������ݶ��������л�ַ��
	// ����2����������mm/memory.c�С�
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {	// ���ƴ�������ݶΡ�
		free_page_tables(new_data_base,data_limit);					// ����������ͷ�������ڴ�
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
// ��������Ҫ��fork �ӳ���������ϵͳ������Ϣ(task[n])�������ñ�Ҫ�ļĴ��������������ظ������ݶΡ�
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;

	p = (struct task_struct *) get_free_page();	// �����ڴ�ĩ��Ϊ���������ݽṹ�����ڴ档
	if (!p)
		return -EAGAIN;
	task[nr] = p;						// ��������ṹָ��������������С�nr������������Ϊ֮ǰ��ջ��eax
										// ����find_empty_process()�ķ���ֵ
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */ // ���Ḵ�Ƴ����û��Ķ�ջ(ֻ���Ƶ�ǰ��������)
	// ��������1����ṹtask_struct
	p->state = TASK_UNINTERRUPTIBLE;	// ���½��̵�״̬����Ϊ�����жϵȴ�״̬����ζ�ţ�ֻ���ں˴�������ȷ��ʾ��
										// �ý�������Ϊ����״̬�������ܱ����ѣ�����֮�⣬û���κΰ취�������ѡ�
										// ֮��������������Ϊ���ӽ�����δ������ϣ�����ṹ�����������������Ϊ��
										// "�����жϵȴ�״̬"�����Եı����ӽ����ܵ��κ��ж����źŵĸ��ţ�������ֻ���
	p->pid = last_pid;					// �½��̺š���ǰ�����find_empty_process()�õ�
	p->father = current->pid;			// ���ø����̺š�
	p->counter = p->priority;			// �ý��̵����ȼ���ֵ����ʼ�����̵�ʱ��Ƭ
	p->signal = 0;						// ���̵��źţ�������signal�У�signal��4�ֽڣ�32λ�������൱��ȫ����0
	p->alarm = 0;						// �û�������Ϊ���趨ִ��ʱ��,alarm�����жϵ�ǰ�����Ƿ��Ѿ��������趨ʱ��
	p->leader = 0;		/* process leadership doesn't inherit */	// ���̵��쵼Ȩ�ǲ��ܼ̳е� 
	p->utime = p->stime = 0;			// ��ʼ���û�̬ʱ��ͺ���̬ʱ�䡣
	p->cutime = p->cstime = 0;			// ��ʼ���ӽ����û�̬�ͺ���̬ʱ�䡣
	p->start_time = jiffies;			// ��ǰ�δ���ʱ�䡣
	// ������������״̬��TSS��������ݡ�ǰ��ϵͳ����ѹջ�ļĴ���ֵ����������Ϊ�������Ĳ������������������ˡ�
	// �����л�������½����̣�ϵͳ������Щֵ����ʼ�������Ĵ������Դ�Ϊ�½����̵�ִ���ṩ�������ݡ�
	p->tss.back_link = 0;				// ��ʼû��Ƕ������
	p->tss.esp0 = PAGE_SIZE + (long) p;	// ��ջָ��(�������ṹp������1ҳ�ڴ�,�ʴ�ʱesp0����ָ���ҳ��ߵ�ַ��)
	p->tss.ss0 = 0x10;					// ��ջ��ѡ������ں����ݶΣ�[[??]��ʼ��ʹ���ں����ݶ�]��
	p->tss.eip = eip;					// ָ�����ָ�롣
	p->tss.eflags = eflags;				// ��־�Ĵ�����
	p->tss.eax = 0;						// ��eax��ʼ��Ϊ0�������ش�!!![??]
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;			// �μĴ�����16λ��Ч����16λ�Ƕ�ѡ�����
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);				// ��������nr(nrΪ�������)�ľֲ���������ѡ�����LDT ����������GDT �У���
	p->tss.trace_bitmap = 0x80000000;
	// �����ǰ����ʹ����Э���������ͱ����������ġ�
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	// ����������Ĵ�������ݶλ�ַ���޳�������ҳ�������������ֵ����0������λ����������
	// ��Ӧ��ͷ�Ϊ�������������ڴ�ҳ��
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
	// ��������������ļ��Ǵ򿪵ģ��򽫶�Ӧ�ļ��Ĵ򿪴�����1��
	for (i=0; i<NR_OPEN;i++)
		if (f=p->filp[i])
			f->f_count++;
	// ����ǰ���̣������̣���pwd, root ��executable ���ô�������1��
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	// ��GDT �������������TSS ��LDT ����������ݴ�task �ṹ��ȡ��
	// �������л�ʱ������Ĵ���tr ��CPU �Զ����ء�
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case */	/* ����ٽ����������óɿ�����״̬���Է���һ */
	return last_pid;			// �����½��̺ţ���������ǲ�ͬ�ģ���
}

// Ϊ�½���ȡ�ò��ظ��Ľ��̺�last_pid,�����������������е������(����index),�����ظ�����˵��Ŀǰ��������������
int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;	// ��last_pid = INT_MAXʱ���½��̵�pid�ͱȸ�����С������ȸ����̴�
										// last_pid = ++last_pid % INT_MAX����������ֹ���̵�pid��ʹpid������0-63��
										// �Ӷ�ʹpid���ڲ���ֵ
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)			// ����0 �ų�����
		if (!task[i])
			return i;
	return -EAGAIN;
}
