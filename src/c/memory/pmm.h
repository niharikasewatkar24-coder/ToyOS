#ifndef PMM_H
#define PMM_H

#include "../kernel/kernel.h"

#define PAGE_SIZE 4096
#define PMM_MAX_MEMORY 32 * 1024 * 1024
#define PMM_MAX_PAGES (PMM_MAX_MEMORY / PAGE_SIZE)

void pmm_init(void);

void* pmm_alloc_page(void);

void pmm_free_page(void* address);

u32 pmm_get_total_pages(void);

u32 pmm_get_used_pages(void);

#endif
