/*
 * mm/plpc.c
 *
 * Written by Sebastien Valat.
 *
 * Process Local Page Cache code <sebastien.valat@cea.fr>
 */

#ifndef LINUX_MM_PLPC_H
#define LINUX_MM_PLPC_H

#include <linux/spinlock.h>
//#include <linux/mm_types.h>
#include <linux/list.h>

struct plpc
{
	spinlock_t lock;
	struct list_head pages;
};

struct plpc_entry
{
	struct list_head list;
	struct page * page;
};

//to avoid loop in definitions with mm_types.h
struct vm_area_struct;

void plpc_vm_init(struct plpc * plpc);
void plpc_vm_fork(struct plpc * plpc_new,struct plpc * plpc_orig);
void plpc_vm_release(struct plpc * plpc);
struct page * plpc_get_internal_page(struct plpc * plpc);
struct page * plpc_get_page(struct plpc * plpc,struct vm_area_struct *vma,unsigned long vaddr);
void plpc_reg_page(struct plpc * plpc,struct page * page);

//#define PLPC_DEBUG(str,args...) printk(KERN_DEBUG "PLPC - " str,##args);
#define PLPC_DEBUG(str,args...) do {} while(0)

#endif //LINUX_MM_PLPC_H
