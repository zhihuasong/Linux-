![passed]
!	setup.s		(C) 1991 Linus Torvalds
!
! setup.s is responsible for getting the system data from the BIOS,
! and putting them into the appropriate places in system memory.
! both setup.s and system has been loaded by the bootblock.
!
! This code asks the bios for memory/disk/other parameters, and
! puts them in a "safe" place: 0x90000-0x901FF, ie where the
! boot-block used to be. It is then up to the protected mode
! system to read them from there before the area is overwritten
! for buffer-blocks.
!
! setup.s负责利用BIOS中断获取系统数据，并将这些数据放到系统内存的适当地方。
! 此时setup.s 和system 已经由bootsect 引导块加载到内存中。
! 这段代码询问bios 有关内存/磁盘/其它参数，并将这些参数放到一个
! “安全的”地方：90000-901FF，也即原来bootsect 代码块曾经在
! 的地方，然后在被缓冲块覆盖掉之前由保护模式的system 读取。

! NOTE! These had better be the same as in bootsect.s!

INITSEG  = 0x9000	! we move boot here - out of the way
SYSSEG   = 0x1000	! system loaded at 0x10000 (65536).
SETUPSEG = 0x9020	! this is the current segment

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:

! ok, the read went well so we get current cursor position and save it for
! posterity.
! 将光标位置信息存放在90000 处，控制台初始化时会来取。
!	BIOS 中断10h 的读光标功能号ah = 03
!	输入：bh = 页号
!	返回：ch = 扫描开始线，cl = 扫描结束线，dh = 行号(00 是顶端)，dl = 列号(00 是左边)。	
	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax
	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches
	mov	[0],dx		! it from 0x90000.

! Get memory size (extended mem, kB)
! 这3句取扩展内存的大小值（KB）。
! 调用中断15h，功能号ah = 88
!	返回：ax = 从100000（1M）处开始的扩展内存大小(KB)。若出错则CF 置位，ax = 出错码。

	mov	ah,#0x88
	int	0x15
	mov	[2],ax

! Get video-card data:
! 下面这段用于取显示卡当前显示模式。
! 调用BIOS 中断10，功能号ah = 0f
!	返回：ah = 字符列数，al = 显示模式，bh = 当前显示页。
! 90004(1 字)存放当前页，90006 显示模式，90007 字符列数。

	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width

! check for EGA/VGA and some config parameters
! 检查显示方式（EGA/VGA）并取参数。
! 调用BIOS 中断10，附加功能选择-取方式信息.功能号：ah = 12，bl = 10
!		返回：	bh = 显示状态(00 - 彩色模式，I/O 端口=3dX)(01 - 单色模式，I/O 端口=3bX)
!				bl = 安装的显示内存(00 - 64k, 01 - 128k, 02 - 192k, 03 = 256k)
!				cx = 显示卡特性参数。

	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax		! 90008 = ??
	mov	[10],bx		! 9000A = 安装的显示内存，9000B = 显示状态(彩色/单色)
	mov	[12],cx		! 9000C = 显示卡特性参数。

! Get hd0 data
! 取第一个硬盘的信息（复制硬盘参数表）。
! 第1 个硬盘参数表的首地址竟然是中断向量41h 的向量值！而第2 个硬盘
! 参数表紧接第1 个表的后面，中断向量46h 的向量值也指向这第2 个硬盘
! 的参数表首址。表的长度是16 个字节(10)。
! 下面两段程序分别复制BIOS 有关两个硬盘的参数表，90080 处存放第1 个
! 硬盘的表，90090 处存放第2 个硬盘的表。

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]	! 取中断向量41h 的值，也即hd0 参数表的地址 -> ds:si。lds的功能是：从存储器取出32位地址的指令.
					! 将段地址（高字）送入ds，偏移地址（低字）送入si
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080
	mov	cx,#0x10
	rep
	movsb

! Get hd1 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]	! 取中断向量46 的值，也即hd1 参数表的地址 -> ds:si
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	rep
	movsb

! Check that there IS a hd1 :-)
! 检查系统是否存在第2 个硬盘，如果不存在则第2 个表清零。
! 利用BIOS 中断调用13 的取盘类型功能。
! 功能号ah = 15；
!		输入：dl = 驱动器号（8X 是硬盘：80 指第1 个硬盘，81 指第2 个硬盘）
!		输出：ah = 类型码；00 --没有这个盘，CF 置位； 01 --是软驱，没有change-line 支持；
!		02--是软驱(或其它可移动设备)，有change-line 支持； 03 --是硬盘。

	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3		! 是硬盘吗？(类型= 3 ？)。
	je	is_disk1
no_disk1:
	mov	ax,#INITSEG	! 第2个硬盘不存在，则对第2个硬盘表清零。
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb			! 将累加器AL中的值传递到当前DI地址处

is_disk1:

! now we want to move to protected mode ...
! 从这里开始我们要做保护模式方面的工作了。

	cli			! no interrupts allowed !	! 关中断

