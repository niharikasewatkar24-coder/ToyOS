#include "paging.h"
#include "pmm.h"
#include "../drivers/serial_port/serial_port.h"

#define PAGE_ENTRIES 1024

#define PAGE_PRESENT 0x001
#define PAGE_WRITE   0x002
#define PAGE_USER    0x004

static u32 page_directory[PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));

static u32 first_page_table[PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));

static u32 second_page_table[PAGE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));

/* Create one page-table entry. */
static void set_page_entry(
    u32* table,
    u32 index,
    u32 address
)
{
    table[index] = address | PAGE_PRESENT | PAGE_WRITE;
}

/* Set up identity paging from 0 MB to 8 MB. */
static void setup_identity_paging(void)
{
    /* Map first 4 MB. */
    for (u32 i = 0; i < PAGE_ENTRIES; i++)
    {
        set_page_entry(
            first_page_table,
            i,
            i * PAGE_SIZE
        );
    }

    /* Map next 4 MB. */
    for (u32 i = 0; i < PAGE_ENTRIES; i++)
    {
        set_page_entry(
            second_page_table,
            i,
            (4 * 1024 * 1024) + (i * PAGE_SIZE)
        );
    }

    /* Page directory entries. */
    page_directory[0] =
        (u32)first_page_table | PAGE_PRESENT | PAGE_WRITE;

    page_directory[1] =
        (u32)second_page_table | PAGE_PRESENT | PAGE_WRITE;

    /* Clear remaining entries. */
    for (u32 i = 2; i < PAGE_ENTRIES; i++)
    {
        page_directory[i] = 0;
    }
}

/* Load page directory into CR3. */
static void load_page_directory(u32* directory)
{
    __asm__ volatile(
        "mov %0, %%cr3"
        :
        : "r"(directory)
        : "memory"
    );
}

/* Enable paging using CR0. */
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
 * Allow Ring 3 to access one selected identity-mapped page.
 * The caller must pass a page allocated for user memory.
 */
int paging_map_user_page(u32 address)
{
    /* Check page alignment. */
    if (address % PAGE_SIZE != 0)
        return 0;

    /* Only allow addresses from 4 MB to 8 MB. */
    if (address < 4 * 1024 * 1024 ||
        address >= 8 * 1024 * 1024)
        return 0;

    u32 index =
        (address - 4 * 1024 * 1024) / PAGE_SIZE;

    /* Mark selected page as user-accessible. */
    second_page_table[index] |= PAGE_USER;

    /* Allow user access through the directory entry. */
    page_directory[1] |= PAGE_USER;

    /* Refresh the TLB. */
    load_page_directory(page_directory);

    return 1;
}

/* Revoke Ring 3 access to a selected page. */
int paging_unmap_user_page(u32 address)
{
    if (address % PAGE_SIZE != 0)
        return 0;

    if (address < 4 * 1024 * 1024 ||
        address >= 8 * 1024 * 1024)
        return 0;

    u32 index =
        (address - 4 * 1024 * 1024) / PAGE_SIZE;

    if ((second_page_table[index] & PAGE_USER) == 0)
        return 0;

    second_page_table[index] &= ~PAGE_USER;

    load_page_directory(page_directory);

    return 1;
}
/* Check whether a page is user-accessible. */
int paging_is_user_page(u32 address)
{
    /* Check allowed memory range. */
    if (address < 4 * 1024 * 1024 ||
        address >= 8 * 1024 * 1024)
        return 0;

    /* Find page-table index. */
    u32 index =
        (address - 4 * 1024 * 1024) / PAGE_SIZE;

    /* Check the user-accessible bit. */
    return (second_page_table[index] & PAGE_USER) != 0;
}

/* Initialize paging. */
void paging_init(void)
{
    setup_identity_paging();

    load_page_directory(page_directory);

    enable_paging();
}

/* Page fault handler. */
void page_fault_handler(u32 error_code)
{
    (void)error_code;

    serial_log(LOG_ERROR, "PAGE FAULT");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}
