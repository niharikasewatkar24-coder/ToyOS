
#include "drivers/ide/ide.h"

#define FS_MAGIC 0x544F5946
#define FS_VERSION 1
#define FS_HEADER_SECTOR 1
#define FS_DATA_START 2
#define FS_DATA_SECTORS 41

struct FSHeader
{
    u32 magic;
    u32 version;
};

/* Forward declaration */
void key_handler_two(struct keyboard_event event);

/* Filesystem save */
int filesystem_save(void)
{
    u16 buffer[256];
    const char *source = (const char *)files;

    int total = sizeof(files);
    int position = 0;

    /* Write file data first */
    for (int sector = 0; sector < FS_DATA_SECTORS; sector++)
    {
        for (int i = 0; i < 256; i++)
            buffer[i] = 0;

        for (int i = 0; i < 512 && position < total; i++)
            ((char *)buffer)[i] = source[position++];

        if (ide_write_sector(FS_DATA_START + sector, buffer) != 0)
            return -1;
    }

    /* Write header only after data succeeds */
    struct FSHeader header;
    header.magic = FS_MAGIC;
    header.version = FS_VERSION;

    for (int i = 0; i < 256; i++)
        buffer[i] = 0;

    for (int i = 0; i < (int)sizeof(header); i++)
        ((char *)buffer)[i] = ((char *)&header)[i];

    if (ide_write_sector(FS_HEADER_SECTOR, buffer) != 0)
        return -1;

    return 0;
}

/* Filesystem load */
int filesystem_load(void)
{
    u16 buffer[256];
    struct FSHeader header;

    if (ide_read_sector(FS_HEADER_SECTOR, buffer) != 0)
        return -1;

    for (int i = 0; i < (int)sizeof(header); i++)
        ((char *)&header)[i] = ((char *)buffer)[i];

    if (header.magic != FS_MAGIC || header.version != FS_VERSION)
        return -1;

    char *destination = (char *)files;

    int total = sizeof(files);
    int position = 0;

    for (int sector = 0; sector < FS_DATA_SECTORS; sector++)
    {
        if (ide_read_sector(FS_DATA_START + sector, buffer) != 0)
            return -1;

        for (int i = 0; i < 512 && position < total; i++)
            destination[position++] = ((char *)buffer)[i];
    }

    return 0;
}

/* Clear file memory */
void clear_files_memory(void)
{
    for (int i = 0; i < FILE_MANAGER_SIZE; i++)
    {
        for (int j = 0; j < FILE_NAME_SIZE; j++)
            files[i].name[j] = '\0';

        for (int j = 0; j < FILE_CONTENT_SIZE; j++)
            files[i].content[j] = '\0';
    }
}

/* Initialize filesystem */
void init_files(void)
{
    clear_files_memory();

    if (filesystem_load() == 0)
    {
        serial_log(LOG_INFO, "FILESYSTEM LOADED FROM DISK");
    }
    else
    {
        clear_files_memory();
        serial_log(LOG_INFO, "NEW FILESYSTEM INITIALIZED");
    }
}

/* Clear screen */
void clear()
{
    for (int i = 0; i < SCREEN_WIDTH; i++)
    {
        for (int j = 0; j < SCREEN_HEIGHT; j++)
            draw_char(i, j, ' ');
    }

    symbols_written = 0;
    lines_written = 0;

    better_put_cursor(symbols_written, lines_written);
}

/* Cow says */
void cowsays(const char* message)
{
    const char* cow_ascii_art =
        " ----- \n"
        "        \\   ^__^\n"
        "         \\  (oo)\\_______\n"
        "            (__)\\       )\\/\\\n"
        "                ||----w |\n"
        "                ||     ||\n";

    print_text(" _____ \n< ");
    print_text(message);
    print_text(" >\n");
    print_message((char *)cow_ascii_art);
}

/* Help */
void help()
{
    print_message("clear, help, sleep, list, cowsays;");
    print_message("create (operand), delete (operand), read (operand), edit (operand)");
}

/* Sleep */
void sleep()
{
    print_message("GOING TO SLEEP");

    screenSaverActive = true;
    saveFramebufferState();
    clear();
}

/* Find file */
int find_file_by_name(const char* filename)
{
    for (int i = 0; i < FILE_MANAGER_SIZE; i++)
    {
        if (areStringsEqual(files[i].name, filename))
            return i;
    }

    return -1;
}

/* List files */
void print_files_names()
{
    for (int i = 0; i < FILE_MANAGER_SIZE; i++)
    {
        if (files[i].name[0] != '\0')
        {
            print_text(" -- ");
            print_message(files[i].name);
        }
    }
}

/* Create file */
void create_file(const char* filename)
{
    int length = 0;

    while (filename[length] != '\0')
        length++;

    if (length == 0 || length >= FILE_NAME_SIZE)
    {
        print_message("Invalid filename");
        return;
    }

    if (find_file_by_name(filename) != -1)
    {
        print_message("File already exists");
        return;
    }

    int empty_index = find_file_by_name("");

    if (empty_index == -1)
    {
        print_message("No empty space");
        return;
    }

    for (int j = 0; j <= length; j++)
        files[empty_index].name[j] = filename[j];

    files[empty_index].content[0] = '\0';

    if (filesystem_save() != 0)
    {
        print_message("Disk save failed");
        return;
    }

    print_message("File created and saved");
}

/* Delete file */
void delete_file(const char* filename)
{
    int file_index = find_file_by_name(filename);

    if (file_index != -1)
    {
        for (int j = 0; j < FILE_NAME_SIZE; j++)
            files[file_index].name[j] = '\0';

        for (int j = 0; j < FILE_CONTENT_SIZE; j++)
            files[file_index].content[j] = '\0';

        if (filesystem_save() != 0)
        {
            print_message("Disk save failed");
            return;
        }

        print_message("File deleted and saved");
    }
    else
    {
        print_message("File not found");
    }
}

/* Read file */
void print_file_content(const char* filename)
{
    int file_index = find_file_by_name(filename);

    if (file_index != -1)
    {
        if (files[file_index].content[0] != '\0')
            print_message(files[file_index].content);
        else
            print_message("File is empty");
    }
    else
    {
        print_message("File not found");
    }
}

/* Edit file */
void edit(const char* filename)
{
    index_of_file = find_file_by_name(filename);

    if (index_of_file != -1)
    {
        saveFramebufferState();
        clear();

        file_index = 0;

        while (file_index < FILE_CONTENT_SIZE - 1 &&
               files[index_of_file].content[file_index] != '\0')
        {
            print_char(files[index_of_file].content[file_index]);
            file_index++;
        }

        keyboard_set_handler(key_handler_two);
    }
    else
    {
        index_of_file = -1;
        print_message("File not found");
    }
}
/* Command table */
struct Command command_table[] = {
    {"clear", clear, false},
    {"cowsays", cowsays, true},
    {"help", help, false},
    {"sleep", sleep, false},
    {"list", print_files_names, false},
    {"create", create_file, true},
    {"delete", delete_file, true},
    {"read", print_file_content, true},
    {"edit", edit, true}
};
