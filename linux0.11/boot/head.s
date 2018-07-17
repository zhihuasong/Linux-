/*[passed]
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */
/*
 *   head.s 含有32 位启动代码。
 *
 * 注意!!! 32 位启动代码是从绝对地址0x00000000 开始的，这里也同样
 * 是页目录将存在的地方，因此这里的启动代码将被页目录覆盖掉。
 * 
 */

.text
.globl _idt,_gdt,_pg_dir,_tmp_floppy_area
_pg_dir:	# 页目录将会存放在这里
startup_32:
	# 刚切换至32位保护模式，设置其他段寄存器
	# 再次注意!!! 这里已经处于32 位运行模式，因此这里的$0x10 并不是把地址0x10 装入各
	# 个段寄存器，它现在其实是全局段描述符表中的偏移值，或者更正确地说是一个描述符表
	# 项的选择符。有关选择符的说明请参见setup.s 中的说明。这里$0x10 的含义是请求特权
	# 级0(位0-1=0)、选择全局描述符表(位2=0)、选择表中第2 项(位3-15=2)。它正好指向表中
	# 的数据段描述符项。（描述符的具体数值参见前面setup.s ）。下面代码的含义是：
	# 置ds,es,fs,gs 中的选择符为setup.s 中构造的数据段（全局段描述符表的第2 项）=0x10，
	# 并将堆栈放置在数据段中的_stack_start 数组内，然后使用新的中断描述符表和全局段
	# 描述表.新的全局段描述表中初始内容与setup.s 中的完全一样。
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
	lss _stack_start,%esp	# 此指令将低位-->esp，高位-->ss
							# 在汇编中引用C文件中的变量要在变量前加下划线“_”，在kernel/sched.c中，结构体stack_start
							# = { & user_stack [PAGE_SIZE>>2] , 0x10 }，这行代码将栈顶指针指向user_stack数据结构
							# 的末尾位置，同样在kernel/sched.c中有，long user_stack [PAGE_SIZE>>2]，其中PAGE_SIZE在
							# include/a.out.h中被定义为4096
	call setup_idt			# 调用设置中断描述符表子程序。
	call setup_gdt			# 调用设置全局描述符表子程序。
	# 因为修改了GDT,段限长从8MB变为16MB,故重新设置段寄存器,CS代码段寄存器已在setup_gdt中重新加载过了(返回时由ret加载)
	movl $0x10,%eax			# reload all the segment registers
	mov %ax,%ds				# after changing gdt. CS was already
	mov %ax,%es				# reloaded in 'setup_gdt'
	mov %ax,%fs
	mov %ax,%gs
	lss _stack_start,%esp	# 参见上面lss _stack_start,%esp

# 检验A20是否确实已经打开，没打开的话，死循环--参见boot/setup.s文件中A20打开处的注释
	xorl %eax,%eax
1:	incl %eax				# check that A20 really IS enabled
	movl %eax,0x000000		# loop forever if it isn't
	cmpl %eax,0x100000
	je 1b					# ‘1b’表示向后（backward）跳转到标号1去（即，倒退3行至标号1: incl %eax）
/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
/*
* 注意! 在下面这段程序中，486 应该将位16 置位，以检查在超级用户模式下的写保护,
* 此后"verify_area()"调用中就不需要了。486 的用户通常也会想将NE(5)置位，以便
* 对数学协处理器的出错使用int 16。
*/
# 对于486以前的处理器，可能不存在数学协处理器，故需检查。下面这段程序用于检查数学协处理器芯片是否存在。
# 方法是修改控制寄存器CR0，在假设存在协处理器的情况下执行一个协处理器指令，如果出错的话则说明协处理器
# 芯片不存在，需要设置CR0 中的协处理器仿真位EM（位2），并复位协处理器存在标志MP（位1）。

	movl %cr0,%eax		# check math chip
	andl $0x80000011,%eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2,%eax		# set MP
	movl %eax,%cr0
	call check_x87
	# 在为调用main函数做最后准备，这是head执行的最后阶段，也是main函数执行前的最后阶段
	jmp after_page_tables

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
/*
 * 我们依赖于ET 标志的正确性来检测287/387 存在与否。
 */
