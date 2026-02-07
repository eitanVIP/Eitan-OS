void syscall(unsigned int num, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    asm volatile(
        "int $0x80"
        : // No output operands
        : "a"(num),
          "b"(arg1),
          "c"(arg2),
          "d"(arg3)
        : "memory"
    );
}

void print(char* str) {
    syscall(30, (unsigned int)str, 0, 0);
}

void clear_screen() {
    syscall(31, 0, 0, 0);
}

void* malloc(unsigned int size) {
    void* ptr;
    syscall(10, size, (unsigned int)&ptr, 0);
    return ptr;
}

void free(void* ptr) {
    syscall(11, (unsigned int)ptr, 0, 0);
}

void exit() {
    syscall(0, 0, 0, 0);
}

// Returns the PID of the new process via the pid_out pointer
void run_program(char* filename, unsigned int* pid_out) {
    syscall(1, (unsigned int)filename, (unsigned int)pid_out, 0);
}

void kill_process(unsigned int pid) {
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
unsigned short read_char() {
    unsigned short c;
    syscall(20, (unsigned int)&c, 0, 0);
    return c;
}
unsigned char is_character(unsigned short scancode) {
    if (scancode >= 256) return 0;

    char ascii = scancode_to_ascii[scancode];

    if (scancode >> 8 == 0xE0) return 0;

    return (ascii >= ' ' && ascii <= '~') ||
           (ascii == '\b') ||
           (ascii == '\t') ||
           (ascii == '\n');
}

// Reads file into a new heap buffer.
// Usage: char* buffer; uint32_t size; read_file("test.txt", &buffer, &size);
void read_file(char* filename, void** data_out, unsigned int* size_out) {
    syscall(40, (unsigned int)filename, (unsigned int)data_out, (unsigned int)size_out);
}

void write_file(char* filename, void* data, unsigned int size) {
    syscall(41, (unsigned int)filename, (unsigned int)data, size);
}

char** list_files(char* path, int* file_count) {
    char** files;
    syscall(42, (unsigned int)path, (unsigned int)&files, (unsigned int)file_count);
    return files;
}

char** list_dirs(char* path, int* dir_count) {
    char** dirs;
    syscall(43, (unsigned int)path, (unsigned int)&dirs, (unsigned int)dir_count);
    return dirs;
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

void* memcpy(void* dest, const void* src, const unsigned long long size) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (unsigned long long i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* dest, unsigned char val, unsigned long long size) {
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

char* str_concat(const char* s1, const char* s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    // Allocate space for both strings + null terminator
    char* result = malloc(len1 + len2 + 1);

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

unsigned char strncmp(const char* s1, const char* s2, int max_size) {
    int len = strlen(s1);
    len = len > max_size ? max_size : len;
    if (len > strlen(s2))
        return 0;

    for (int i = 0; i < len; i++) {
        if (s1[i] != s2[i])
            return 0;
    }

    return 1;
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



enum command {
    none,
    ls,
    echo,
    clear,
    cd
};

char working_dir[128] = "/";

void execute_command_line(enum command command, char** args, int args_size) {
    switch (command) {
        case ls:
            int file_count;
            char** files = list_files(working_dir, &file_count);
            for (int i = 0; i < file_count; i++) {
                int name_len = strlen(files[i]);
                char* msg = malloc(name_len + 2);
                memcpy(msg, files[i], name_len);
                msg[name_len] = '\n';
                msg[name_len + 1] = '\0';
                print(msg);
                free(msg);
                free(files[i]);
            }
            free(files);
            break;

        case echo:
            int msg_len = strlen(args[0]);
            char* msg = malloc((msg_len + 2) * sizeof(char));

            memcpy(msg, args[0], msg_len);
            msg[msg_len] = '\n';
            msg[msg_len + 1] = '\0';

            print(msg);

            free(msg);
            break;

        case clear:
            break;

        case cd:
            int dir_name_len = strlen(args[0]);
            if (dir_name_len >= 127) {
                memcpy(working_dir, args[0], 127);
                working_dir[127] = '\0';
            } else {
                memcpy(working_dir, args[0], dir_name_len);
                working_dir[dir_name_len + 1] = '\0';
            }
            break;

        default:
            break;
    }
}

char parse_command_line(const char* command_line, enum command* command_out, char*** args_out, int* args_size_out) {
    if (command_line[0] == '\0')
        return 0;

    int command_size = 0;
    while (command_line[command_size] != ' ' && command_line[command_size] != '\0') {
        if (command_size > 127)
            return 0;

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
    }

    unsigned int args_capacity = 10;
    char** args = malloc(args_capacity * sizeof(char*));
    unsigned int args_size = 0;

    int arg_start = command_size + 1;
    char reached_end = 0;
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
    return 1;
}

char* read_command_line() {
    unsigned short scancode = 0;
    char* command_line = malloc(128);
    memset(command_line, 0, 128);
    int counter = 0;

    while (scancode != 0x1C) { // Enter
        scancode = read_char();

        if (is_character(scancode)) {
            char ascii = scancode_to_ascii[scancode];
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
    print("Hello World From Shell\n");

    //    Command Parse Test
    // enum command command;
    // char** args;
    // int args_size;
    // parse_command_line("echo -al -bh -ng", &command, &args, &args_size);
    //
    // char msg[] = { command + '0', '\n', '\0' };
    // print(msg);
    //
    // for (int i = 0; i < args_size; i++) {
    //     char* arg_msg = malloc((strlen(args[i]) + 2) * sizeof(char));
    //     memcpy(arg_msg, args[i], strlen(args[i]));
    //
    //     arg_msg[strlen(args[i])] = '\n';
    //     arg_msg[strlen(args[i]) + 1] = '\0';
    //     print(arg_msg);
    // }

    while (1) {
        char* command_line = read_command_line();

        enum command command;
        char** args;
        int args_size;
        char parse_success = parse_command_line(command_line, &command, &args, &args_size);
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