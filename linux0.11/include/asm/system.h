// 移动到用户模式运行。该宏利用iret指令实现从内核模式移动到初始进程0去执行!!!!!!--被main()调用
#define move_to_user_mode() \
__asm__ ("movl %%esp,%%eax\n\t"	/*保存堆栈指针esp到eax寄存器*/ \
	"pushl $0x17\n\t"			/*首先将Task0堆栈段选择符（SS）入栈，0x17即00010111B，特权级3，选择LDT第3项*/ \
	"pushl %%eax\n\t"			/*然后将保存的堆栈指针入栈*/ \
	"pushfl\n\t"				/*将标志寄存器（eflags）内容入栈*/ \
	"pushl $0x0f\n\t"			/*将Task0代码段选择符（CS）入栈，0x0f即1111，特权级3，选择LDT第2项*/ \
	"pushl $1f\n\t"				/*将下面标号1的偏移地址（eip）入栈*/ \
	"iret\n"					/*执行中断返回指令，则会跳转到下面标号1处执行*/ \
	"1:\tmovl $0x17,%%eax\n\t"	/*此时开始执行进程0!!!!!!*/ \
	"movw %%ax,%%ds\n\t"		/*初始化段寄存器指向本局部表的数据段*/ \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

/*
 *	gcc内嵌汇编语法：
 *	__asm__( 汇编语句
 *			: 输出寄存器
 *			: 输入寄存器
 *			: 会被修改的寄存器
 *			)
 *	_set_gate 宏定义注释：
 *	总的功能为：根据给定参数，设置IDT的一个表项
 *	输入寄存器定义： 
 *		%0 - "i" ((short) (0x8000+(dpl<<13)+(type<<8))),该输入项说明是立即数， 
 *	0x8000对应的上面Trap Gate 的47bit （P），dpl<<13将dpl （在此是ring0）填入到 
 *	上图的45-46bit，type<<8将type填入到40-44bit，在此是陷阱门应为0xF； 
 *		%1 - "o" (*((char *) (gate_addr))), 该输入项说明是使用内存地址并且可以加 
 *	偏移值，这个对应的是idt[n]的低4个字节； 
 *		%2 - "o" (*(4+(char *) (gate_addr))), 该输入项说明是使用内存地址并且可以 
 *	加偏移值，这个对应的是idt[n]的高4个字节； 
 *		"d" ((char *) (addr)),"a" (0x00080000))，说明edx存放回调函数地址，eax 
 *	存放陷阱门描述符中的16-31bit （Segment Selector）。
 *
 *		这样看的话汇编语句应该就很清晰了： 
 *		"movw %%dx,%%ax\n\t" – 将回调函数的低16位放入描述符的0-15bit，放入ax，这 
 *	样与原本的eax的高16位 （0x0008）组合，组成了描述符的低4个字节内容； 
 *		"movw %0,%%dx\n\t"    -  填写描述符的32-47bit，放入dx，这样与原本的edx的高 
 *	16位（回调函数的高16位地址）组合，组成了描述符的高4个字节内容； 
 *		"movl %%eax,%1\n\t"  -  将描述符的低4字节放入idt[n]的低4字节； 
 *		"movl %%edx,%2"        -  将描述符的高4节放入idt[n]的高4字节。
 */
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

// 中断门，DPL=0
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

// 陷阱门，DPL=0
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

// 陷阱门，DPL=3
#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),addr,"0x89")
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),addr,"0x82")
