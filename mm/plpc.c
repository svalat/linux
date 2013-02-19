/*
 * mm/plpc.c
 *
 * Written by Sebastien Valat.
 *
 * Process Local Page Cache code <sebastien.valat@cea.fr>
 */

#include <linux/mm_plpc.h>
#include <linux/spinlock.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <asm/bug.h>

//TODO rename into mm_plpc
//TODO rename functions into mm_plpc_*
//TODO maybe rename into Process Local Memory Pool => PLMP

void plpc_vm_init(struct plpc * plpc)
{
	PLPC_DEBUG("Init (%p)\n",plpc);
	spin_lock_init(&plpc->lock);
	INIT_LIST_HEAD(&plpc->pages);
}

void plpc_vm_fork(struct plpc * plpc_new,struct plpc * plpc_orig)
{
	PLPC_DEBUG("Fork (old=%p -> new=%p)\n",plpc_orig,plpc_new);
	plpc_vm_init(plpc_new);
}

void plpc_vm_release(struct plpc * plpc)
{
	//vars
	struct plpc_entry * entry = NULL;
	struct plpc_entry * next = NULL;

	PLPC_DEBUG("Cleanup (plpc=%p)\n",plpc);

	if (plpc == NULL)
	{
		printk(KERN_ERR "PLPC - Get NULL plpc struct in release function.");
		return;
	}

	//lock current plpc
	spin_lock(&plpc->lock);

	//loop on all pages in cache
	list_for_each_entry_safe(entry, next, &plpc->pages, list)
	{
		//remove entry from list
		list_del(&entry->list);
		//free it
		PLPC_DEBUG("__free_page(%p)",entry->page);
		//TODO maybe in kernel this is better to use this :
		//__free_page(entry->page);
		free_page_and_swap_cache(entry->page);
		//free entry
		kfree(entry);
		//TODO
		//kmem_cache_free(plpc_cache,entry);
	}

	//unlock
	//spin_unlock(&plpc->lock);
	PLPC_DEBUG("end of loop");
}

struct page * plpc_get_internal_page(struct plpc * plpc)
{
	//vars
	struct page * page = NULL;
	struct plpc_entry * entry = NULL;

	PLPC_DEBUG("ok do it");

	//lock current plpc
	spin_lock(&plpc->lock);

	PLPC_DEBUG("get internal page (plpc=%p)",plpc);

	if (plpc == NULL)
	{
		printk(KERN_ERR "PLPC - Get NULL plpc struct on get_internal_page");
		return NULL;
	}

	//if not empty we got a page
	if (!list_empty(&plpc->pages)) {
		PLPC_DEBUG("OK, get entries");
		//get first one
		entry = list_entry(plpc->pages.prev, struct plpc_entry, list);
		PLPC_DEBUG("OK, page is %p",entry->page);
		//remove from list
		list_del(&entry->list);
		//get the page
		page = entry->page;
		//free the memory of the entry
		kfree(entry);
		//TODO
		//kmem_cache_free(plpc_cache,entry);
	}

	//unlock
	spin_unlock(&plpc->lock);

	PLPC_DEBUG("Returned page is %p",page);
	//return the page
	return page;
}

struct page * plpc_get_page(struct plpc * plpc,struct vm_area_struct *vma,unsigned long vaddr)
{
	struct page * page = NULL;

	PLPC_DEBUG("Get page (plpc=%p)\n",plpc);

	//try to get one from local cache
	page = plpc_get_internal_page(plpc);

	//if not request new one from kernel
	if (page == NULL)
	{
		PLPC_DEBUG("Need to get new page from kernel");
		/*page = alloc_pages(
			GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP | __GFP_NOMEMALLOC
			| __GFP_MOVABLE | __GFP_NORETRY | __GFP_NOWARN,0);*/
		page = alloc_zeroed_user_highpage_movable(vma,vaddr);
		//TODO tmp test
		//page = alloc_page_vma(GFP_HIGHUSER | __GFP_MOVABLE,vma,vaddr);
		PLPC_DEBUG("Finally get page from kernel %p",page);
	} else {
		BUG_ON(!PageReuse(page));
		ClearPageReuse(page);
	}

	return page;
}

void plpc_reg_page(struct plpc * plpc,struct page * page)
{
	//vars
	struct plpc_entry * page_entry = NULL;

	PLPC_DEBUG("Reg page on free for future reuse\n");

	if (page == NULL)
		return;

	PLPC_DEBUG("plpc = %p , page = %p, refs = %d",plpc,page,atomic_read(&page->_count));
	if (plpc == NULL)
	{
		printk(KERN_ERR "PLPC - Get NULL plpc struct on reg_page");
		free_page_and_swap_cache(page);
		return;
	}

	//lock current plpc
	spin_lock(&plpc->lock);

	//alloc page entry
	//page_entry = kmem_cache_alloc(plpc_cache,GFP_KERNEL);
	page_entry =  kmalloc(sizeof(struct plpc_entry),GFP_KERNEL);

	//setup
	INIT_LIST_HEAD(&page_entry->list);
	page_entry->page = page;
	//get_page(page);

	PLPC_DEBUG("after reg, refs = %d",atomic_read(&page->_count));

	//add to list
	list_add(&page_entry->list,&plpc->pages);

	//unlock
	spin_unlock(&plpc->lock);
}