! first we move the system to it's rightful place
! 首先我们将system 模块移到正确的位置。
! bootsect 引导程序是将system 模块读入到从10000（64k）开始的位置。由于当时假设
! system 模块最大长度不会超过80000（512k），也即其末端不会超过内存地址90000，
! 所以bootsect 会将自己移动到90000 开始的地方，并把setup 加载到它的后面。
! 下面这段程序的用途是再把整个system 模块移动到00000 位置，即把从10000 到8ffff
! 的内存数据块(512k)，整块地向内存低端移动了10000（64k）的位置。

	mov	ax,#0x0000
	cld			! 'direction'=0, movs moves forward	! 设置标志位DF，控制传送方向
do_move:			! 复制内核代码到0x00000处
	mov	es,ax		! destination segment
	add	ax,#0x1000
	cmp	ax,#0x9000
	jz	end_move	! 已经把从8000 段开始的64k 代码移动完？
	mov	ds,ax		! source segment
	sub	di,di
	sub	si,si
	mov cx,#0x8000	! 移动0x8000 字（64k 字节）。
	rep
	movsw
	jmp	do_move

! then we load the segment descriptors
! 此后，我们加载段描述符。
! 从这里开始会遇到32 位保护模式的操作，因此需要Intel 32 位保护模式编程方面的知识了,
! 有关这方面的信息请查阅列表后的简单介绍或附录中的详细说明。这里仅作概要说明。
!
! lidt 指令用于加载中断描述符表(idt)寄存器，它的操作数是6 个字节，0-1 字节是描述符表的
! 长度值(字节)；2-5 字节是描述符表的32 位线性基地址（首地址），其形式参见下面的说明。中断描述符表中
! 的每一个表项（8 字节）指出发生中断时需要调用的代码的信息，与中断向量有些相似，但要包含更多的信息。
! 
! lgdt 指令用于加载全局描述符表(gdt)寄存器，其操作数格式与lidt 指令的相同。全局描述符
! 表中的每个描述符项(8 字节)描述了保护模式下数据和代码段（块）的信息。其中包括段的
! 最大长度限制(16 位)、段的线性基址（32 位）、段的特权级、段是否在内存、读写许可以及
! 其它一些保护模式运行的标志。

end_move:
	mov	ax,#SETUPSEG	! right, forgot this at first. didn't work :-)
	mov	ds,ax
	lidt	idt_48		! load idt with 0,0	! 初始化专用寄存器IDTR（中断描述符表IDT的基址寄存器）的指向,idt_48是6字节
											! 操作数的位置前2 字节表示idt 表的限长，后4 字节表示idt 表所处的基地址
	lgdt	gdt_48		! load gdt with whatever appropriate ! 初始化专用寄存器GDTR（全局描述符表GDT的基址寄存器）的指向
															! ,gdt_48 是6 字节操作数的位置

! that was painless, now we enable A20
! 打开A20地址线，寻址模式切换至32位
! 【扩展】：在实模式下，当程序寻址超过0xFFFFF时，CPU将“回滚”至内存地址起始处寻址（在只有20位地址线的条件下
!		，0xFFFFF+1=0X00000,最高位溢出）。例如，系统的段寄存器（CS）最大允许地址为0xFFFF，指令指针（IP）
!		最大允许段内偏移地址也为0xFFFF，二者确定的最大绝对地址为0x10FFEF，这将意味着程序中可产生的实模式下
!		的寻址范围比1MB多出将近64KB。这样，此处对A20地址线启用相当于关闭了CPU在实模式下寻址的“回滚”机制。
!		在后续代码中，也将看到利用此特点来验证A20地址线是否确实已经打开。

	call	empty_8042					! 等待输入缓冲器空。只有当输入缓冲器为空时才可以对其进行写命令。
	mov	al,#0xD1		! command write
	out	#0x64,al						! D1命令码表示要写数据到8042的P2端口.P2端口的位1用于A20线的选通.数据要写到60口
	call	empty_8042					! 等待输入缓冲器空，看命令是否被接受。
	mov	al,#0xDF		! A20 on		! 选通A20 地址线的参数。
	out	#0x60,al
	call	empty_8042					! 输入缓冲器为空，则表示A20 线已经选通。

! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.
! 对8259A中断控制器重新编程
! 我们将它们放在正好处于intel 保留的硬件中断后面，在int 20h-2Fh。
! 在那里它们不会引起冲突。不幸的是IBM 在原PC 机中搞糟了，以后也没有纠正过来。
! PC 机的bios 将中断放在了08h-0fh，这些中断也被用于内部硬件中断。
! 所以我们就必须重新对8259 中断控制器进行编程

	mov	al,#0x11		! initialization sequence	! 11 表示初始化命令开始，是ICW1 命令字，表示边
													! 沿触发、多片8259 级连、最后要发送ICW4 命令字。
	out	#0x20,al		! send it to 8259A-1		! 发送到8259A 主芯片。
	.word	0x00eb,0x00eb							! jmp $+2, jmp $+2，$表示当前指令的地址，.word 0x00eb是
													! 直接近跳转指令的操作码，跳到下一条指令，起延时作用
	out	#0xA0,al		! and to 8259A-2			! 再发送到8259A 从芯片
	.word	0x00eb,0x00eb
	mov	al,#0x20		! start of hardware int's (0x20)
	out	#0x21,al									! 送主芯片ICW2 命令字，起始中断号，要送奇地址。
	.word	0x00eb,0x00eb
	mov	al,#0x28		! start of hardware int's 2 (0x28)
	out	#0xA1,al									! 送从芯片ICW2 命令字，从芯片的起始中断号。
	.word	0x00eb,0x00eb
	mov	al,#0x04		! 8259-1 is master
	out	#0x21,al									! 送主芯片ICW3 命令字，主芯片的IR2 连从芯片INT。
	.word	0x00eb,0x00eb
	mov	al,#0x02		! 8259-2 is slave
	out	#0xA1,al									! 送从芯片ICW3 命令字，表示从芯片的INT 连到主芯片的IR2 引脚上。
	.word	0x00eb,0x00eb
	mov	al,#0x01		! 8086 mode for both
	out	#0x21,al									! 送主芯片ICW4 命令字。8086 模式；普通EOI 方式，
													! 需发送指令来复位。初始化结束，芯片就绪。
	.word	0x00eb,0x00eb
	out	#0xA1,al									! 送从芯片ICW4 命令字，内容同上。
	.word	0x00eb,0x00eb
	mov	al,#0xFF		! mask off all interrupts for now
	out	#0x21,al									! 屏蔽主芯片所有中断请求。
	.word	0x00eb,0x00eb
	out	#0xA1,al									! 屏蔽从芯片所有中断请求。

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
!
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.
! 这里设置进入32 位保护模式运行。首先加载机器状态字(lmsw - Load Machine Status Word)，
! 也称控制寄存器CR0，其比特位0 置1 将导致CPU 工作在保护模式。

	mov	ax,#0x0001	! protected mode (PE) bit
	lmsw	ax		! This is it!	! 将ax寄存器内容装载到机器状态字，此处将PE位（CR0寄存器的第0位）
									!	置为1，使处理器切换至32位保护模式
	jmpi	0,8		! jmp offset 0 of segment 8 (cs) ! 此处已经开始32位寻址模式，8是段寄存器中的段描述符，
													! 用于选择描述符表和描述符表项以及所要求的特权级。
													! 此处，应将8作为二进制数1000看待，其中最后2位00表示
													! 内核特权级，与之相对应的用户特权级为11；倒数第3位0表示
													! GDT表，若是1，则表示LDT；倒数第4位1，表示所选表（在此为GDT）
													! 的1项（编号从0开始）来确定代码段的段基址和段限长等信息。
													! 由GDT第一项可知段基址为0，故此处转去执行system模块代码

! ------------------------------执行结束，转去执行system模块代码（0x000000处）-------------------------

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway.
! 下面这个子程序检查键盘命令队列是否为空。这里不使用超时方法- 如果这里死机，
! 则说明PC 机有问题，我们就没有办法再处理下去了。
! 只有当输入缓冲器为空时（状态寄存器位2 = 0）才可以对其进行写命令。
empty_8042:
	.word	0x00eb,0x00eb	! jmp $+2, jmp $+2，$表示当前指令的地址，.word 0x00eb是直接近跳转指令的操作码，
							! 跳到下一条指令，起延时作用
	in	al,#0x64	! 8042 status port			! 读AT 键盘控制器状态寄存器。
	test	al,#2		! is input buffer full?	! 2（10B）用于测试位2，输入缓冲器满？
	jnz	empty_8042	! yes - loop
	ret

! 全局描述符表开始处。描述符表由多个8 字节长的描述符项组成。
! 这里给出了3 个描述符项。第1 项无用，但须存在。第2 项是系统代码段
! 描述符（208-211 行），第3 项是系统数据段描述符(213-216 行)。每个描述符的具体
! 含义参见列表后说明。
gdt:
	.word	0,0,0,0		! dummy

	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9A00		! code read/exec
	.word	0x00C0		! granularity=4096, 386

	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)	! 0x00C0中的最后一位（作为高位） 与 0x07FF（作为低位）
															! 组合成0x007FF，0x00C0中C的二进制1100的第3位（倒数，从0开始）
															! 表示应乘以4K，故：段限长 = 0x007FF*4K = 8MB
	.word	0x0000		! base address=0
	.word	0x9200		! data read/write
	.word	0x00C0		! granularity=4096, 386

idt_48:
	.word	0			! idt limit=0
	.word	0,0			! idt base=0L

gdt_48:
	.word	0x800		! gdt limit=2048, 256 GDT entries	! 全局表长度为2k 字节，因为每8 字节组成一个段描述符项
															! 所以表中共可有256 项。
	.word	512+gdt,0x9	! gdt base = 0X9xxxx				! 4 个字节构成的内存线性地址：0009<<16 + 0200+gdt
															! ,也即90200 + gdt(即在本程序段中的偏移地址)

.text
endtext:
.data
enddata:
.bss
endbss:
