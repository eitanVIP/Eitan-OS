//
// Created by eitan on 1/20/26.
//

#include "filesystem.h"

#include "screen.h"
#include "memory/allocator.h"
#include "util/util.h"
#include "util/io.h"
#include "util/string.h"

// ATA protocol IO ports
#define ATA_DATA           0x1F0
#define ATA_ERROR          0x1F1
#define ATA_SECTORS        0x1F2
#define ATA_LBA_LOW        0x1F3 // Low 8 bits of LBA (index of sector)
#define ATA_LBA_MID        0x1F4 // Mid 8 bits of LBA (index of sector)
#define ATA_LBA_HIGH       0x1F5 // High 8 bits of LBA (index of sector)
#define ATA_DRIVE_SELECT   0x1F6
#define ATA_COMMAND_STATUS 0x1F7

// ATA port 0x1F7 (command/status) status bits
#define ATA_STATUS_BUSY                0x80
#define ATA_STATUS_READY               0x40
#define ATA_STATUS_WRITE_FAULT         0x20
#define ATA_STATUS_SEEK_COMPLETE       0x10
#define ATA_STATUS_DATA_REQUEST_READY  0x08
#define ATA_STATUS_ERR                 0x01

// ATA drive select options
#define ATA_DRIVE_SELECT_MASTER 0b10100000
#define ATA_DRIVE_SELECT_SLAVE  0b10110000
#define ATA_DRIVE_SELECT_MASTER_LBA 0b11100000
#define ATA_DRIVE_SELECT_SLAVE_LBA  0b11110000

#define MAGIC_NUMBER 0xE17A9055
#define SECTOR_SIZE 512
#define FILE_TABLE_SECTORS 20
#define ENTRIES_PER_SECTOR (SECTOR_SIZE / sizeof(file_entry_t))
#define FILE_TABLE_ENTRIES (ENTRIES_PER_SECTOR * FILE_TABLE_SECTORS)
#define FILE_TABLE_SIZE (FILE_TABLE_SECTORS * SECTOR_SIZE)
#define MAX_FILES FILE_TABLE_ENTRIES

typedef struct {
    uint32_t magic_number;
    uint16_t version;
    uint32_t sectors;
    uint32_t file_table_start;
    uint32_t file_table_size;
    uint16_t block_size;
} __attribute__((packed)) superblock_t;

typedef struct {
    char name[60];
    uint32_t file_table_index;
} __attribute__((packed)) dir_entry_t;

static char model[41];
static uint16_t sector_count;
static uint8_t writable_drive;
static file_entry_t* file_table;

static void wait_busy() {
    while (io_inb(ATA_COMMAND_STATUS) & ATA_STATUS_BUSY);
}

static void wait_data_request_ready() {
    while (!(io_inb(ATA_COMMAND_STATUS) & ATA_STATUS_DATA_REQUEST_READY));
}

static void identify_drive() {
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_SELECT_SLAVE);
    io_outb(ATA_SECTORS, 0);
    io_outb(ATA_LBA_LOW, 0);
    io_outb(ATA_LBA_MID, 0);
    io_outb(ATA_LBA_HIGH, 0);
    io_outb(ATA_COMMAND_STATUS, 0xEC); // Command 0xEC: IDENTIFY
    
    uint8_t status = io_inb(ATA_COMMAND_STATUS);
    if (status == 0) {
        screen_print("Drive does not exist");
        return;
    }
    
    wait_busy();
    
    // Check if the drive is ATA or something else (like ATAPI/CD-ROM)
    writable_drive = 1;
    if (io_inb(ATA_LBA_MID) != 0 || io_inb(ATA_LBA_HIGH) != 0) {
        writable_drive = 0;
        screen_print("Not a writable drive");
        return;
    }
    
    wait_data_request_ready();
    
    uint16_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = io_inw(ATA_DATA);
    }
    
    // Disk model
    for (int i = 0; i < 20; i++) {
        model[i * 2] = data[27 + i] >> 8;
        model[i * 2 + 1] = data[27 + i] & 0xFF;
    }
    model[40] = '\0';
    char* strs[] = { "The disk model connected: ", model, "\n" };
    char* msg = str_concats(strs, 3);
    screen_print(msg);
    free(msg);
    
    // Disk number of sectors
    memcpy(&sector_count, &data[60], 2);
    char* sector_count_str = num_to_str(sector_count);
    char* size_str = num_to_str((double)sector_count * SECTOR_SIZE);
    char* strs2[] = { "Sector count: ", sector_count_str, ", Disk size: ", size_str, "\n" };
    msg = str_concats(strs2, 5);
    screen_print(msg);
    free(msg);
    free(sector_count_str);
    free(size_str);
}

