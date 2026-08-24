#define null 0
#define true 1
#define false 0

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef uint64_t size_t;
typedef uint8_t bool_t;

#define FILE_NAME_LENGTH 60
typedef enum {
    FILE,
    DIRECTORY,
    ROOT_DIRECTORY,
} file_type_t;
typedef struct {
    file_type_t type;
    char name[FILE_NAME_LENGTH];
} dir_listing_t;

uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)  // rax has return value
        : "a"(num),  // rax is syscall num
          "b"(arg1), // rbx is arg1
          "c"(arg2), // rcx is arg2
          "d"(arg3)  // rdx is arg3
        : "memory"
    );

    return ret;
}

void print(char* str) {
    syscall(30, (uint64_t)str, 0, 0);
}

void clear_screen() {
    syscall(31, 0, 0, 0);
}

void* malloc(uint64_t size) {
    void* ptr;
    syscall(10, size, (uint64_t)&ptr, 0);
    return ptr;
}

void free(void* ptr) {
    syscall(11, (uint64_t)ptr, 0, 0);
}

void exit() {
    syscall(0, 0, 0, 0);
}

bool_t run_program(char* filename, uint32_t* pid_out) {
    return syscall(1, (uint64_t)filename, (uint64_t)pid_out, 0);
}

void kill_process(uint32_t pid) {
    syscall(2, pid, 0, 0);
}

char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, // Control
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ','0','.'
};

uint16_t read_keyboard() {
    uint16_t c = 0;
    while (c == 0) {
        syscall(20, (uint64_t)&c, 0, 0);
    }
    return c;
}

bool_t is_character(uint16_t scancode) {
    if (scancode >= 256) return 0;

    char ascii = scancode_to_ascii[scancode];

    if (scancode >> 8 == 0xE0) return 0;

    return (ascii >= ' ' && ascii <= '~') ||
           (ascii == '\b') ||
           (ascii == '\t') ||
           (ascii == '\n');
}

bool_t read_file(const char* path, void** data_out, size_t* size_out) {
    return syscall(40, (uint64_t)path, (uint64_t)data_out, (uint64_t)size_out);
}

bool_t write_file(const char* path, void* data, size_t size) {
    return syscall(41, (uint64_t)path, (uint64_t)data, size);
}

dir_listing_t* list_dir(const char* path, size_t* file_count) {
    dir_listing_t* files;
    syscall(42, (uint64_t)path, (uint64_t)&files, (uint64_t)file_count);
    return files;
}

bool_t delete_file(const char* path) {
    return syscall(43, (uint64_t)path, 0, 0);
}



double abs(double num) {
    if (num < 0)
        return -num;
    return num;
}

double pow(double base, int power) {
    if (power == 0)
        return 1;
    else if (power == 1)
        return base;
    else if (power == -1)
        return 1 / base;
    else if (power > 1) {
        double result = base;
        for (int i = 0; i < abs(power) - 1; i++)
            result *= base;
        return result;
    }
    else {
        double result = 1 / base;
        for (int i = 0; i < abs(power) - 1; i++)
            result /= base;
        return result;
    }
}

double max(double num1, double num2) {
    return num1 > num2 ? num1 : num2;
}

double min(double num1, double num2) {
    return num1 < num2 ? num1 : num2;
}

double clamp(double num, double min, double max) {
    return num > max ? max : num < min ? min : num;
}

double floor(double num) {
    return num >= 0 ? (int)num : ((num - (int)num) != 0 ? (int)num - 1 : num);
}

double ceil(double num) {
    double flo = floor(num);
    double frac = num - flo;
    return frac != 0 ? flo + 1 : num;
}

uint64_t ceil_div(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

double round(double num) {
    double flo = floor(num);
    double frac = num - flo;
    return frac >= 0.5 ? flo + 1 : flo;
}

static int rand_state = 183;
int rand() {
    long r = ((rand_state * 1103515245) + 12345);
    rand_state = r % 0xffffffff;
    return rand_state;
}



void* memcpy(void* dest, const void* src, const size_t size) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* dest, uint8_t val, size_t size) {
    unsigned char *d = dest;
    while (size--) {
        *d++ = val;
    }
    return dest;
}

int strlen(const char* str) {
    int count = 0;
    while (str[count] != '\0') {
        if (count > 1000)
            return -1;
        count++;
    }
    return count;
}

char* num_to_str(double num) {
     if (num == 0) {
         char* result = (char*)malloc(2);
         result[0] = '0';
         result[1] = '\0';
         return result;
     }

     int is_negative = num < 0;
     if (is_negative)
         num = -num;

     int fraction_digit_count = 0;
     double no_frac_num = abs(num);
     while ((long)no_frac_num != no_frac_num && fraction_digit_count < 6) {
         fraction_digit_count++;
         no_frac_num *= 10;
     }

     long int_num = (long)no_frac_num;

     int digit_count = 0;
     long tmp = int_num;
     do {
         digit_count++;
         tmp /= 10;
     } while (tmp > 0);

     //        digits,       '.'                          '0.'                                    '-'       '\0'
     int len = digit_count + (fraction_digit_count > 0) + (fraction_digit_count == digit_count) + is_negative + 1;
     char* result = malloc(len);

     result[len - 1] = '\0';
     if (is_negative)
         result[0] = '-';

     int i = -(is_negative) - (fraction_digit_count > 0) - (fraction_digit_count == digit_count);
     while (int_num != 0) {
         result[digit_count - i - 1] = (char)(int_num % 10 + '0');
         if (i + is_negative + (fraction_digit_count > 0) + (fraction_digit_count == digit_count) + 1 == fraction_digit_count) {
             i++;
             result[digit_count - i - 1] = '.';

             if (int_num / 10 == 0)
                 result[is_negative] = '0';

         }
         int_num /= 10;
         i++;
     }

     return result;
}

char* num_to_str_no_malloc(uint64_t num, char *buffer, size_t buffer_size) {
    // Handle the edge case of an empty or too-small buffer
    if (buffer == null || buffer_size < 2) return null;

    // Start filling from the end of the buffer (leaving room for '\0')
    char *ptr = &buffer[buffer_size - 1];
    *ptr = '\0';

    int is_negative = 0;

    // Handle 0 explicitly
    if (num == 0) {
        *(--ptr) = '0';
        return ptr;
    }

    // Handle negative numbers
    // Note: Using unsigned or long long avoids overflow issues with INT_MIN
    uint64_t n = num;
    if (n < 0) {
        is_negative = 1;
        n = -n;
    }

    // Extract digits backwards
    while (n > 0 && ptr > buffer) {
        *(--ptr) = (n % 10) + '0';
        n /= 10;
    }

    // Add negative sign if applicable and if space permits
    if (is_negative && ptr > buffer) {
        *(--ptr) = '-';
    }

    return ptr;
}

char* str_concat(const char* s1, const char* s2) {
     int len1 = strlen(s1);
     int len2 = strlen(s2);

     // Allocate space for both strings + null terminator
     char* result = malloc(len1 + len2 + 1);
     if (!result) return null; // handle malloc failure

     // Copy first string
     for (int i = 0; i < len1; i++)
         result[i] = s1[i];

     // Copy second string
     for (int i = 0; i < len2; i++)
         result[len1 + i] = s2[i];

     // Null terminate
     result[len1 + len2] = '\0';

     return result;
}

char* strdup(const char* str) {
     char* result = malloc(strlen(str) + 1);
     return memcpy(result, str, strlen(str) + 1);
}

char* str_concats(const char** strings, int count) {
    char* result = strdup(strings[0]);
    for (int i = 1; i < count; i++) {
        char* temp = str_concat(result, strings[i]);
        free(result);
        result = temp;
    }

    return result;
}

unsigned char strcmp(const char* s1, const char* s2) {
    int len = strlen(s1);
    if (len != strlen(s2))
        return 0;

    for (int i = 0; i < len; i++) {
        if (s1[i] != s2[i])
            return 0;
    }

    return 1;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        if (*s1 != *s2 || *s1 == '\0') {
            return false;
        }
        s1++;
        s2++;
        n--;
    }
    return true;
}

