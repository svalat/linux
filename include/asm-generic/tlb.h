/* include/asm-generic/tlb.h
 *
 *	Generic TLB shootdown code
 *
 * Copyright 2001 Red Hat, Inc.
 * Based on code from mm/memory.c Copyright Linus Torvalds and others.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_GENERIC__TLB_H
#define _ASM_GENERIC__TLB_H

#include <linux/swap.h>
#include <linux/mm_plpc.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

/*
 * For UP we don't need to worry about TLB flush
 * and page free order so much..
 */
#ifdef CONFIG_SMP
  #ifdef ARCH_FREE_PTR_NR
    #define FREE_PTR_NR   ARCH_FREE_PTR_NR
  #else
    #define FREE_PTE_NR	506
  #endif
  #define tlb_fast_mode(tlb) ((tlb)->nr == ~0U)
#else
  #define FREE_PTE_NR	1
  #define tlb_fast_mode(tlb) 1
#endif

/* struct mmu_gather is an opaque type used by the mm code for passing around
 * any data needed by arch specific code for tlb_remove_page.
 */
struct mmu_gather {
	struct mm_struct	*mm;
	unsigned int		nr;	/* set to ~0U means fast mode */
	unsigned int		need_flush;/* Really unmapped some ptes? */
	unsigned int		fullmm; /* non-zero means full mm flush */
	struct page *		pages[FREE_PTE_NR];
#ifdef CONFIG_PLPC
	struct page *		plpc_capture[FREE_PTE_NR]; /* Need boolean, but as first step, ensure security by checking correct state. */
#endif
};

/* Users of the generic TLB shootdown code must declare this storage space. */
DECLARE_PER_CPU(struct mmu_gather, mmu_gathers);

/* tlb_gather_mmu
 *	Return a pointer to an initialized struct mmu_gather.
 */
static inline struct mmu_gather *
tlb_gather_mmu(struct mm_struct *mm, unsigned int full_mm_flush)
{
	struct mmu_gather *tlb = &get_cpu_var(mmu_gathers);

	tlb->mm = mm;

	/* Use fast mode if only one CPU is online */
	tlb->nr = num_online_cpus() > 1 ? 0U : ~0U;

	tlb->fullmm = full_mm_flush;

	return tlb;
}

static inline void
tlb_flush_mmu(struct mmu_gather *tlb, unsigned long start, unsigned long end)
{
	//int i;
	if (!tlb->need_flush)
		return;
	tlb->need_flush = 0;
	tlb_flush(tlb);
	if (!tlb_fast_mode(tlb)) {
		free_pages_and_swap_cache(tlb->pages, tlb->nr,tlb);
/*#ifdef CONFIG_PLPC
		for (i = 0 ; i < tlb->nr ;i++)
			tlb->plpc_capture[i] = NULL;
#endif //CONFIG_PLPC*/
		tlb->nr = 0;
	}
}

/* tlb_finish_mmu
 *	Called at the end of the shootdown operation to free up any resources
 *	that were required.
 */
static inline void
tlb_finish_mmu(struct mmu_gather *tlb, unsigned long start, unsigned long end)
{
	tlb_flush_mmu(tlb, start, end);

	/* keep the page table cache within bounds */
	check_pgt_cache();

	put_cpu_var(mmu_gathers);
}

/* tlb_remove_page
 *	Must perform the equivalent to __free_pte(pte_get_and_clear(ptep)), while
 *	handling the additional races in SMP caused by other CPUs caching valid
 *	mappings in their TLBs.
 */
static inline void tlb_remove_page(struct vm_area_struct *vma,struct mmu_gather *tlb, struct page *page)
{
	tlb->need_flush = 1;
	if (tlb_fast_mode(tlb)) {
#ifdef CONFIG_PLPC //_SKIPED
	//TODO avoid dupolication here
		if (vma != NULL && vma->vm_flags & VM_PAGE_REUSE && vma->vm_mm != NULL)
		{
			//printk(KERN_DEBUG "PLPC - page free, enabled on VMA (%p)",vma);
			free_page_and_swap_cache_plpc(page);
			VM_BUG_ON(atomic_read(&page->_count) == 0);
			plpc_reg_page(&vma->vm_mm->plpc,page);
		} else {
			//printk(KERN_DEBUG "PLPC - skip page free, not enabled on VMA (%p)",vma);
			free_page_and_swap_cache(page);
		}
#else
		free_page_and_swap_cache(page);
#endif
		return;
	}
#ifdef CONFIG_PLPC //_SKIPED
	else if (vma != NULL && vma->vm_flags & VM_PAGE_REUSE && vma->vm_mm != NULL)
	{
			//increase ref counter to avoid a free in tlb_flush_mmu, need to check that their is
			//no side effects, otherwise wi need to patch tlb_flush_mmu.
			//get_page(page);
			//BUG_ON(true);//hummm need to do something here for the non SMT modei
			SetPageReuse(page);
			tlb->plpc_capture[tlb->nr] = page;
			tlb->pages[tlb->nr++] = page;
			if (tlb->nr >= FREE_PTE_NR)
				tlb_flush_mmu(tlb, 0, 0);
			//printk(KERN_DEBUG "PLPC - --> !tlbfastmode : page free, enabled on VMA (%p)",vma);
			//VM_BUG_ON(atomic_read(&page->_count) == 0);
			//plpc_reg_page(&vma->vm_mm->plpc,page);
	}
#endif
	else {
#ifdef CONFIG_PLPC
		tlb->plpc_capture[tlb->nr] = NULL;
#endif //CONFIG_PLPC
		tlb->pages[tlb->nr++] = page;
		if (tlb->nr >= FREE_PTE_NR)
			tlb_flush_mmu(tlb, 0, 0);
	}
}

/**
 * tlb_remove_tlb_entry - remember a pte unmapping for later tlb invalidation.
 *
 * Record the fact that pte's were really umapped in ->need_flush, so we can
 * later optimise away the tlb invalidate.   This helps when userspace is
 * unmapping already-unmapped pages, which happens quite a lot.
 */
#define tlb_remove_tlb_entry(tlb, ptep, address)		\
	do {							\
		tlb->need_flush = 1;				\
		__tlb_remove_tlb_entry(tlb, ptep, address);	\
	} while (0)

#define pte_free_tlb(tlb, ptep, address)			\
	do {							\
		tlb->need_flush = 1;				\
		__pte_free_tlb(tlb, ptep, address);		\
	} while (0)

#ifndef __ARCH_HAS_4LEVEL_HACK
#define pud_free_tlb(tlb, pudp, address)			\
	do {							\
		tlb->need_flush = 1;				\
		__pud_free_tlb(tlb, pudp, address);		\
	} while (0)
#endif

#define pmd_free_tlb(tlb, pmdp, address)			\
	do {							\
		tlb->need_flush = 1;				\
		__pmd_free_tlb(tlb, pmdp, address);		\
	} while (0)

#define tlb_migrate_finish(mm) do {} while (0)

#endif /* _ASM_GENERIC__TLB_H */