void read_sectors(uint32_t lba, void* target, size_t size) {
    const uint8_t count = ceil((double)size / SECTOR_SIZE);

    if (count < 1 || lba + count >= sector_count)
        return;

    wait_busy();

    // Set up the Drive/Head Register
    // 1, Use LBA, 1, Master, LBA
    // 1  1        1  0       0000
    // LBA is 28 bits, I put here the last 4: 24-27
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_SELECT_SLAVE_LBA | ((lba >> 24) & 0b1111));

    io_outb(ATA_SECTORS, count);                       // Sector count
    io_outb(ATA_LBA_LOW, (uint8_t)lba);          // LBA Low (bits 0-7)
    io_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));   // LBA Mid (bits 8-15)
    io_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16)); // LBA High (bits 16-23)

    io_outb(ATA_COMMAND_STATUS, 0x20); //0x20 READ command

    uint16_t* t = target;
    for (int j = 0; j < count; j++) {
        wait_busy();
        wait_data_request_ready();

        for (int i = 0; i < 256; i++) {
            int word_idx = j * 256 + i;
            int byte_idx = word_idx * 2;

            uint16_t data = io_inw(ATA_DATA);
            if (byte_idx < size) {
                if (byte_idx + 1 >= size)
                    t[word_idx] = data & 0xFF;
                else
                    t[word_idx] = data;
            }
        }
    }
}

void write_sectors(uint32_t lba, const void* src, size_t size) {
    const uint8_t count = ceil((double)size / SECTOR_SIZE);

    if (!writable_drive || count < 1 || lba + count >= sector_count)
        return;

    wait_busy();

    // Set up the Drive/Head Register
    // 1, Use LBA, 1, Master, LBA
    // 1  1        1  0       0000
    // LBA is 28 bits, I put here the last 4: 24-27
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_SELECT_SLAVE_LBA | ((lba >> 24) & 0b1111));

    io_outb(ATA_SECTORS, count);                       // Sector count
    io_outb(ATA_LBA_LOW, (uint8_t)lba);          // LBA Low (bits 0-7)
    io_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));   // LBA Mid (bits 8-15)
    io_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16)); // LBA High (bits 16-23)

    io_outb(ATA_COMMAND_STATUS, 0x30); //0x30 WRITE command

    uint16_t* s = src;
    for (int j = 0; j < count; j++) {
        wait_busy();
        wait_data_request_ready();

        for (int i = 0; i < 256; i++) {
            int word_idx = j * 256 + i;
            int byte_idx = word_idx * 2;

            if (byte_idx < size) {
                if (byte_idx + 1 >= size) {
                    uint16_t data = ((uint8_t*)s)[byte_idx];
                    io_outw(ATA_DATA, data);
                }
                else {
                    uint16_t data = s[word_idx];
                    io_outw(ATA_DATA, data);
                }
            } else {
                io_outw(ATA_DATA, 0);
            }
        }
    }
}

static void init_superblock() {
    superblock_t superblock = {
        MAGIC_NUMBER,
        1,
        sector_count,
        1,
        FILE_TABLE_SECTORS,
        SECTOR_SIZE
    };
    write_sectors(0, &superblock, sizeof(superblock));
}

void load_file_table() {
    file_table = malloc(FILE_TABLE_SIZE);
    read_sectors(1, file_table, FILE_TABLE_SIZE);
}

