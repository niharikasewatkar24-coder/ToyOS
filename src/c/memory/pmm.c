
#include "pmm.h"

#define BITMAP_SIZE (PMM_MAX_PAGES / 8)
#define RESERVED_LIMIT (4 * 1024 * 1024)

typedef unsigned long long u64;

extern u32 multiboot_info_ptr;

/* Multiboot v1 information structure */
struct multiboot_info
{
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} __attribute__((packed));

/* Multiboot v1 memory-map entry */
struct mmap_entry
{
    u32 size;
    u32 base_low;
    u32 base_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} __attribute__((packed));

static unsigned char page_bitmap[BITMAP_SIZE];
static unsigned char usable_bitmap[BITMAP_SIZE];

static u32 total_pages = PMM_MAPPED_LIMIT / PAGE_SIZE;
static u32 used_pages = 0;

/* Mark a physical page as used. */
static void mark_page_used(u32 page)
{
    page_bitmap[page / 8] |= (1 << (page % 8));
}

/* Mark a physical page as free. */
static void mark_page_free(u32 page)
{
    page_bitmap[page / 8] &= ~(1 << (page % 8));
}

/* Check whether a page is used. */
static int is_page_used(u32 page)
{
    return (page_bitmap[page / 8] &
            (1 << (page % 8))) != 0;
}

/* Mark a page as usable according to the memory map. */
static void mark_page_usable(u32 page)
{
    usable_bitmap[page / 8] |= (1 << (page % 8));
}

/* Check whether a page is usable. */
static int is_page_usable(u32 page)
{
    return (usable_bitmap[page / 8] &
            (1 << (page % 8))) != 0;
}

/*
 * Initialize the physical memory manager.
 * Only usable pages between 4 MB and 8 MB are made available.
 */
void pmm_init(void)
{
    /* Initially mark all pages as used and unavailable. */
    for (u32 i = 0; i < BITMAP_SIZE; i++)
    {
        page_bitmap[i] = 0xFF;
        usable_bitmap[i] = 0;
    }

    used_pages = total_pages;

    /* Get Multiboot information address saved by boot.asm. */
    u32 info_address = multiboot_info_ptr;

    if (info_address == 0)
        return;

    struct multiboot_info *info =
        (struct multiboot_info *)info_address;

    /* Check whether the memory-map information is valid. */
    if ((info->flags & (1 << 6)) == 0)
        return;

    if (info->mmap_addr == 0 || info->mmap_length == 0)
        return;

    u32 mmap_start = info->mmap_addr;
    u32 mmap_end = mmap_start + info->mmap_length;

    /* Check address overflow. */
    if (mmap_end < mmap_start)
        return;

    u32 current = mmap_start;

    while (current < mmap_end)
    {
        if (mmap_end - current < 4)
            break;

        struct mmap_entry *entry =
            (struct mmap_entry *)current;

        if (entry->size < 20 ||
            entry->size > mmap_end - current - 4)
            break;

        /* Type 1 indicates usable RAM. */
        if (entry->type == 1)
        {
            u64 base =
                ((u64)entry->base_high << 32) |
                entry->base_low;

            u64 length =
                ((u64)entry->length_high << 32) |
                entry->length_low;

            u64 region_end = base + length;

            if (region_end >= base &&
                base < PMM_MAPPED_LIMIT &&
                region_end > RESERVED_LIMIT)
            {
                /* Restrict the region to 4 MB - 8 MB. */
                if (base < RESERVED_LIMIT)
                    base = RESERVED_LIMIT;

                if (region_end > PMM_MAPPED_LIMIT)
                    region_end = PMM_MAPPED_LIMIT;

                /* Calculate completely available pages. */
                u32 first_page =
                    (u32)((base + PAGE_SIZE - 1) / PAGE_SIZE);

                u32 last_page =
                    (u32)(region_end / PAGE_SIZE);

                for (u32 page = first_page;
                     page < last_page && page < total_pages;
                     page++)
                {
                    if (!is_page_usable(page))
                    {
                        mark_page_usable(page);
                        mark_page_free(page);
                        used_pages--;
                    }
                }
            }
        }

        /* Move to the next variable-sized memory-map entry. */
        current += entry->size + 4;
    }
}

/* Allocate one 4 KB physical page. */
void* pmm_alloc_page(void)
{
    for (u32 page = 0; page < total_pages; page++)
    {
        if (is_page_usable(page) && !is_page_used(page))
        {
            mark_page_used(page);
            used_pages++;

            return (void *)(page * PAGE_SIZE);
        }
    }

    return 0;
}

/* Free a previously allocated physical page. */
void pmm_free_page(void* address)
{
    if (address == 0)
        return;

    u32 physical_address = (u32)address;

    if (physical_address % PAGE_SIZE != 0)
        return;

    u32 page = physical_address / PAGE_SIZE;

    if (page >= total_pages)
        return;

    /* Do not free reserved or unavailable memory. */
    if (!is_page_usable(page))
        return;

    if (is_page_used(page))
    {
        mark_page_free(page);
        used_pages--;
    }
}

/* Return total managed pages. */
u32 pmm_get_total_pages(void)
{
    return total_pages;
}

/* Return currently used pages. */
u32 pmm_get_used_pages(void)
{
    return used_pages;
}