char* substr(const char* str, int start, int size) {
    char* new_str = (char*)malloc(size + 1);

    for (int i = 0; i < size; i++) {
        new_str[i] = str[start + i];

        if (str[start + i] == '\0') {
            break;
        }
    }

    new_str[size] = '\0';

    return new_str;
}

char* strchr(const char *s, int c) {
    char target = (char)c;

    while (*s != '\0') {
        if (*s == target) {
            return (char *)s;
        }
        s++;
    }

    if (target == '\0') {
        return (char *)s;
    }

    return null;
}

char* strrchr(const char *s, char c) {
    char* last = null;
    while (*s) {
        if (*s == c) last = (char *)s;
        s++;
    }
    return last;
}

char** str_split(const char* str, char delim, int* out_count) {
    int count = 0;
    const char *p = str;

    while (*p) {
        while (*p == delim) p++;
        if (!*p) break;
        while (*p && *p != delim) p++;
        count++;
    }

    char **tokens = malloc(count * sizeof(char *));
    p = str;
    int i = 0;

    while (*p) {
        while (*p == delim) p++;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != delim) p++;
        size_t len = p - start;

        tokens[i] = malloc(len + 1);
        memcpy(tokens[i], start, len);
        tokens[i][len] = '\0';
        i++;
    }

    *out_count = count;
    return tokens;
}



enum command {
    none,
    ls,
    echo,
    clear,
    cd,
    touch,
    cat,
    write,
    rm,
    man,
    find
};

char working_dir[128] = "/";
int working_dir_len = 1;

char* join_working_dir(char* path) {
    char* strs[] = { working_dir, "/", path };
    return str_concats(strs, sizeof(strs) / sizeof(strs[0]));
}

void parse_path(char* path) {
    int len = strlen(path);
    if (len <= 1) return; // Already "/" or empty

    char* stack[64];
    int top = 0;
    int i = 0;

    while (path[i] != '\0') {
        // Skip slashes to find the start of a folder name
        while (path[i] == '/') i++;
        if (path[i] == '\0') break;

        int start = i;
        while (path[i] != '/' && path[i] != '\0') i++;

        int part_len = i - start;
        char* part = substr(path, start, part_len);

        if (strcmp(part, "..")) {
            // Pop from stack if we can
            if (top > 0) {
                top--;
                free(stack[top]);
            }
            free(part);
        } else if (strcmp(part, ".")) {
            // Ignore current directory
            free(part);
        } else {
            // Push folder to stack (guard against overflow)
            if (top < 64) {
                stack[top++] = part;
            } else {
                free(part);
            }
        }
    }

    // Reconstruct the string
    memset(path, 0, len);
    path[0] = '/';
    path[1] = '\0';

    for (int j = 0; j < top; j++) {
        char* temp = str_concat(path, stack[j]);
        char* temp2 = (j < top - 1) ? str_concat(temp, "/") : temp;

        memcpy(path, temp2, strlen(temp2) + 1);

        free(temp);
        if (temp2 != temp) free(temp2);
        free(stack[j]);
    }
}

void cmd_ls(char** args, int args_size) {
    if (args_size != 0) {
        print("Usage: ls\n");
        return;
    }

    size_t file_count;
    dir_listing_t* files = list_dir(working_dir, &file_count);
    if (files == null) {
        print("No files found\n");
        return;
    }

    for (size_t i = 0; i < file_count; i++) {
        char* msg = str_concat(files[i].name, "\n");
        print(msg);
        free(msg);
    }

    free(files);
}

void cmd_echo(char** args, int args_size) {
    if (args_size != 1) {
        print("Usage: echo <string>\n");
        return;
    }
    int msg_len = strlen(args[0]);
    char* msg = malloc((msg_len + 2) * sizeof(char));
    memcpy(msg, args[0], msg_len);
    msg[msg_len] = '\n';
    msg[msg_len + 1] = '\0';
    print(msg);
    free(msg);
}

void cmd_cd(char** args, int args_size) {
    if (args_size != 1) {
        print("Usage: cd <path>\n");
        return;
    }

    char* path = join_working_dir(args[0]);
    parse_path(path);

    memcpy(working_dir, path, strlen(path) + 1);
    working_dir_len = strlen(working_dir);

    free(path);
}

