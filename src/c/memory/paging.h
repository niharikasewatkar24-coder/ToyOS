#ifndef PAGING_H
#define PAGING_H

#include "../kernel/kernel.h"

/*
 * x86 uses 4 KiB pages in the paging mode
 * we are implementing.
 */
#define PAGE_SIZE 4096

/*
 * Initialize paging and enable the CPU paging bit.
 */
void paging_init(void);

/*
 * Handle a page fault.
 */
void page_fault_handler(u32 error_code);

#endif
