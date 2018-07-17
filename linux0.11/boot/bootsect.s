![passed]
!
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
!
! SYSSIZE是要加载的节数（16字节为1节）。3000h共为30000h字节＝192kB
! 对当前的版本空间已足够了。
SYSSIZE = 0x3000	! kernel size	! 这里给出了一个最大默认值。
!
!	bootsect.s		(C) 1991 Linus Torvalds
!
! bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
! iself out of the way to address 0x90000, and jumps there.
!
! It then loads 'setup' directly after itself (0x90200), and the system
! at 0x10000, using BIOS interrupts. 
!
! NOTE! currently system is at most 8*65536 bytes long. This should be no
! problem, even in the future. I want to keep it simple. This 512 kB
! kernel size should be enough, especially as this doesn't contain the
! buffer cache as in minix
!
! The loader has been made as simple as possible, and continuos
! read errors will result in a unbreakable loop. Reboot by hand. It
! loads pretty fast by getting whole sectors at a time whenever possible.
!************************************************************************
!	boot被bios－启动子程序加载至7c00h（31k）处，并将自己移动到了
!	地址90000h（576k）处，并跳转至那里。
!	它然后使用BIOS中断将'setup'直接加载到自己的后面（90200h）（576.5k），
!	并将system加载到地址10000h处。
!
!	注意：目前的内核系统最大长度限制为（8*65536）（512kB）字节，即使是在
!	将来这也应该没有问题的。我想让它保持简单明了。这样512k的最大内核长度应该
!	足够了，尤其是这里没有象minix中一样包含缓冲区高速缓冲。
!
!	加载程序已经做的够简单了，所以持续的读出错将导致死循环。只能手工重启。
!	只要可能，通过一次取取所有的扇区，加载过程可以做的很快的。
!************************************************************************

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4				! nr of setup-sectors					! setup程序的扇区数（setup－sectors）值
BOOTSEG  = 0x07c0			! original address of boot-sector		! bootsect的原始地址（是段地址，以下同）
INITSEG  = 0x9000			! we move boot here - out of the way	! 将bootsect移到这里
SETUPSEG = 0x9020			! setup starts here						! setup程序从这里开始
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).		! system模块加载到10000(64kB)处.
ENDSEG   = SYSSEG + SYSSIZE	! where to stop loading					! 停止加载的段地址

