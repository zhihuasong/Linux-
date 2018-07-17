![passed]
!
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
!
! SYSSIZE��Ҫ���صĽ�����16�ֽ�Ϊ1�ڣ���3000h��Ϊ30000h�ֽڣ�192kB
! �Ե�ǰ�İ汾�ռ����㹻�ˡ�
SYSSIZE = 0x3000	! kernel size	! ���������һ�����Ĭ��ֵ��
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
!	boot��bios�������ӳ��������7c00h��31k�����������Լ��ƶ�����
!	��ַ90000h��576k����������ת�����
!	��Ȼ��ʹ��BIOS�жϽ�'setup'ֱ�Ӽ��ص��Լ��ĺ��棨90200h����576.5k����
!	����system���ص���ַ10000h����
!
!	ע�⣺Ŀǰ���ں�ϵͳ��󳤶�����Ϊ��8*65536����512kB���ֽڣ���ʹ����
!	������ҲӦ��û������ġ������������ּ����ˡ�����512k������ں˳���Ӧ��
!	�㹻�ˣ�����������û����minix��һ���������������ٻ��塣
!
!	���س����Ѿ����Ĺ����ˣ����Գ����Ķ�����������ѭ����ֻ���ֹ�������
!	ֻҪ���ܣ�ͨ��һ��ȡȡ���е����������ع��̿������ĺܿ�ġ�
!************************************************************************

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4				! nr of setup-sectors					! setup�������������setup��sectors��ֵ
BOOTSEG  = 0x07c0			! original address of boot-sector		! bootsect��ԭʼ��ַ���Ƕε�ַ������ͬ��
INITSEG  = 0x9000			! we move boot here - out of the way	! ��bootsect�Ƶ�����
SETUPSEG = 0x9020			! setup starts here						! setup��������￪ʼ
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).		! systemģ����ص�10000(64kB)��.
ENDSEG   = SYSSEG + SYSSIZE	! where to stop loading					! ֹͣ���صĶε�ַ