void free_file_table() {
    free(file_table);
    file_table = 0;
}

bool_t find_root(uint32_t* file_index_ptr) {
    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        file_entry_t file_entry = file_table[i];

        if (file_entry.magic_number != MAGIC_NUMBER)
            continue;

        if (file_entry.type == ROOT_DIRECTORY) {
            *file_index_ptr = i;
            return true;
        }
    }

    return false;
}

// Checks whether root dir exists, if not creates one
void ensure_root() {
    load_file_table();

    uint32_t _;
    if (find_root(&_))
        return;

    memset(file_table, 0, FILE_TABLE_SIZE);
    file_table[0] = (file_entry_t){ MAGIC_NUMBER, ROOT_DIRECTORY, 1 + FILE_TABLE_SECTORS, 0 };

    write_sectors(1, file_table, FILE_TABLE_SIZE);
    free_file_table();
}

void filesystem_init(void) {
    identify_drive();
    init_superblock();
    ensure_root();
}

bool_t resolve_path(const char* path, uint32_t* file_index_ptr) {
    if (strlen(path) == 0)
        return false;

    if (path[0] != '/')
        return false;

    int path_parts_count;
    char** path_parts = str_split(path, '/', &path_parts_count);

    if (path_parts_count == 0)
        return false;

    uint32_t root_index;
    if (!find_root(&root_index))
        return false;

    uint32_t current_index = root_index;
    for (int i = 0; i < path_parts_count; i++) {
        bool_t is_last_part = i == path_parts_count - 1;

        file_entry_t current_dir_file = file_table[current_index];

        if (current_dir_file.size == 0)
            return false;

        dir_entry_t* dir_entries = malloc(current_dir_file.size);
        uint32_t dir_entries_count = current_dir_file.size / sizeof(dir_entry_t);
        read_sectors(current_dir_file.start_sector, dir_entries, current_dir_file.size);

        bool_t found = false;
        for (int j = 0; j < dir_entries_count; j++) {
            if (!strcmp(dir_entries[j].name, path_parts[i]))
                continue;

            uint32_t file_index = dir_entries[j].file_table_index;

            if (file_table[file_index].magic_number != MAGIC_NUMBER)
                continue;

            if (!(file_table[file_index].type == DIRECTORY || is_last_part))
                continue;

            current_index = file_index;
            found = true;
            break;
        }

        free(dir_entries);

        if (!found)
            return false;
    }

    *file_index_ptr = current_index;
    return true;
}

bool_t filesystem_read_file(const char* path, uint8_t** data_out, uint32_t* data_size_out, file_entry_t* file_entry_out) {
    load_file_table();

    uint32_t file_index;
    if (!resolve_path(path, &file_index)) {
        free_file_table();
        return false;
    }

    file_entry_t file_entry = file_table[file_index];
    if (file_entry.magic_number != MAGIC_NUMBER) {
        free_file_table();
        return false;
    }

    uint8_t* data = malloc(file_entry.size);
    read_sectors(file_entry.start_sector, data, file_entry.size);
    *data_out = data;
    *data_size_out = file_entry.size;
    *file_entry_out = file_entry;

    free_file_table();
    return true;
}