check_x87:
	fninit		# fninit指令向协处理器发出初始化命令
	fstsw %ax	# fstsw指令取协处理器的状态字.若系统中存在协处理器的话,那么在执行了fninit后其状态字低字节肯定为0。
	cmpb $0,%al
	je 1f			/* no coprocessor: have to set bits */	# 1f表示向前跳转至1标号去，即跳转至1: .byte 0xDB,0xE4
	movl %cr0,%eax
	xorl $6,%eax		/* reset MP, set EM */
	movl %eax,%cr0
	ret
.align 2	# 这里.align 2的含义是指存储边界对齐调整。2则表示调整到地址最后2位为0，即按4字节方式对齐内存地址。
			# 对齐的主要作用是提高CPU运行效率。
1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */ # 287 协处理器码。
	ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
/*
 * 下面这段是设置中断描述符表子程序setup_idt
 *
 * 将中断描述符表idt 设置成具有256 个项，并都指向ignore_int 中断门。然后加载
 * 中断描述符表寄存器(用lidt 指令)。真正实用的中断门以后再安装。当我们在其它
 * 地方认为一切都正常时再开启中断。该子程序将会被页表覆盖掉。
 */
setup_idt:
	lea ignore_int,%edx		# lea是有效地址传送指令，将源操作数给出的有效地址传送到指定的寄存器中，而mov则是传送值
	# 以下设置IDT的表项，参见IDT表项结构
	movl $0x00080000,%eax	# 将选择符0x0008 置入eax 的高16 位中。
	movw %dx,%ax		/* selector = 0x0008 = cs */			# 偏移值的低16 位置入eax 的低16 位中。此时eax 含
																# 有门描述符低4 字节的值。
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present */	# 此时edx 含有门描述符高4 字节的值

	lea _idt,%edi
	mov $256,%ecx			# 设置256个IDT表项
rp_sidt:
	movl %eax,(%edi)		# 将哑中断门描述符存入表中。
	movl %edx,4(%edi)
	addl $8,%edi			# edi 指向表中下一项。
	dec %ecx
	jne rp_sidt
	lidt idt_descr			# 加载中断描述符表寄存器值。
	ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
# 加载全局描述符表寄存器(内容已设置好，见尾部)。
setup_gdt:
	lgdt gdt_descr
	ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
.org 0x1000		# 定义第一个页表从偏移0x1000处开始
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000		# 定义以下的内存数据块从偏移0x5000处开始
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
/*
 * 当DMA（直接存储器访问）不能访问缓冲块时，下面的tmp_floppy_area 内存块
 * 就可供软盘驱动程序使用。其地址需要对齐调整，这样就不会跨越64kB 边界。
 */
_tmp_floppy_area:
	.fill 1024,1,0		# 共保留1024 项，每项1 字节，填充数值0 。

after_page_tables:
	pushl $0			# These are the parameters to main :-)	# 调用main函数的参数，但实际上main函数并不使用
	pushl $0
	pushl $0
	pushl $L6			# return address for main, if it decides to.	# 模拟main函数的返回地址，如果main
																		# 函数真的退出时，则会进入死循环																	
	pushl $_main		# 将main函数的地址入栈，最后用ret指令弹出到eip寄存器中，进而执行main函数
	jmp setup_paging	# 转去设置分页处理（setup_paging）
L6:
	jmp L6				# main should never return here, but
						# just in case, we know what happens.

/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax		# 置段选择符（使段寄存器指向gdt表示的数据段）
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	pushl $int_msg		# 把调用printk函数的参数指针（地址）入栈
	call _printk		# _printk是printk编译后模块中的内部表示法
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */
/*
 * Setup_paging
 *
 * 这个子程序通过设置控制寄存器cr0 的标志（PG 位31）来启动对内存的分页处理
 * 功能，并设置各个页表项的内容，以恒等映射前16 MB 的物理内存。分页器假定
 * 不会产生非法的地址映射（也即在只有4Mb 的机器上设置出大于4Mb 的内存地址）。
 *
 * 注意！尽管所有的物理地址都应该由这个子程序进行恒等映射，但只有内核页面管
 * 理函数能直接使用>1Mb 的地址。所有“一般”函数仅使用低于1Mb 的地址空间，或
 * 者是使用局部数据空间，地址空间将被映射到其它一些地方去-- mm(内存管理程序)
 * 会管理这些事的。
 *
 * 对于那些有多于16Mb 内存的家伙- 太幸运了，我还没有，为什么你会有:-)。代码就
 * 在这里，对它进行修改吧。（实际上，这并不太困难的。通常只需修改一些常数等。
 * 我把它设置为16Mb，因为我的机器再怎么扩充甚至不能超过这个界限（当然，我的机 
 * 器很便宜的:-)）。我已经通过设置某类标志来给出需要改动的地方（搜索“16Mb”），
 * 但我不能保证作这些改动就行了 :-( )
 */