void cmd_touch(char** args, int args_size) {
    if (args_size != 1) {
        print("Usage: touch <filename>\n");
        return;
    }

    char* path = join_working_dir(args[0]);
    parse_path(path);

    write_file(path, "", 1);
    print("Created: "); print(path); print("\n");

    free(path);
}

void cmd_cat(char** args, int args_size) {
    if (args_size != 1) {
        print("Usage: cat <filename>\n");
        return;
    }

    char* path = join_working_dir(args[0]);
    parse_path(path);

    char* data;
    size_t data_size;
    if (read_file(path, &data, &data_size)) {
        print(data);
        print("\n");
        free(data);
    } else {
        print("Error: File doesn't exist\n");
    }
    free(path);
}

void cmd_write(char** args, int args_size) {
    if (args_size != 2) {
        print("Usage: write <filename> <data>\n");
        return;
    }

    char* path = join_working_dir(args[0]);
    parse_path(path);

    char* file_data = 0;
    size_t file_data_size;
    bool_t file_exists = read_file(path, &file_data, &file_data_size);
    if (file_exists) {
        write_file(path, args[1], strlen(args[1]) + 1);
        free(file_data);
    } else {
        print("Error: File doesn't exist\n");
    }
    free(path);
}

void cmd_rm(char** args, int args_size) {
    if (args_size != 1) {
        print("Usage: rm <filename>\n");
        return;
    }

    char* path = join_working_dir(args[0]);
    parse_path(path);

    if (delete_file(path)) {
        print("Deleted: ");
        print(path);
        print("\n");
    } else {
        print("Error: File doesn't exist\n");
    }
    free(path);
}

void cmd_man(char** args, int args_size) {
    if (args_size != 0 && args_size != 1) {
        print("Usage: man or man <command>\n");
        return;
    }
    if (args_size == 0) {
        print("List of commands:\nls echo cd touch cat write rm man find\n");
    } else {
        if (strcmp(args[0], "ls")) print("ls - prints all files and folders inside of working dir\nUsage: ls\n");
        else if (strcmp(args[0], "echo")) print("echo - prints the provided text to the screen\nUsage: echo <string>\n");
        else if (strcmp(args[0], "cd")) print("cd - changes the current working directory\nUsage: cd <path>\n");
        else if (strcmp(args[0], "touch")) print("touch - creates a new empty file\nUsage: touch <filename>\n");
        else if (strcmp(args[0], "cat")) print("cat - displays the contents of a file\nUsage: cat <filename>\n");
        else if (strcmp(args[0], "write")) print("write - overwrites a file with provided text\nUsage: write <filename> <data>\n");
        else if (strcmp(args[0], "rm")) print("rm - removes a file from the system\nUsage: rm <filename>\n");
        else if (strcmp(args[0], "man")) print("man - displays manual pages for commands\nUsage: man or man <command>\n");
        else if (strcmp(args[0], "find")) print("find - prints all nested files and folders in working dir\nUsage: find or find <path>\n");
        else print("Error: Command not found in manual.\n");
    }
}

void recursive_find(const char* path, int recursion_level) {
    size_t file_count;
    dir_listing_t* files = list_dir(path, &file_count);
    if (files == null)
        return;

    for (int i = 0; i < file_count; i++) {
        for (int j = 0; j < recursion_level; j++) // Print tabs to indent files in dirs
            print("   ");

        print(files[i].name); print("\n"); // Print file name

        if (files[i].type == DIRECTORY) { // If the file is a dir, continue recursively
            char* strs[] = { path, "/", files[i].name };
            char* child_path = str_concats(strs, sizeof(strs) / sizeof(strs[0]));
            parse_path(child_path);

            recursive_find(child_path, recursion_level + 1);
            free(child_path);
        }
    }

    free(files);
}

void cmd_find(char** args, int args_size) {
    if (args_size == 0) {
        recursive_find(working_dir, 0);
    }
    else if (args_size == 1) {
        char* path = join_working_dir(args[0]);
        parse_path(path);
        recursive_find(path, 0);
    }
    else {
        print("Usage: find or find <path> \n");
    }
}