bool_t find_space_for_file(size_t file_size, uint32_t *file_start_sector) {
    int order[FILE_TABLE_ENTRIES];
    int n = 0;

    // Collect valid entries - don't break on the first hole, skip it.
    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        if (file_table[i].magic_number == MAGIC_NUMBER) {
            order[n++] = i;
        }
    }

    // Sort indices by start_sector - this is disk order, independent of table index.
    for (int i = 1; i < n; i++) {
        int key = order[i];
        uint32_t key_sector = file_table[key].start_sector;
        int j = i - 1;
        while (j >= 0 && file_table[order[j]].start_sector > key_sector) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    uint32_t sectors_needed = (file_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint32_t prev_end = FILE_TABLE_ENTRIES + 1;

    for (int k = 0; k < n; k++) {
        file_entry_t *entry = &file_table[order[k]];
        uint32_t gap = entry->start_sector - prev_end;
        if (gap >= sectors_needed) {
            *file_start_sector = prev_end;
            return true;
        }
        prev_end = entry->start_sector + (entry->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    }

    uint32_t trailing_gap = sector_count - 1 - prev_end;
    if (trailing_gap >= sectors_needed) {
        *file_start_sector = prev_end;
        return true;
    }

    return false;
}

void shift_file_entries_forward(file_entry_t* file_table, uint8_t idx_start) {
    for (uint32_t i = FILE_TABLE_ENTRIES - 1; i > idx_start; i--) {
        file_table[i] = file_table[i - 1];
    }

    file_table[idx_start] = (file_entry_t){};

    write_sectors(1, file_table, FILE_TABLE_SIZE);
}

void shift_file_entries_backward(file_entry_t* file_table, uint8_t idx_start) {
    for (uint32_t i = max(idx_start - 1, 0); i < FILE_TABLE_ENTRIES - 1; i++) {
        file_table[i] = file_table[i + 1];
    }

    file_table[FILE_TABLE_ENTRIES - 1] = (file_entry_t){};

    write_sectors(1, file_table, FILE_TABLE_SIZE);
}

bool_t add_file_to_dir(uint32_t dir_file_entry_idx, uint32_t file_entry_idx) {
    filesystem_write_file();
}

bool_t remove_file_from_dir(const char* dir_path, const char* file_path) {

}

bool_t filesystem_write_file(const char* path, const uint8_t* data, file_type_t type, size_t size) {
    // Check if file already exists
    uint8_t* _;
    uint32_t _2;
    file_entry_t current_file_entry;
    bool_t new_file = !filesystem_read_file(path, &_, &_2, &current_file_entry);
    free(_);

    load_file_table();

    // Find space for new file
    uint32_t file_start_sector = 0;
    if (!find_space_for_file(size, &file_start_sector)) {
        free_file_table();
        return false;
    }

    // Find file entry for new file
    uint8_t file_entry_idx = -1;
    if (new_file) { // If new file find an empty slot in the table
        for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
            if (file_table[i].magic_number != MAGIC_NUMBER) {
                file_entry_idx = i;
                break;
            }
        }
    } else { // If file already exists find the existing entry
        for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
            if (file_table[i].magic_number == MAGIC_NUMBER) {
                if (file_table[i].start_sector == current_file_entry.start_sector) {
                    file_entry_idx = i;
                    break;
                }
            }
        }
    }
    if (file_entry_idx == -1) {
        free_file_table();
        return false;
    }

    // Delete old file
    if (!new_file) {
        filesystem_delete_file(path); // TODO: don't delete file before checks that can return
    }

    // Create/Update file entry
    file_entry_t new_file_entry = (file_entry_t){ MAGIC_NUMBER, type, file_start_sector, size };
    file_table[file_entry_idx] = new_file_entry;

    if (new_file) {
        add_file_to_dir();
    }

    // Save file data
    write_sectors(file_start_sector, data, size);

    // Save updated file table
    write_sectors(1, file_table, FILE_TABLE_SIZE);

    free_file_table();
    return true;
}

bool_t filesystem_delete_file(const char* name) {
    file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    read_sectors(1, file_table, FILE_TABLE_SIZE);

    file_entry_t file_entry = (file_entry_t){};
    int file_entry_idx = 0;
    bool_t found = 0;

    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        file_entry = file_table[i];
        if (file_entry.magic_number != MAGIC_NUMBER)
            continue;

        if (strcmp(file_entry.name, name)) {
            file_entry_idx = i;
            found = 1;
            break;
        }
    }

    free(file_table);

    if (!found)
        return 0;

    file_table[file_entry_idx] = (file_entry_t){};
    shift_file_entries_backward(file_table, file_entry_idx + 1);

    return 1;
}

// bool_t filesystem_create_directory(const char* path) {
//
// }
//
// bool_t filesystem_directory_move_file(const char* path) {
//
// }