#
# 设置页目录（1个）、页表（4个）；然后使cr3中高20位设置为页目录基址，设置cr0的PG位（31位），启动分页机制
#
.align 2				# 按4字节对齐方式对齐内存地址边界
setup_paging:			# 首先对5页内存（1页目录+4页页表）清零
	movl $1024*5,%ecx		/* 5 pages - pg_dir+4 page tables */
	xorl %eax,%eax
	xorl %edi,%edi			/* pg_dir is at 0x000 */
	cld;rep;stosl		# 循环清零
	# 设置页目录表的前4项，使之分别指向4个页表，页目录项的结构与页表中项的结构一样，4个字节为一项
	# "$pg0+7"表示：0x00001007，是页目录表中的第1 项。则第1 个页表所在的地址 = 0x00001007 & 0xfffff000 = 0x1000； 
	# 第1 个页表的属性标志 = 0x00001007 & 0x00000fff = 0x07，表示该页存在、用户可读写。 
	movl $pg0+7,_pg_dir			/* set present bit/user r/w */
	movl $pg1+7,_pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7,_pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7,_pg_dir+12		/*  --------- " " --------- */
	# 从最后一个页表的最后一项倒序填写4个页表的页表项。每项的内容是：当前项所映射的物理
	# 内存地址 + 该页的标志（这里均为7）。 
	movl $pg3+4092,%edi
	movl $0xfff007,%eax		/*  16Mb - 4096 + 7 (r/w user,p) */
	std					# 设置DF标志位为1，edi值递减（4字节）
1:	stosl			/* fill pages backwards - more efficient :-) */
	subl $0x1000,%eax	# 每填写好一项，物理地址值减0x1000（4KB）
	jge 1b				# 如果小于0 则说明全添写好了
	xorl %eax,%eax		/* pg_dir is at 0x0000 */
	movl %eax,%cr3		/* cr3 - page directory start */	# 将CR3(3号32位控制寄存器,高20位存页目录基址)指向页目录表
	movl %cr0,%eax
	orl $0x80000000,%eax									# 设置启动使用分页处理（cr0 的PG 标志，位31）
	movl %eax,%cr0		/* set paging (PG) bit */			# 设置CR0中分页标志位PG为1
	ret		/* this also flushes prefetch-queue */	# 在改变分页处理标志后要求使用转移指令刷新预取指令队列，这里用的
													# 是返回指令ret。该返回指令的另一个作用是将堆栈中的main 程序的地 
													# 址弹出，并开始运行/init/main.c程序。本程序到此真正结束了。

# ----------------------------------本程序到此真正结束了，开始运行/init/main.c程序----------------------------------

.align 2
.word 0
idt_descr:
	.word 256*8-1		# idt contains 256 entries	# 限长0x7FF
	.long _idt			# 废弃在setup中设定的的IDT，改为新地址
.align 2
.word 0
gdt_descr:
	.word 256*8-1		# so does gdt (not that that's any	# 限长0x7FF
	.long _gdt		# magic number, but it works for me :^)

	.align 3				# 按8 字节方式对齐内存地址边界。
_idt:	.fill 256,8,0		# idt is uninitialized

# 全局描述符表。前4项分别为空项（不用）、代码段描述符、数据段描述符、系统段描述符，其中系统段描述符在Linux中没有派用处。
# 后面还预留了252项的空间，用于放置所创建的任务的局部描述符（LDT）和对应的任务状态段（TSS）的描述符。（0-nul,1-cs,2-ds,
# 3-sys,4-TSS0,5-LDT0,6-TSS1,7-LDT1......）
_gdt:	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00c09a0000000fff	/* 16Mb */	# 代码段最大长度16M
	.quad 0x00c0920000000fff	/* 16Mb */	# 数据段最大长度16M
	.quad 0x0000000000000000	/* TEMPORARY - don't use */
	.fill 252,8,0			/* space for LDT's and TSS's etc */
