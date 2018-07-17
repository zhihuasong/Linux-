// �ƶ����û�ģʽ���С��ú�����iretָ��ʵ�ִ��ں�ģʽ�ƶ�����ʼ����0ȥִ��!!!!!!--��main()����
#define move_to_user_mode() \
__asm__ ("movl %%esp,%%eax\n\t"	/*�����ջָ��esp��eax�Ĵ���*/ \
	"pushl $0x17\n\t"			/*���Ƚ�Task0��ջ��ѡ�����SS����ջ��0x17��00010111B����Ȩ��3��ѡ��LDT��3��*/ \
	"pushl %%eax\n\t"			/*Ȼ�󽫱���Ķ�ջָ����ջ*/ \
	"pushfl\n\t"				/*����־�Ĵ�����eflags��������ջ*/ \
	"pushl $0x0f\n\t"			/*��Task0�����ѡ�����CS����ջ��0x0f��1111����Ȩ��3��ѡ��LDT��2��*/ \
	"pushl $1f\n\t"				/*��������1��ƫ�Ƶ�ַ��eip����ջ*/ \
	"iret\n"					/*ִ���жϷ���ָ������ת��������1��ִ��*/ \
	"1:\tmovl $0x17,%%eax\n\t"	/*��ʱ��ʼִ�н���0!!!!!!*/ \
	"movw %%ax,%%ds\n\t"		/*��ʼ���μĴ���ָ�򱾾ֲ�������ݶ�*/ \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

/*
 *	gcc��Ƕ����﷨��
 *	__asm__( ������
 *			: ����Ĵ���
 *			: ����Ĵ���
 *			: �ᱻ�޸ĵļĴ���
 *			)
 *	_set_gate �궨��ע�ͣ�
 *	�ܵĹ���Ϊ�����ݸ�������������IDT��һ������
 *	����Ĵ������壺 
 *		%0 - "i" ((short) (0x8000+(dpl<<13)+(type<<8))),��������˵������������ 
 *	0x8000��Ӧ������Trap Gate ��47bit ��P����dpl<<13��dpl ���ڴ���ring0�����뵽 
 *	��ͼ��45-46bit��type<<8��type���뵽40-44bit���ڴ���������ӦΪ0xF�� 
 *		%1 - "o" (*((char *) (gate_addr))), ��������˵����ʹ���ڴ��ַ���ҿ��Լ� 
 *	ƫ��ֵ�������Ӧ����idt[n]�ĵ�4���ֽڣ� 
 *		%2 - "o" (*(4+(char *) (gate_addr))), ��������˵����ʹ���ڴ��ַ���ҿ��� 
 *	��ƫ��ֵ�������Ӧ����idt[n]�ĸ�4���ֽڣ� 
 *		"d" ((char *) (addr)),"a" (0x00080000))��˵��edx��Żص�������ַ��eax 
 *	����������������е�16-31bit ��Segment Selector����
 *
 *		�������Ļ�������Ӧ�þͺ������ˣ� 
 *		"movw %%dx,%%ax\n\t" �C ���ص������ĵ�16λ������������0-15bit������ax���� 
 *	����ԭ����eax�ĸ�16λ ��0x0008����ϣ�������������ĵ�4���ֽ����ݣ� 
 *		"movw %0,%%dx\n\t"    -  ��д��������32-47bit������dx��������ԭ����edx�ĸ� 
 *	16λ���ص������ĸ�16λ��ַ����ϣ�������������ĸ�4���ֽ����ݣ� 
 *		"movl %%eax,%1\n\t"  -  ���������ĵ�4�ֽڷ���idt[n]�ĵ�4�ֽڣ� 
 *		"movl %%edx,%2"        -  ���������ĸ�4�ڷ���idt[n]�ĸ�4�ֽڡ�
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

// �ж��ţ�DPL=0
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

// �����ţ�DPL=0
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

// �����ţ�DPL=3
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
