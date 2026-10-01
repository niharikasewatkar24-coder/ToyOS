void key_handler_two(struct keyboard_event event);

void clear() {
    for (int i = 0; i < SCREEN_WIDTH; i++) {
        for (int j = 0; j < SCREEN_HEIGHT; j++) {
            draw_char(i, j, ' ');
        }
    }
    symbols_written = 0;
    lines_written = 0;
    better_put_cursor(symbols_written, lines_written);
}

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
    print_message(cow_ascii_art);
}

void help()
{
    print_message("clear, help, sleep, list, cowsays;\ncreate (operand), delete (operand), read (operand), edit (operand)");
}

void sleep() {
    print_message("GOING TO SLEEP");
    screenSaverActive = true;
    saveFramebufferState();
    clear();
}

void init_files() {
    for (int i = 0; i < FILE_MANAGER_SIZE; ++i) {
        int j;
        for (j = 0; j < FILE_NAME_SIZE; ++j) {
            files[i].name[j] = '\0';
        }
        for (j = 0; j < FILE_CONTENT_SIZE; ++j) {
            files[i].content[j] = '\0';
        }
    }
}

int find_file_by_name(const char* filename) {
    for (int i = 0; i < FILE_MANAGER_SIZE; ++i) {
        if (areStringsEqual(files[i].name, filename)) {
            return i;
        }
    }
    return -1;
}

void print_files_names() {
    for (int i = 0; i < FILE_MANAGER_SIZE; ++i) {
        if (files[i].name[0] != '\0') {
            print_text(" -- ");
            print_message(files[i].name);
        }
    }
}

void create_file(const char* filename) {
    int exists_index = find_file_by_name(filename);
    if (exists_index != -1) {
        print_message("File already exists");
        return;
    }
    int empty_index = find_file_by_name("\0");
    if (empty_index == -1) {
        print_message("No empty space");
        return;
    }
    int j = 0;
    while (filename[j] != '\0') {
        files[empty_index].name[j] = filename[j];
        j++;
    }
    files[empty_index].content[0] = '\0';
    print_message("File created");
}

void delete_file(const char* filename) {
    int file_index = find_file_by_name(filename);
    if (file_index != -1) {
        for (int j = 0; j < FILE_NAME_SIZE; ++j) {
            files[file_index].name[j] = '\0';
        }
        for (int j = 0; j < FILE_CONTENT_SIZE; ++j) {
            files[file_index].content[j] = '\0';
        }
        print_message("File has been successfully deleted");
    }
    else {
        print_message("File not found");
    }
}

void print_file_content(const char* filename) {
    int file_index = find_file_by_name(filename);
    if (file_index != -1) {
        if (files[file_index].content[0] != '\0')
            print_message(files[file_index].content);
        else {
            print_message("File is empty");
        }
    }
    else {
        print_message("File not found");
    }
}

void edit(const char* filename) {
    index_of_file = find_file_by_name(filename);
    if (index_of_file != -1) {
        saveFramebufferState();
        clear();
        keyboard_set_handler(key_handler_two);
        for (int j = 0; j < FILE_CONTENT_SIZE; ++j) {
            files[index_of_file].content[j] = '\0';
        }
        file_index = 0;
    }
    else {
        index_of_file = -1;
        print_message("File not found");
    }
}

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
