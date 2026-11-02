#ifndef PAGING_H
#define PAGING_H

#include "../kernel/kernel.h"

#define PAGE_SIZE 4096

/* Initialize paging. */
void paging_init(void);

/* Handle page faults. */
void page_fault_handler(u32 error_code);

/* Allow a selected mapped page to be accessed from Ring 3. */
int paging_map_user_page(u32 address);

/* Check whether a page is user-accessible. */
int paging_is_user_page(u32 address);

#endif