! ROOT_DEV:	0x000 - same type of floppy as boot.	! ���ļ�ϵͳ�豸ʹ��������ʱͬ���������豸.
!		0x301 - first partition on first drive etc	! ���ļ�ϵͳ�豸�ڵ�һ��Ӳ�̵ĵ�һ��������
ROOT_DEV = 0x306									! ָ�����ļ�ϵͳ�豸�ǵ�2��Ӳ�̵ĵ�1��������
						! ����Linux��ʽ���豸������ʽ������ֵ�ĺ������£�
						! �豸�� �� ���豸��*256 �� ���豸�� 
						!           (Ҳ�� dev_no = (major<<8 + minor)
						! (���豸�ţ�1���ڴ棬2�����̣�3��Ӳ�̣�4��ttyx��5��tty��6�����пڣ�7���������ܵ�)
						! 300 - /dev/hd0 �� ����������1��Ӳ��
						! 301 - /dev/hd1 �� ��1���̵ĵ�1������
						! ... ...
						! 304 - /dev/hd4 �� ��1���̵ĵ�4������
						! 305 - /dev/hd5 �� ����������2��Ӳ��
						! 306 - /dev/hd6 �� ��2���̵ĵ�1������
						! ... ...
						! 309 - /dev/hd9 �� ��2���̵ĵ�4������ 


! ����ϵͳ��ʼ�滮�ڴ棬����һ������bootsect���Ƶ��Լ�ָ����λ�ã���������Ӧ��ջ!!!(����ϵͳ��ʼ��ʱ��ʱʹ�õĶ�ջ)
entry start				! ��֪���ӳ��򣬳����start��ſ�ʼִ��
start:					! ����10�������ǽ�����(bootsect)��Ŀǰ��λ��07c0h(31k)
						! �ƶ���9000h(576k)������256��(512�ֽ�)��Ȼ����ת��
						! �ƶ������� go ��Ŵ���Ҳ�����������һ��䴦�� 
	mov	ax,#BOOTSEG		
	mov	ds,ax			! ds��si���ʹ�ã�es��di���ʹ��
	mov	ax,#INITSEG
	mov	es,ax
	mov	cx,#256			! �ƶ�����ֵ=256��
	sub	si,si
	sub	di,di
	rep
	movw				! move "boot-sector code" to new position
	jmpi	go,INITSEG	! ����"boot-sector code"��������go:	mov	ax,csһ��ʹCPU��ת����λ�õ���һ��ָ��ִ��
go:	mov	ax,cs			! ��ds��es��ss���ó��ƶ���������ڵĶδ���9000h����
	mov	ds,ax
	mov	es,ax
! put stack at 0x9ff00.	! ���ڳ������ж�ջ������push��pop��call������˱������ö�ջ(9000h:0ff00h).
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >> 512
						! spֻҪָ��Զ����512ƫ�ƣ�����ַ90200h���������ԡ���Ϊ��90200h��ַ��ʼ����Ҫ����setup����
						!  ����ʱsetup�����ԼΪ4�����������spҪָ����ڣ�200h + 200h*4 + ��ջ��С������

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.

! ���ڶ�������setup���ص�bootsect����֮��(90200h��)
load_setup:				! ����10�е���;������BIOS�ж�INT 13h��setupģ��Ӵ��̵�2������
						! ��ʼ����90200h��ʼ��������4�������������������λ����������
						! ���ԣ�û����·��
						! INT 13h ��ʹ�÷������£�
						! ah = 02h - �������������ڴ棻	al = ��Ҫ����������������
						! ch = �ŵ������棩�ŵĵ�8λ��  cl = ��ʼ������0��5λ�����ŵ��Ÿ�2λ��6��7����
						! dh = ��ͷ�ţ�					dl = �������ţ������Ӳ����Ҫ��Ϊ7����
						! es:bx ->ָ�����ݻ�������  ���������CF��־��λ�� 
	mov	dx,#0x0000		! drive 0, head 0
	mov	cx,#0x0002		! sector 2, track 0
	mov	bx,#0x0200		! address = 512, in INITSEG��es:bxָ�����ݻ�����
	mov	ax,#0x0200+SETUPLEN	! service 2, nr of sectors
	int	0x13			! read it
	jnc	ok_load_setup		! ok - continue		! ������CF=1����ת��
	mov	dx,#0x0000
	mov	ax,#0x0000		! reset the diskette	! ��λ����
	int	0x13
	j	load_setup

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track
!	ȡ�����������Ĳ������ر���ÿ��������������
!   ȡ��������������INT 13h���ø�ʽ�ͷ�����Ϣ���£�
!   ah = 08h	dl = �������ţ������Ӳ����Ҫ��λ7Ϊ1����
!   ������Ϣ��
!   ���������CF��λ������ah = ״̬�롣
!   ah = 0, al = 0,         bl = ���������ͣ�AT/PS2��
!   ch = ���ŵ��ŵĵ�8λ��cl = ÿ�ŵ������������λ0-5�������ŵ��Ÿ�2λ��λ6-7��
!   dh = ����ͷ����       dl = ������������
!   es:di -> �������̲����� 
	mov	dl,#0x00
	mov	ax,#0x0800		! AH=8 is get drive parameters
	int	0x13
	mov	ch,#0x00
	seg cs				! ��ʾ��һ�����Ĳ�������cs�μĴ�����ָ�Ķ���
	mov	sectors,cx		! ����ÿ�ŵ���������sectors�����ڱ��ļ�β��
	mov	ax,#INITSEG
	mov	es,ax			! ��Ϊ����ȡ���̲����жϸĵ���es��ֵ���������¸Ļء�

! Print some inane message
! ��ʾ��ʾ��Ϣ��'Loading system...'��
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10			! cursor pos������dx��,dh = �к�(00 �Ƕ���)��dl = �к�(00 �����)��
	
	mov	cx,#24			! ��24���ַ�
	mov	bx,#0x0007		! page 0, attribute 7 (normal)
	mov	bp,#msg1		! ָ��Ҫ��ʾ���ַ�����
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

! ok, we've written the message, now
! we want to load the system (at 0x10000)
! ��system ģ����ص�10000h(64k)��
	mov	ax,#SYSSEG
	mov	es,ax			! segment of 0x010000
	call	read_it		! ��������systemģ�飬esΪ���������
	call	kill_motor	! �ر��������������Ϳ���֪����������״̬�ˡ�

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.
! �˺󣬼��Ҫʹ���ĸ����ļ�ϵͳ�豸����Ƹ��豸��������Ѿ�ָ�����豸��!=0����ֱ��ʹ�ø������豸��
! �������Ҫ����BIOS�����ÿ�ŵ���������ȷ������ʹ��/dev/PS0(2,28)����/dev/at0(2,8)��
! ����һ���������豸�ļ��ĺ��壺
!	��Linux�����������豸����2���μӵ�43��ע�ͣ������豸�� = type*4 + nr, ����nrΪ0��3�ֱ��
!	Ӧ����A��B��C��D��type�����������ͣ�2->1.2M��7->1.44M�ȣ�����Ϊ7*4 + 0 = 28������/dev/PS0(2,28)ָ����
!	1.44M A�����������豸����021c,ͬ�� /dev/at0(2,8)ָ����1.2M A�����������豸����0208.

	seg cs				! ��ʾ��һ�����Ĳ�������cs�μĴ�����ָ�Ķ���
	mov	ax,root_dev
	cmp	ax,#0
	jne	root_defined
	seg cs
	mov	bx,sectors		! ȡ���汣���ÿ�ŵ������������sectors=15��˵����1.2Mb�������������sectors=18����˵����
						! 1.44Mb��������Ϊ�ǿ������������������Կ϶���A����
	mov	ax,#0x0208		! /dev/ps0 - 1.2Mb
	cmp	bx,#15
	je	root_defined
	mov	ax,#0x021c		! /dev/PS0 - 1.44Mb
	cmp	bx,#18
	je	root_defined
undef_root:				! �������һ��������ѭ������������
	jmp undef_root
root_defined:
	seg cs
	mov	root_dev,ax		! ���������豸�ű���������

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:
! ���ˣ����г��򶼼�����ϣ����Ǿ���ת����������bootsect�����setup����(9020:0000)ȥ.

	jmpi	0,SETUPSEG

! ������������������������ �����򵽴˾�ִ�н����ˡ���������������������������

! ******�����������ӳ���*******

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.
!
! in:	es - starting address segment (normally 0x1000)
!
! ���ӳ���ϵͳģ����ص��ڴ��ַ10000h������ȷ��û�п�Խ64kB���ڴ�߽硣
! ������ͼ����ؽ��м��أ�ֻҪ���ܣ���ÿ�μ��������ŵ�������
! ���룺es �� ��ʼ�ڴ��ַ��ֵ��ͨ����1000h��

sread:	.word 1+SETUPLEN	! sectors read of current track	! ��ǰ�ŵ����Ѷ�����������
head:	.word 0				! current head					! ��ǰ��ͷ��
track:	.word 0				! current track					! ��ǰ�ŵ���

read_it:
! ��������Ķ�ֵ������λ���ڴ��ַ64KB�߽紦�����������ѭ������bx�Ĵ��������ڱ�ʾ��ǰ���ڴ�����ݵĿ�ʼλ�á�
	mov ax,es
	test ax,#0x0fff
die:	jne die			! es must be at 64kB boundary
	xor bx,bx			! bx is starting address within segment
rp_read:
! �ж��Ƿ��Ѿ�����ȫ�����ݡ��Ƚϵ�ǰ�������Ƿ����ϵͳ����ĩ�������ĶΣ�#ENDSEG�������
! ���Ǿ���ת������ok1_read��Ŵ����������ݡ������˳��ӳ��򷵻ء�
	mov ax,es
	cmp ax,#ENDSEG		! have we loaded all yet?
	jb ok1_read			! ax < ENDSEG,��ʾ��û������
	ret
ok1_read:
! �������֤��ǰ�ŵ���Ҫ��ȡ��������������ax�Ĵ����С����ݵ�ǰ�ŵ���δ��ȡ���������Լ����������ֽڿ�ʼƫ��λ��
! ���������ȫ����ȡ��Щδ���������������ֽ����Ƿ�ᳬ��64KB�γ��ȵ����ơ����ᳬ��������ݴ˴�����ܶ������ 
! ������64KB - ����ƫ��λ�ã���������˴���Ҫ��ȡ����������
	seg cs				! ��ʾ��һ�����Ĳ�������cs�μĴ�����ָ�Ķ���
	mov ax,sectors
	sub ax,sread
	mov cx,ax			! ax = ��ǰ�ŵ�δ����������
	shl cx,#9			! �߼�����9λ,512�ֽ�
	add cx,bx			! cx = cx + ���ڵ�ǰƫ��ֵ��bx��,���˴ζ������󣬶��ڹ�������ֽ���
	jnc ok2_read
	je ok2_read
	xor ax,ax			! ע��!!!����16λ�Ĵ������ֻ�ܱ�ʾ64KB -1���������ax=0��ʾ���ֵ64KB
	sub ax,bx			! ��ax��Ϊ�޷���������
	shr ax,#9			! �߼�����9λ����λ��0
ok2_read:
	call read_track		! ��ȡ����
	mov cx,ax			! �ô˲����Ѷ�ȡ����������
	add ax,sread
	seg cs
	cmp ax,sectors
	jne ok3_read
	! ���ôŵ�����һ��ͷ�棨1�Ŵ�ͷ���ϵ����ݡ�����Ѿ���ɣ���ȥ����һ�ŵ�������һ�㶼��˫ͷ˫���!!!
	mov ax,#1
	sub ax,head
	jne ok4_read		! �����0��ͷ������ȥ��1��ͷ���ϵ��������ݡ�����1��ͷ���ȡ��һ�ŵ�0��ͷ
	inc track
ok4_read:
	mov head,ax			! ���浱ǰ��ͷ�š�
	xor ax,ax			! �嵱ǰ�ŵ��Ѷ���������
ok3_read:
	mov sread,ax
	shl cx,#9
	add bx,cx			! ���������������޷���������
	jnc rp_read			! ��С��64KB�߽�ֵ������ת��rp_read�������������ݡ����������ǰ�Σ�Ϊ����һ��������׼����
						! ע��!!! CF��־λ�Ƕ��޷�����������ġ��μ�80x86�Ĵ���
	mov ax,es
	add ax,#0x1000
	mov es,ax			! ���λ�ַ����Ϊָ����һ��64KB���ڴ档
	xor bx,bx
	jmp rp_read

! ����ǰ�ŵ���ָ����ʼ��������������������ݵ�es:bx��ʼ����
! INT 13h ��ʹ�÷������£�
! ah = 02h - �������������ڴ棻	al = ��Ҫ����������������
! ch = �ŵ������棩�ŵĵ�8λ��  cl = ��ʼ������0��5λ�����ŵ��Ÿ�2λ��6��7����
! dh = ��ͷ�ţ�					dl = �������ţ������Ӳ����Ҫ��Ϊ7����
! es:bx ->ָ�����ݻ�������  ���������CF��־��λ�� 
read_track:
	push ax
	push bx
	push cx
	push dx
	mov dx,track
	mov cx,sread
	inc cx			! cl+1 = ��ʼ��������
	mov ch,dl
	mov dx,head
	mov dh,dl
	mov dl,#0
	and dx,#0x0100	! ��ͷ�Ų�����1
	mov ah,#2
	int 0x13
	jc bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret
bad_rt:	mov ax,#0	! ִ����������λ�����������жϹ��ܺ�0��������ת��read_track�����ԡ�
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
/* ����ӳ������ڹر����������������ǽ����ں˺���������֪״̬���Ժ�Ҳ�����뵣�����ˡ�*/
kill_motor:
	push dx
	mov dx,#0x3f2	! �������ƿ��������˿ڣ�ֻд��
	mov al,#0		! A���������ر�FDC����ֹDMA���ж����󣬹ر���
	outb			! ��al�е����������dxָ���Ķ˿�ȥ��
	pop dx
	ret

sectors:			! ��ŵ�ǰ��������ÿ�ŵ�����������
	.word 0

msg1:				! ��24��ASCII���ַ���
	.byte 13,10		! �س������е�ASCII�롣
	.ascii "Loading system ..."
	.byte 13,10,13,10

.org 508			! ��ʾ�������ӵ�ַ508(1FC)��ʼ������root_dev�����������ĵ�508��ʼ��2���ֽ��С�
root_dev:			! �����Ÿ��ļ�ϵͳ���ڵ��豸�ţ�init/main.c�л��ã���
	.word ROOT_DEV
boot_flag:			! Ӳ����Ч��ʶ��
	.word 0xAA55

.text
endtext:
.data
enddata:
.bss
endbss:
