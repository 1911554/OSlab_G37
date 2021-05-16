#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_fifo.h>
#include <list.h>
//页错误异常发生时，有可能是因为页面保存在swap区或者磁盘文件上造成的，所以我们需要通过页面分配解决这个问题。 页面替换主要分为两个方面，页面换出和页面换入。
/* [wikipedia]The simplest Page Replacement Algorithm(PRA) is a FIFO algorithm. The first-in, first-out
 * page replacement algorithm is a low-overhead algorithm that requires little book-keeping on
 * the part of the operating system. The idea is obvious from the name - the operating system
 * keeps track of all the pages in memory in a queue, with the most recent arrival at the back,
 * and the earliest arrival in front. When a page needs to be replaced, the page at the front
 * of the queue (the oldest page) is selected. While FIFO is cheap and intuitive, it performs
 * poorly in practical application. Thus, it is rarely used in its unmodified form. This
 * algorithm experiences Belady's anomaly.
 *
 * Details of FIFO PRA
 * (1) Prepare: In order to implement FIFO PRA, we should manage all swappable pages, so we can
 *              link these pages into pra_list_head according the time order. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list
 *              implementation. You should know howto USE: list_init, list_add(list_add_after),
 *              list_add_before, list_del, list_next, list_prev. Another tricky method is to transform
 *              a general list struct to a special struct (such as struct page). You can find some MACRO:
 *              le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.
 */

list_entry_t pra_list_head;
/*
 * (2) _fifo_init_mm: init pra_list_head and let  mm->sm_priv point to the addr of pra_list_head.
 *              Now, From the memory control struct mm_struct, we can access FIFO PRA
 *		从内存控制结构mm\u结构，我们可以访问FIFO PRA
 */
static int
_fifo_init_mm(struct mm_struct *mm)
{     
     list_init(&pra_list_head);
     mm->sm_priv = &pra_list_head;
     //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
     return 0;
}
/*
 * (3)_fifo_map_swappable: According FIFO PRA, we should link the most recent arrival page at the back of pra_list_head qeueue
 */
 //将该页面设置为可交换的
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{//用于记录页访问情况相关属性
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    //record the page access situlation
    /*LAB3 EXERCISE 2: YOUR CODE*/ 
    //(1)link the most recent arrival page at the back of the pra_list_head qeueue.
    //（1） 链接pra_list_ head queue后面的最近到达页面。
    list_add(head, entry);//将最近使用的页面添加到次序的队尾
    return 0;
}
/*
 *  (4)_fifo_swap_out_victim: According FIFO PRA, we should unlink the  earliest arrival page in front of pra_list_head qeueue,
 *                            then assign the value of *ptr_page to the addr of this page.
 		根据fifo PRA，我们应该取消PRA list head QUEUEUE前面最早到达页面的链接，然后将*ptr\u page的值赋给该页面的addr。
 */
 //页面换出
 //挑选需要换出的页
static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
     /* Select the victim */
     /*LAB3 EXERCISE 2: YOUR CODE*/ 
     //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
     //(2)  assign the value of *ptr_page to the addr of this page
     /* Select the tail */
     //（1） 取消链接pra\u列表前的最早到达页面\u head queue
     //（2） 将*ptr\u页面的值分配给此页的addr
     /*选择尾部*/
     list_entry_t *le = head->prev;//用le指示需要被换出的页
     assert(head!=le);
     
     //dirtybit extendedclock////////////////////////////////////
     //dirtybit的extendedclock替换算法
//根据页面近期是否被修改来决定该页面是否换出。
//所以在查询空闲页时判断dirtybit
//操作系统淘汰页时，对页表项进行查询， 
//dirtybit是0，则换出
//dirtybit是1，则置0
//未被访问，未被修改的页优先换出。所以要多轮循环。
	//查找未被修改的页
     while(le!=head){
     	struct Page *p=le2page(le,pra_page_link);
     	pte_t*ptep=get_pte(mm->pgdir,p->pra_vaddr,0);//获取页表项
     	if(!(*ptep&PTE_A)&&!(*ptep&PTE_D)){//判断页表项是否正确
     	//未被访问，未被修改;换出后删除
     		list_del(le);
     		assert(p!=NULL);
     		*ptr_page=p;//地址存储在prt_page
     		return 0;
     	}
     	le=le->next;
     }
     le=le->next;
     
      while(le!=head){
     	struct Page *p=le2page(le,pra_page_link);
     	pte_t*ptep=get_pte(mm->pgdir,p->pra_vaddr,0);
     	if(!(*ptep&PTE_A)&&!(*ptep&PTE_D)){//未被访问，已被修改
     		list_del(le);
     		assert(p!=NULL);
     		*ptr_page=p;
     		return 0;
     	}
     	*ptep^=PTE_A;/////////////////////////页被访问过则将PTE_A置0
     	le=le->next;
     }
     le=le->next;
     
      while(le!=head){
     	struct Page *p=le2page(le,pra_page_link);
     	pte_t*ptep=get_pte(mm->pgdir,p->pra_vaddr,0);
     	if(*ptep&PTE_D){//都被访问过了，PTE_A都是0。
     		list_del(le);
     		assert(p!=NULL);
     		*ptr_page=p;
     		return 0;
     	}
     	le=le->next;
     }
     le=le->next;
     //都不符合。那就换出最先进入的页面
      while(le!=head){
     	struct Page *p=le2page(le,pra_page_link);
     	pte_t*ptep=get_pte(mm->pgdir,p->pra_vaddr,0);
     	if(*ptep&PTE_D){
     		list_del(le);
     		assert(p!=NULL);
     		*ptr_page=p;
     		return 0;
     	}
     	le=le->next;
     }
    
     
     
     
     
     
     
     
     
     
     
     //dirtybit extendedcolck////////////////////////////////////
    // struct Page *p = le2page(le, pra_page_link);//le2page 宏可以根据链表元素，获得对应page的指针
     //list_del(le);//将进来最早的页面从队列中删除
     //assert(p !=NULL);
     //*ptr_page = p;//将这一页的地址存储在ptr_page中
     //return 0;
}

static int
_fifo_check_swap(void) {
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==4);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==4);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==4);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==4);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==5);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==6);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==7);
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==8);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==9);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==10);
    cprintf("write Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==11);
    return 0;
}


static int
_fifo_init(void)
{
    return 0;
}

static int
_fifo_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_fifo_tick_event(struct mm_struct *mm)
{ return 0; }


struct swap_manager swap_manager_fifo =
{
     .name            = "fifo swap manager",
     .init            = &_fifo_init,
     .init_mm         = &_fifo_init_mm,
     .tick_event      = &_fifo_tick_event,
     .map_swappable   = &_fifo_map_swappable,
     .set_unswappable = &_fifo_set_unswappable,
     .swap_out_victim = &_fifo_swap_out_victim,
     .check_swap      = &_fifo_check_swap,
};
