#include "kernel/kernel.h"

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2)

#define MAX_COMMAND_LENGTH 80
#define MAX_HISTORY_SIZE 10
#define FILE_MANAGER_SIZE 10
#define FILE_NAME_SIZE 80
#define FILE_CONTENT_SIZE 2000

struct Command {
    const char* name;
    void (*function)(const char*);
    bool requires_argument;
};

struct File {
    char name[MAX_COMMAND_LENGTH];
    char content[SCREEN_HEIGHT * SCREEN_WIDTH];
};

unsigned int symbols_written = 0;
unsigned int lines_written = 0;

bool screenSaverActive;

unsigned int savedSymbols_written = 0;
unsigned int savedLines_written = 0;

struct File files[MAX_HISTORY_SIZE];
int index_of_file = -1;
int file_index = 0;

void split_string(char* str, char* command, char* argument) {
    int i = 0;
    int j = 0;
    command[0] = '\0';
    argument[0] = '\0';

    while (str[i] != ' ' && str[i] != '\0') {
        command[i] = str[i];
        i++;
    }
    command[i] = '\0';


    if (str[i] == ' ') {
        i++;
        while (str[i] != '\0') {
            argument[j] = str[i];
            i++;
            j++;
        }
        argument[j] = '\0';
    }
}

int areStringsEqual(const char* str1, const char* str2) {
    int i = 0;

    while (str1[i] != '\0' || str2[i] != '\0') {
        if (str1[i] != str2[i]) {
            return 0;
        }
        i++;
    }

    if (str1[i] != str2[i]) {
        return 0;
    }

    return 1;
}