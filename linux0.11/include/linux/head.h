#ifndef _HEAD_H
#define _HEAD_H

// ȫ�ֻ��ж����������ݽṹ
typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

extern unsigned long pg_dir[1024];
extern desc_table idt,gdt;			// idt,gdt����head.s�ж���Ϊȫ�ֱ�����.globl _idt,_gdt��

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif
