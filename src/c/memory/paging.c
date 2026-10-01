#include "paging.h"
#include "pmm.h"
#include "../drivers/serial_port/serial_port.h"

#define PAGE_ENTRIES 1024

#define PAGE_PRESENT 0x001
#define PAGE_WRITE   0x002
#define PAGE_USER    0x004

/*
 * Page directory.
 *
 * Each entry points to a page table.
 * It must be aligned to 4 KiB.
 */
static u32 page_directory[PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));

/*
 * Page table 0:
 * Maps physical memory from 0 - 4 MiB.
 */
static u32 first_page_table[PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));

/*
 * Page table 1:
 * Maps physical memory from 4 - 8 MiB.
 */
static u32 second_page_table[PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));


/*
 * Create one page-table entry.
 */
static void set_page_entry(
    u32* table,
    u32 index,
    u32 address
)
{
    table[index] =
        address
        | PAGE_PRESENT
        | PAGE_WRITE;
}


/*
 * Set up identity paging.
 *
 * Page table 0:
 *   Virtual 0 - 4 MiB
 *       ->
 *   Physical 0 - 4 MiB
 *
 * Page table 1:
 *   Virtual 4 - 8 MiB
 *       ->
 *   Physical 4 - 8 MiB
 */
static void setup_identity_paging(void)
{
    /*
     * Page table 0:
     * Identity-map physical addresses 0 - 4 MiB.
     */
    for (u32 i = 0; i < PAGE_ENTRIES; i++)
    {
        set_page_entry(
            first_page_table,
            i,
            i * PAGE_SIZE
        );
    }


    /*
     * Page table 1:
     * Identity-map physical addresses 4 - 8 MiB.
     */
    for (u32 i = 0; i < PAGE_ENTRIES; i++)
    {
        set_page_entry(
            second_page_table,
            i,
            (4 * 1024 * 1024) + (i * PAGE_SIZE)
        );
    }


    /*
     * Page directory entry 0:
     *
     * Maps virtual addresses:
     * 0 - 4 MiB
     */
    page_directory[0] =
        ((u32)first_page_table)
        | PAGE_PRESENT
        | PAGE_WRITE;


    /*
     * Page directory entry 1:
     *
     * Maps virtual addresses:
     * 4 - 8 MiB
     */
    page_directory[1] =
        ((u32)second_page_table)
        | PAGE_PRESENT
        | PAGE_WRITE;


    /*
     * Clear the remaining page-directory entries.
     */
    for (u32 i = 2; i < PAGE_ENTRIES; i++)
    {
        page_directory[i] = 0;
    }
}


/*
 * Load the page directory into CR3.
 */
static void load_page_directory(u32* directory)
{
    __asm__ volatile(
        "mov %0, %%cr3"
        :
        : "r"(directory)
        : "memory"
    );
}


/*
 * Enable paging through CR0.
 */
static void enable_paging(void)
{
    u32 cr0;

    __asm__ volatile(
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    cr0 |= 0x80000000;

    __asm__ volatile(
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );
}


/*
 * Initialize paging.
 */
void paging_init(void)
{
    setup_identity_paging();

    load_page_directory(page_directory);

    enable_paging();
}


/*
 * Page fault handler.
 */
void page_fault_handler(u32 error_code)
{
    (void)error_code;

    serial_log(LOG_ERROR, "PAGE FAULT");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}