char** filesystem_list_files(const char* path, int* file_count) {
    // file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    //
    // filesystem_read_sectors(1, file_table, FILE_TABLE_SIZE);
    //
    // int count = 0;
    // int path_len = strlen(path);
    //
    // int needs_trailing_slash = (path_len > 0 && path[path_len - 1] != '/') ? 1 : 0;
    //
    // for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
    //     if (file_table[i].magic_number != MAGIC_NUMBER) continue;
    //
    //     const char* name = file_table[i].name;
    //
    //     if (strncmp(name, path, path_len)) {
    //         const char* relative_name = name + path_len;
    //
    //         if (*relative_name == '\0') continue;
    //
    //         if (strchr(relative_name, '/') == null) {
    //             count++;
    //         }
    //     }
    // }
    //
    // // Allocate exact size needed
    // char** files = malloc(count * sizeof(char*));
    //
    // int j = 0;
    // for (int i = 0; i < FILE_TABLE_ENTRIES && j < count; i++) {
    //     if (file_table[i].magic_number != MAGIC_NUMBER) continue;
    //
    //     const char* name = file_table[i].name;
    //     if (strncmp(name, path, path_len)) {
    //         const char* relative_name = name + path_len;
    //         if (*relative_name == '\0') continue;
    //
    //         if (strchr(relative_name, '/') == null) {
    //             files[j++] = strdup(name);
    //         }
    //     }
    // }
    //
    // free(file_table);
    // *file_count = j;
    // return files;
}

char** filesystem_list_dirs(const char* path, int* dir_count) {
    // file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    // if (!file_table) return null;
    //
    // filesystem_read_sectors(1, file_table, FILE_TABLE_SIZE);
    //
    // int count = 0;
    // int path_len = strlen(path);
    // char** found_dirs = malloc(FILE_TABLE_ENTRIES * sizeof(char*));
    //
    // for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
    //     if (file_table[i].magic_number != MAGIC_NUMBER) continue;
    //
    //     const char* name = file_table[i].name;
    //
    //     if (strncmp(name, path, path_len)) {
    //         const char* relative = name + path_len;
    //
    //         char* next_slash = strchr(relative, '/');
    //
    //         if (next_slash != null) {
    //             int full_dir_path_len = (next_slash - name) + 1;
    //
    //             char* dir_full_path = malloc(full_dir_path_len + 1);
    //             memcpy(dir_full_path, name, full_dir_path_len);
    //             dir_full_path[full_dir_path_len] = '\0';
    //
    //             bool_t is_duplicate = false;
    //             for (int k = 0; k < count; k++) {
    //                 // FIX: strcmp returns 0 on match.
    //                 if (strcmp(found_dirs[k], dir_full_path)) {
    //                     is_duplicate = true;
    //                     break;
    //                 }
    //             }
    //
    //             if (!is_duplicate) {
    //                 found_dirs[count++] = dir_full_path;
    //             } else {
    //                 free(dir_full_path);
    //             }
    //         }
    //     }
    // }
    //
    // free(file_table);
    //
    // char** result = malloc(count * sizeof(char*));
    // if (result) {
    //     memcpy(result, found_dirs, count * sizeof(char*));
    // }
    //
    // free(found_dirs);
    // *dir_count = count;
    // return result;
}

void filesystem_print_all_entries() {
    file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    read_sectors(1, file_table, FILE_TABLE_SIZE);

    screen_print("Printing All File Entries:\n");
    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        if (file_table[i].magic_number != MAGIC_NUMBER)
            break;

        char* id = num_to_str(i);
        char* sector = num_to_str(file_table[i].start_sector);
        char* size = num_to_str(file_table[i].size);
        char* strs[] = { "Id: ", id, "\n", "Name: ", file_table[i].name, "\n", "Sector: ", sector, "\n", "Size: ", size, "\n\n" };
        char* msg = str_concats(strs, sizeof(strs) / sizeof(strs[0]));
        screen_print(msg);
        free(id);
        free(sector);
        free(size);
        free(msg);
    }
    screen_print("------------------------------\n");

    free(file_table);
}