! ROOT_DEV:	0x000 - same type of floppy as boot.	! 根文件系统设备使用与引导时同样的软驱设备.
!		0x301 - first partition on first drive etc	! 根文件系统设备在第一个硬盘的第一个分区上
ROOT_DEV = 0x306									! 指定根文件系统设备是第2个硬盘的第1个分区。
						! 这是Linux老式的设备命名方式，具体值的含义如下：
						! 设备号 ＝ 主设备号*256 ＋ 次设备号 
						!           (也即 dev_no = (major<<8 + minor)
						! (主设备号：1－内存，2－磁盘，3－硬盘，4－ttyx，5－tty，6－并行口，7－非命名管道)
						! 300 - /dev/hd0 － 代表整个第1个硬盘
						! 301 - /dev/hd1 － 第1个盘的第1个分区
						! ... ...
						! 304 - /dev/hd4 － 第1个盘的第4个分区
						! 305 - /dev/hd5 － 代表整个第2个硬盘
						! 306 - /dev/hd6 － 第2个盘的第1个分区
						! ... ...
						! 309 - /dev/hd9 － 第2个盘的第4个分区 


! 操作系统开始规划内存，将第一批代码bootsect复制到自己指定的位置，并设置相应的栈!!!(这是系统初始化时临时使用的堆栈)
entry start				! 告知连接程序，程序从start标号开始执行
start:					! 以下10行作用是将自身(bootsect)从目前段位置07c0h(31k)
						! 移动到9000h(576k)处，共256字(512字节)，然后跳转到
						! 移动后代码的 go 标号处，也即本程序的下一语句处。 
	mov	ax,#BOOTSEG		
	mov	ds,ax			! ds和si配合使用，es和di配合使用
	mov	ax,#INITSEG
	mov	es,ax
	mov	cx,#256			! 移动计数值=256字
	sub	si,si
	sub	di,di
	rep
	movw				! move "boot-sector code" to new position
	jmpi	go,INITSEG	! 复制"boot-sector code"结束，与go:	mov	ax,cs一起使CPU跳转到新位置的下一个指令执行
go:	mov	ax,cs			! 将ds、es和ss都置成移动后代码所在的段处（9000h）。
	mov	ds,ax
	mov	es,ax
! put stack at 0x9ff00.	! 由于程序中有堆栈操作（push，pop，call），因此必须设置堆栈(9000h:0ff00h).
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >> 512
						! sp只要指向远大于512偏移（即地址90200h）处都可以。因为从90200h地址开始处还要放置setup程序，
						!  而此时setup程序大约为4个扇区，因此sp要指向大于（200h + 200h*4 + 堆栈大小）处。

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.

! 将第二批代码setup加载到bootsect代码之后(90200h处)
load_setup:				! 以下10行的用途是利用BIOS中断INT 13h将setup模块从磁盘第2个扇区
						! 开始读到90200h开始处，共读4个扇区。如果读出错，则复位驱动器，并
						! 重试，没有退路。
						! INT 13h 的使用方法如下：
						! ah = 02h - 读磁盘扇区到内存；	al = 需要读出的扇区数量；
						! ch = 磁道（柱面）号的低8位；  cl = 开始扇区（0－5位），磁道号高2位（6－7）；
						! dh = 磁头号；					dl = 驱动器号（如果是硬盘则要置为7）；
						! es:bx ->指向数据缓冲区；  如果出错则CF标志置位。 
	mov	dx,#0x0000		! drive 0, head 0
	mov	cx,#0x0002		! sector 2, track 0
	mov	bx,#0x0200		! address = 512, in INITSEG，es:bx指向数据缓冲区
	mov	ax,#0x0200+SETUPLEN	! service 2, nr of sectors
	int	0x13			! read it
	jnc	ok_load_setup		! ok - continue		! 出错则CF=1，不转移
	mov	dx,#0x0000
	mov	ax,#0x0000		! reset the diskette	! 复位磁盘
	int	0x13
	j	load_setup

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track
!	取磁盘驱动器的参数，特别是每道的扇区数量。
!   取磁盘驱动器参数INT 13h调用格式和返回信息如下：
!   ah = 08h	dl = 驱动器号（如果是硬盘则要置位7为1）。
!   返回信息：
!   如果出错则CF置位，并且ah = 状态码。
!   ah = 0, al = 0,         bl = 驱动器类型（AT/PS2）
!   ch = 最大磁道号的低8位，cl = 每磁道最大扇区数（位0-5），最大磁道号高2位（位6-7）
!   dh = 最大磁头数，       dl = 驱动器数量，
!   es:di -> 软驱磁盘参数表。 
	mov	dl,#0x00
	mov	ax,#0x0800		! AH=8 is get drive parameters
	int	0x13
	mov	ch,#0x00
	seg cs				! 表示下一条语句的操作数在cs段寄存器所指的段中
	mov	sectors,cx		! 保存每磁道扇区数。sectors定义在本文件尾部
	mov	ax,#INITSEG
	mov	es,ax			! 因为上面取磁盘参数中断改掉了es的值，这里重新改回。

! Print some inane message
! 显示提示信息（'Loading system...'）
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10			! cursor pos保存在dx中,dh = 行号(00 是顶端)，dl = 列号(00 是左边)。
	
	mov	cx,#24			! 共24个字符
	mov	bx,#0x0007		! page 0, attribute 7 (normal)
	mov	bp,#msg1		! 指向要显示的字符串。
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

! ok, we've written the message, now
! we want to load the system (at 0x10000)
! 将system 模块加载到10000h(64k)处
	mov	ax,#SYSSEG
	mov	es,ax			! segment of 0x010000
	call	read_it		! 读磁盘上system模块，es为输入参数。
	call	kill_motor	! 关闭驱动器马达，这样就可以知道驱动器的状态了。

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.
! 此后，检查要使用哪个根文件系统设备（简称根设备）。如果已经指定了设备（!=0）就直接使用给定的设备。
! 否则就需要根据BIOS报告的每磁道扇区数来确定到底使用/dev/PS0(2,28)还是/dev/at0(2,8)。
! 上面一行中两个设备文件的含义：
!	在Linux中软驱的主设备号是2（参加第43行注释），次设备号 = type*4 + nr, 其中nr为0－3分别对
!	应软驱A、B、C或D；type是软驱的类型（2->1.2M或7->1.44M等）。因为7*4 + 0 = 28，所以/dev/PS0(2,28)指的是
!	1.44M A驱动器，其设备号是021c,同理 /dev/at0(2,8)指的是1.2M A驱动器，其设备号是0208.

	seg cs				! 表示下一条语句的操作数在cs段寄存器所指的段中
	mov	ax,root_dev
	cmp	ax,#0
	jne	root_defined
	seg cs
	mov	bx,sectors		! 取上面保存的每磁道扇区数。如果sectors=15则说明是1.2Mb的驱动器；如果sectors=18，则说明是
						! 1.44Mb软驱。因为是可引导的驱动器，所以肯定是A驱。
	mov	ax,#0x0208		! /dev/ps0 - 1.2Mb
	cmp	bx,#15
	je	root_defined
	mov	ax,#0x021c		! /dev/PS0 - 1.44Mb
	cmp	bx,#18
	je	root_defined
undef_root:				! 如果都不一样，则死循环（死机）。
	jmp undef_root
root_defined:
	seg cs
	mov	root_dev,ax		! 将检查过的设备号保存起来。

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:
! 到此，所有程序都加载完毕，我们就跳转到被加载在bootsect后面的setup程序(9020:0000)去.

	jmpi	0,SETUPSEG

! －－－－－－－－－－－－ 本程序到此就执行结束了。－－－－－－－－－－－－－

! ******下面是两个子程序。*******

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.
!
! in:	es - starting address segment (normally 0x1000)
!
! 该子程序将系统模块加载到内存地址10000h处，并确定没有跨越64kB的内存边界。
! 我们试图尽快地进行加载，只要可能，就每次加载整条磁道的数据
! 输入：es － 开始内存地址段值（通常是1000h）

sread:	.word 1+SETUPLEN	! sectors read of current track	! 当前磁道中已读的扇区数。
head:	.word 0				! current head					! 当前磁头号
track:	.word 0				! current track					! 当前磁道号

read_it:
! 测试输入的段值。必须位于内存地址64KB边界处，否则进入死循环。清bx寄存器，用于表示当前段内存放数据的开始位置。
	mov ax,es
	test ax,#0x0fff
die:	jne die			! es must be at 64kB boundary
	xor bx,bx			! bx is starting address within segment
rp_read:
! 判断是否已经读入全部数据。比较当前所读段是否就是系统数据末端所处的段（#ENDSEG），如果
! 不是就跳转至下面ok1_read标号处继续读数据。否则退出子程序返回。
	mov ax,es
	cmp ax,#ENDSEG		! have we loaded all yet?
	jb ok1_read			! ax < ENDSEG,表示还没加载完
	ret
ok1_read:
! 计算和验证当前磁道需要读取的扇区数，放在ax寄存器中。根据当前磁道还未读取的扇区数以及段内数据字节开始偏移位置
! ，计算如果全部读取这些未读扇区，所读总字节数是否会超过64KB段长度的限制。若会超过，则根据此次最多能读入的字 
! 节数（64KB - 段内偏移位置），反算出此次需要读取的扇区数。
	seg cs				! 表示下一条语句的操作数在cs段寄存器所指的段中
	mov ax,sectors
	sub ax,sread
	mov cx,ax			! ax = 当前磁道未读扇区数。
	shl cx,#9			! 逻辑左移9位,512字节
	add cx,bx			! cx = cx + 段内当前偏移值（bx）,即此次读操作后，段内共读入的字节数
	jnc ok2_read
	je ok2_read
	xor ax,ax			! 注意!!!由于16位寄存器最大只能表示64KB -1，因此这里ax=0表示最大值64KB
	sub ax,bx			! 将ax作为无符号数看待
	shr ax,#9			! 逻辑右移9位，高位补0
ok2_read:
	call read_track		! 读取扇区
	mov cx,ax			! 该此操作已读取的扇区数。
	add ax,sread
	seg cs
	cmp ax,sectors
	jne ok3_read
	! 读该磁道的下一磁头面（1号磁头）上的数据。如果已经完成，则去读下一磁道。磁盘一般都是双头双面的!!!
	mov ax,#1
	sub ax,head
	jne ok4_read		! 如果是0磁头，则再去读1磁头面上的扇区数据。若是1磁头则读取下一磁道0磁头
	inc track
ok4_read:
	mov head,ax			! 保存当前磁头号。
	xor ax,ax			! 清当前磁道已读扇区数。
ok3_read:
	mov sread,ax
	shl cx,#9
	add bx,cx			! 把两操作数当做无符号数看待
	jnc rp_read			! 若小于64KB边界值，则跳转到rp_read处，继续读数据。否则调整当前段，为读下一段数据作准备。
						! 注意!!! CF标志位是对无符号数有意义的。参见80x86寄存器
	mov ax,es
	add ax,#0x1000
	mov es,ax			! 将段基址调整为指向下一个64KB段内存。
	xor bx,bx
	jmp rp_read

! 读当前磁道上指定开始扇区和需读扇区数的数据到es:bx开始处。
! INT 13h 的使用方法如下：
! ah = 02h - 读磁盘扇区到内存；	al = 需要读出的扇区数量；
! ch = 磁道（柱面）号的低8位；  cl = 开始扇区（0－5位），磁道号高2位（6－7）；
! dh = 磁头号；					dl = 驱动器号（如果是硬盘则要置为7）；
! es:bx ->指向数据缓冲区；  如果出错则CF标志置位。 
read_track:
	push ax
	push bx
	push cx
	push dx
	mov dx,track
	mov cx,sread
	inc cx			! cl+1 = 开始读扇区。
	mov ch,dl
	mov dx,head
	mov dh,dl
	mov dl,#0
	and dx,#0x0100	! 磁头号不大于1
	mov ah,#2
	int 0x13
	jc bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret
bad_rt:	mov ax,#0	! 执行驱动器复位操作（磁盘中断功能号0），再跳转到read_track处重试。
	mov dx,#0
	int 0x13
	pop dx
	pop cx
	pop bx
	pop ax
	jmp read_track

/*
 * This procedure turns off the floppy drive motor, so
 * that we enter the kernel in a known state, and
 * don't have to worry about it later.
 */
/* 这个子程序用于关闭软驱的马达，这样我们进入内核后它处于已知状态，以后也就无须担心它了。*/
kill_motor:
	push dx
	mov dx,#0x3f2	! 软驱控制卡的驱动端口，只写。
	mov al,#0		! A驱动器，关闭FDC，禁止DMA和中断请求，关闭马达。
	outb			! 将al中的内容输出到dx指定的端口去。
	pop dx
	ret

sectors:			! 存放当前启动软盘每磁道的扇区数。
	.word 0

msg1:				! 共24个ASCII码字符。
	.byte 13,10		! 回车、换行的ASCII码。
	.ascii "Loading system ..."
	.byte 13,10,13,10

.org 508			! 表示下面语句从地址508(1FC)开始，所以root_dev在启动扇区的第508开始的2个字节中。
root_dev:			! 这里存放根文件系统所在的设备号（init/main.c中会用）。
	.word ROOT_DEV
boot_flag:			! 硬盘有效标识。
	.word 0xAA55

.text
endtext:
.data
enddata:
.bss
endbss:
