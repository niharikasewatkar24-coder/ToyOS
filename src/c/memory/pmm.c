#include "pmm.h"

#define BITMAP_SIZE (PMM_MAX_PAGES / 8)

static unsigned char page_bitmap[BITMAP_SIZE];

static u32 total_pages = PMM_MAX_PAGES;
static u32 used_pages = 0;

/*
 * Mark a physical page as used.
 */
static void mark_page_used(u32 page)
{
    page_bitmap[page / 8] |= (1 << (page % 8));
}

/*
 * Mark a physical page as free.
 */
static void mark_page_free(u32 page)
{
    page_bitmap[page / 8] &= ~(1 << (page % 8));
}

/*
 * Check whether a physical page is used.
 */
static int is_page_used(u32 page)
{
    return (page_bitmap[page / 8] &
            (1 << (page % 8))) != 0;
}

/*
 * Initialize the physical memory manager.
 *
 * For now we assume the first 1 MiB is reserved for:
 * - BIOS/reserved memory
 * - kernel
 * - boot structures
 *
 * Everything after 1 MiB is initially available.
 */
void pmm_init(void)
{
    /*
     * Initially mark every page as used.
     * This gives us a safe starting state.
     */
    for (u32 i = 0; i < BITMAP_SIZE; i++)
    {
        page_bitmap[i] = 0xFF;
    }

    used_pages = total_pages;

    /*
     * Make memory after the first 1 MiB available.
     */
    u32 reserved_pages = 0x400000 / PAGE_SIZE;

    for (u32 page = reserved_pages; page < total_pages; page++)
    {
        mark_page_free(page);
        used_pages--;
    }
}

/*
 * Allocate one 4 KiB physical page.
 */
void* pmm_alloc_page(void)
{
    for (u32 page = 0; page < total_pages; page++)
    {
        if (!is_page_used(page))
        {
            mark_page_used(page);
            used_pages++;

            return (void*)(page * PAGE_SIZE);
        }
    }

    return 0;
}

/*
 * Free a previously allocated physical page.
 */
void pmm_free_page(void* address)
{
    if (address == 0)
    {
        return;
    }

    u32 physical_address = (u32)address;

    if (physical_address % PAGE_SIZE != 0)
    {
        return;
    }

    u32 page = physical_address / PAGE_SIZE;

    if (page >= total_pages)
    {
        return;
    }

    if (is_page_used(page))
    {
        mark_page_free(page);
        used_pages--;
    }
}

u32 pmm_get_total_pages(void)
{
    return total_pages;
}

u32 pmm_get_used_pages(void)
{
    return used_pages;
}