void execute_command_line(enum command command, char** args, int args_size) {
    switch (command) {
        case ls:
            cmd_ls(args, args_size);
            break;

        case echo:
            cmd_echo(args, args_size);
            break;

        case clear:
            clear_screen();
            break;

        case cd:
            cmd_cd(args, args_size);
            break;

        case touch:
            cmd_touch(args, args_size);
            break;

        case cat:
            cmd_cat(args, args_size);
            break;

        case write:
            cmd_write(args, args_size);
            break;

        case rm:
            cmd_rm(args, args_size);
            break;

        case man:
            cmd_man(args, args_size);
            break;

        case find:
            cmd_find(args, args_size);
            break;

        default:
            print("Error: Command not found\n");
            break;
    }
}

bool_t parse_command_line(const char* command_line, enum command* command_out, char*** args_out, int* args_size_out) {
    if (command_line[0] == '\0')
        return false;

    int command_size = 0;
    while (command_line[command_size] != ' ' && command_line[command_size] != '\0') {
        if (command_size > 127)
            return false;

        command_size++;
    }

    if (strncmp(command_line, "ls", command_size)) {
        *command_out = ls;
    } else if (strncmp(command_line, "echo", command_size)) {
        *command_out = echo;
    } else if (strncmp(command_line, "clear", command_size)) {
        *command_out = clear;
    } else if (strncmp(command_line, "cd", command_size)) {
        *command_out = cd;
    } else if (strncmp(command_line, "touch", command_size)) {
        *command_out = touch;
    } else if (strncmp(command_line, "cat", command_size)) {
        *command_out = cat;
    } else if (strncmp(command_line, "write", command_size)) {
        *command_out = write;
    } else if (strncmp(command_line, "rm", command_size)) {
        *command_out = rm;
    } else if (strncmp(command_line, "man", command_size)) {
        *command_out = man;
    } else if (strncmp(command_line, "find", command_size)) {
        *command_out = find;
    } else {
        *command_out = none;
    }

    unsigned int args_capacity = 10;
    char** args = malloc(args_capacity * sizeof(char*));
    unsigned int args_size = 0;

    int arg_start = command_size + 1;
    char reached_end = command_line[arg_start] == '\0' || arg_start > 127;
    while (!reached_end) {
        int arg_size = 0;
        while (command_line[arg_start + arg_size] != ' ') {
            if (command_line[arg_start + arg_size] == '\0' || arg_start + arg_size > 127) {
                reached_end = 1;
                break;
            }

            arg_size++;
        }

        if (args_size > args_capacity / 2) {
            args_capacity *= 2;
            char** new_args = malloc(args_capacity * sizeof(char*));

            for (int i = 0; i < args_size; i++)
                new_args[i] = args[i];

            free(args);
            args = new_args;
        }

        args[args_size++] = substr(command_line, arg_start, arg_size);

        arg_start += arg_size + 1;
    }

    *args_out = args;
    *args_size_out = args_size;
    return true;
}

char* read_command_line() {
    unsigned short scancode = 0;
    char* command_line = malloc(128);
    memset(command_line, 0, 128);
    int counter = 0;

    while (scancode != 0x1C) { // Enter
        scancode = read_keyboard();

        if (is_character(scancode)) {
            char ascii = scancode_to_ascii[scancode];

            if (ascii == '\b' && counter <= 0)
                continue;

            if (ascii >= 32 && ascii <= 126)
                command_line[counter++] = ascii;
            else if (ascii == '\b')
                command_line[--counter] = '\0';

            char msg[] = { ascii, '\0' };
            print(msg);

            if (counter >= 128)
                counter = 0;
        }
    }

    return command_line;
}

void main(void) {
    while (true) {
        char* line_prefix = str_concat(working_dir, "> ");
        print(line_prefix);
        free(line_prefix);

        char* command_line = read_command_line();

        enum command command;
        char** args;
        int args_size;
        bool_t parse_success = parse_command_line(command_line, &command, &args, &args_size);
        free(command_line);

        if (parse_success) {
            execute_command_line(command, args, args_size);

            for (int i = 0; i < args_size; i++) {
                free(args[i]);
            }
            free(args);
        }
    }
}