//
// Created by eitan on 1/20/26.
//

#include "filesystem.h"

#include "screen.h"
#include "memory/allocator.h"
#include "util/util.h"
#include "util/io.h"
#include "util/panic.h"
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

// Filesystem constants
#define MAGIC_NUMBER 0xE17A9055
#define SECTOR_SIZE 512
#define FILE_TABLE_SECTORS 20
#define ENTRIES_PER_SECTOR (SECTOR_SIZE / sizeof(file_entry_t))
#define FILE_TABLE_ENTRIES (ENTRIES_PER_SECTOR * FILE_TABLE_SECTORS)
#define FILE_TABLE_SIZE (FILE_TABLE_SECTORS * SECTOR_SIZE)
#define MAX_FILES FILE_TABLE_ENTRIES
#define FILE_NAME_LENGTH 60

typedef struct {
    uint32_t magic_number;
    uint16_t version;
    uint32_t sectors;
    uint32_t file_table_start;
    uint32_t file_table_size;
    uint16_t block_size;
} __attribute__((packed)) superblock_t;

typedef struct {
    char name[FILE_NAME_LENGTH];
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
    screen_print("[filesystem] identifying drive\n");

    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_SELECT_SLAVE);
    io_outb(ATA_SECTORS, 0);
    io_outb(ATA_LBA_LOW, 0);
    io_outb(ATA_LBA_MID, 0);
    io_outb(ATA_LBA_HIGH, 0);
    io_outb(ATA_COMMAND_STATUS, 0xEC); // Command 0xEC: IDENTIFY

    screen_print("[filesystem] sent identify command to drive\n");
    
    uint8_t status = io_inb(ATA_COMMAND_STATUS);
    if (status == 0) {
        panic("drive does not exist");
        return;
    }

    screen_print("[filesystem] drive exists\n");
    
    wait_busy();
    
    // Check if the drive is ATA or something else (like ATAPI/CD-ROM)
    writable_drive = 1;
    if (io_inb(ATA_LBA_MID) != 0 || io_inb(ATA_LBA_HIGH) != 0) {
        writable_drive = 0;
        panic("not a writeable drive");
        return;
    }

    screen_print("[filesystem] drive is writeable\n");
    
    wait_data_request_ready();
    
    uint16_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = io_inw(ATA_DATA);
    }

    screen_print("[filesystem] read identifying data from drive:\n");
    
    // Disk model
    for (int i = 0; i < 20; i++) {
        model[i * 2] = data[27 + i] >> 8;
        model[i * 2 + 1] = data[27 + i] & 0xFF;
    }
    model[40] = '\0';
    char* strs[] = { "[filesystem] the disk model connected: ", model, "\n" };
    char* msg = str_concats(strs, sizeof(strs) / sizeof(strs[0]));
    screen_print(msg);
    free(msg);
    
    // Disk number of sectors
    memcpy(&sector_count, &data[60], 2);
    char* sector_count_str = num_to_str(sector_count);
    char* size_str = num_to_str((double)sector_count * SECTOR_SIZE);
    char* size_KB_str = num_to_str((uint64_t)sector_count * SECTOR_SIZE / 1000);
    char* size_MB_str = num_to_str((uint64_t)sector_count * SECTOR_SIZE / 1000000);
    char* size_GB_str = num_to_str((uint64_t)sector_count * SECTOR_SIZE / 1000000000);
    char* strs2[] = { "[filesystem] sector count: ", sector_count_str, ", disk size: ", size_str, "B ", size_KB_str, "KB ", size_MB_str, "MB ", size_GB_str, "GB", "\n" };
    msg = str_concats(strs2, sizeof(strs2) / sizeof(strs2[0]));
    screen_print(msg);
    free(msg);
    free(sector_count_str);
    free(size_str);
    free(size_KB_str);
    free(size_MB_str);
    free(size_GB_str);
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

    screen_print("[filesystem] wrote superblock\n");
}

void free_file_table() {
    free(file_table);
    file_table = 0;
}

void load_file_table() {
    if (file_table != 0)
        free_file_table();

    file_table = malloc(FILE_TABLE_SIZE);
    read_sectors(1, file_table, FILE_TABLE_SIZE);
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

    screen_print("[filesystem] searching for root\n");

    uint32_t _;
    if (find_root(&_)) {
        screen_print("[filesystem] found root\n");
        return;
    }

    screen_print("[filesystem] root wasn't found, creating root...\n");

    memset(file_table, 0, FILE_TABLE_SIZE);
    screen_print("[filesystem] deleted file table\n");

    file_table[0] = (file_entry_t){ MAGIC_NUMBER, ROOT_DIRECTORY, 1 + FILE_TABLE_SECTORS, 0 };
    screen_print("[filesystem] created root entry\n");

    write_sectors(1, file_table, FILE_TABLE_SIZE);
    screen_print("[filesystem] saved new file table with root\n");

    free_file_table();
}

void filesystem_init(void) {
    identify_drive();
    init_superblock();
    ensure_root();

    screen_print("[filesystem] filesystem init\n");
}

bool_t is_valid_path(const char *path) {
    if (!path || path[0] != '/') return false;

    size_t len = strlen(path);
    if (len == 0) return false;

    int component_len = 0;
    for (size_t i = 1; i < len; i++) {
        char c = path[i];

        if (c == '/') {
            if (component_len == 0) return false; // empty component, e.g. "//"
            component_len = 0;
            continue;
        }

        if (c == '\0') return false; // shouldn't happen given strlen, just safe

        component_len++;
        if (component_len > FILE_NAME_LENGTH) return false; // name too long
    }

    if (component_len == 0 && len > 1) return false; // trailing slash, e.g. "/foo/"

    return true;
}

bool_t resolve_path(const char* path, uint32_t* file_entry_idx_out) {
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

    *file_entry_idx_out = current_index;
    return true;
}

void split_path(const char* path, char** dir_path_out, char** name_out) {
    uint64_t last_slash_idx = (uint64_t)strrchr(path, '/') - (uint64_t)path;

    size_t name_size = strlen(path) - last_slash_idx;
    char* name = malloc(name_size); // Size of name at end of path, not including '/', including '\0'
    char* dir_path = malloc(last_slash_idx + 2); // Size of path until name at the end, including last '/', including additional '\0'

    memcpy(name, &path[last_slash_idx + 1], name_size);
    memcpy(dir_path, path, last_slash_idx + 1);
    dir_path[last_slash_idx + 1] = '\0';

    *dir_path_out = dir_path;
    *name_out = name;
}

bool_t filesystem_read_file(const char* path, uint8_t** data_out, size_t* data_size_out, uint32_t* file_entry_idx_out) {
    if (!is_valid_path(path))
        return false;

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
    *file_entry_idx_out = file_index;

    free_file_table();
    return true;
}

bool_t find_space_for_file(size_t file_size, uint32_t *file_start_sector_out) {
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
            *file_start_sector_out = prev_end;
            return true;
        }
        prev_end = entry->start_sector + (entry->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    }

    uint32_t trailing_gap = sector_count - 1 - prev_end;
    if (trailing_gap >= sectors_needed) {
        *file_start_sector_out = prev_end;
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

bool_t edit_file(uint32_t file_entry_idx, const uint8_t* data, size_t size) {
    if (file_table[file_entry_idx].magic_number != MAGIC_NUMBER) // File doesn't exist
        return false;

    // Find space for new file
    uint32_t file_start_sector;
    file_table[file_entry_idx].magic_number = null; // Remove current file from search, so the new file can be in the same place
    if (!find_space_for_file(size, &file_start_sector)) {
        file_table[file_entry_idx].magic_number = MAGIC_NUMBER;
        return false;
    }

    // Update entry
    file_table[file_entry_idx].magic_number = MAGIC_NUMBER;
    file_table[file_entry_idx].start_sector = file_start_sector;

    // Write file data to disk
    write_sectors(file_start_sector, data, size);

    // Update file table on disk
    write_sectors(1, file_table, FILE_TABLE_SIZE);

    return true;
}

bool_t add_file_to_dir(uint32_t dir_file_entry_idx, uint32_t file_entry_idx, const char* name) {
    // Current dir entries
    size_t entries_count = file_table[dir_file_entry_idx].size / sizeof(dir_entry_t) + 1; // Plus 1 for new file
    dir_entry_t* entries = malloc(entries_count * sizeof(dir_entry_t));

    // Load current dir entries
    if (file_table[dir_file_entry_idx].size > 0)
        read_sectors(file_table[dir_file_entry_idx].start_sector, entries, file_table[dir_file_entry_idx].size);

    // Add new file to dir entries
    entries[entries_count - 1].file_table_index = file_entry_idx;
    memcpy(entries[entries_count - 1].name, name, min(strlen(name) + 1, FILE_NAME_LENGTH));
    entries[entries_count - 1].name[FILE_NAME_LENGTH - 1] = '\0';

    // Edit dir file
    bool_t success = edit_file(dir_file_entry_idx, (uint8_t*)entries, entries_count * sizeof(dir_entry_t));
    free(entries);
    return success;
}

bool_t remove_file_from_dir(uint32_t dir_file_entry_idx, uint32_t file_entry_idx) {
    // Current dir entries
    size_t old_entries_count = file_table[dir_file_entry_idx].size / sizeof(dir_entry_t);
    dir_entry_t* old_entries = malloc(old_entries_count * sizeof(dir_entry_t));

    // Load current dir entries
    if (file_table[dir_file_entry_idx].size > 0)
        read_sectors(file_table[dir_file_entry_idx].start_sector, old_entries, file_table[dir_file_entry_idx].size);

    // Copy old entries, not including the removed one
    size_t new_entries_count = old_entries_count -1;
    dir_entry_t* new_entries = malloc(new_entries_count * sizeof(dir_entry_t));
    uint64_t j = 0;
    for (uint64_t i = 0; i < old_entries_count; i++) {
        if (old_entries[i].file_table_index != file_entry_idx)
            new_entries[j++] = old_entries[i];
    }

    // Edit dir file
    bool_t success = edit_file(dir_file_entry_idx, (uint8_t*)new_entries, new_entries_count * sizeof(dir_entry_t));
    free(old_entries);
    free(new_entries);
    return success;
}

uint32_t find_file_entry_slot() {
    for (uint32_t i = 0; i < FILE_TABLE_ENTRIES; i++) {
        if (file_table[i].magic_number != MAGIC_NUMBER)
            return i;
    }

    return -1;
}

bool_t filesystem_write_file(const char* path, const uint8_t* data, file_type_t type, size_t size) {
    if (!is_valid_path(path))
        return false;

    // Check if file already exists
    uint8_t* _;
    uint32_t _2;
    uint32_t current_file_entry_idx;
    bool_t new_file = !filesystem_read_file(path, &_, &_2, &current_file_entry_idx);
    free(_);

    load_file_table();

    // If not new file, edit file
    if (!new_file) {
        bool_t success = edit_file(current_file_entry_idx, data, size);
        free_file_table();
        return success;
    }

    // Find space for new file
    uint32_t file_start_sector = 0;
    if (!find_space_for_file(size, &file_start_sector)) {
        free_file_table();
        return false;
    }

    // Find free file entry slot
    uint32_t file_entry_idx = find_file_entry_slot();
    if (file_entry_idx == -1) {
        free_file_table();
        return false;
    }

    // Create file entry
    file_entry_t new_file_entry = (file_entry_t){ MAGIC_NUMBER, type, file_start_sector, size };
    file_table[file_entry_idx] = new_file_entry;

    // Extract name and dir_path from path
    char* dir_path;
    char* name;
    split_path(path, &dir_path, &name);

    // Add file to dir
    uint32_t dir_file_index;
    if (!resolve_path(dir_path, &dir_file_index)) {
        free(name);
        free(dir_path);
        free_file_table();
        return false;
    }
    add_file_to_dir(dir_file_index, file_entry_idx, name);

    free(name);
    free(dir_path);

    // Save updated file table
    // write_sectors(1, file_table, FILE_TABLE_SIZE); COMMENTED BECAUSE add_file_to_dir ALREADY SAVES FILE TABLE CHANGES

    // Save file data
    write_sectors(file_start_sector, data, size);

    free_file_table();
    return true;
}

bool_t filesystem_delete_file(const char* path) {
    if (!is_valid_path(path))
        return false;

    load_file_table();

    // Find file entry
    uint32_t file_entry_idx;
    if (!resolve_path(path, &file_entry_idx)) {
        free_file_table();
        return false;
    }

    // Find dir path
    char* dir_path;
    char* name;
    split_path(path, &dir_path, &name);

    // Find file's dir's file entry
    uint32_t dir_file_entry_idx;
    if (!resolve_path(dir_path, &dir_file_entry_idx)) {
        free(dir_path);
        free(name);
        free_file_table();
        return false;
    }

    // Remove file from its dir
    if (!remove_file_from_dir(dir_file_entry_idx, file_entry_idx)) {
        free(dir_path);
        free(name);
        free_file_table();
        return false;
    }

    // Remove file entry
    file_table[file_entry_idx].magic_number = null;

    // Save file table changes
    write_sectors(1, file_table, FILE_TABLE_SIZE);

    free_file_table();
    return true;
}

bool_t filesystem_create_directory(const char* path) {
    uint8_t data = 0;
    return filesystem_write_file(path, &data, DIRECTORY, 0);
}

bool_t filesystem_move_file(const char* path, const char* new_path) {
    if (!is_valid_path(path) || !is_valid_path(new_path))
        return false;

    // Split paths
    char* dir_path;
    char* name;
    split_path(path, &dir_path, &name);
    char* new_dir_path;
    char* new_name;
    split_path(new_path, &new_dir_path, &new_name);

    // Find file entry
    uint32_t file_entry_idx;
    if (!resolve_path(path, &file_entry_idx)) {
        goto fail;
    }

    // Find file's dir's file entry
    uint32_t dir_file_entry_idx;
    if (!resolve_path(dir_path, &dir_file_entry_idx)) {
        goto fail;
    }

    // Find new file's dir's file entry
    uint32_t new_dir_file_entry_idx;
    if (!resolve_path(new_dir_path, &new_dir_file_entry_idx)) {
        goto fail;
    }

    // Move the file
    remove_file_from_dir(dir_file_entry_idx, file_entry_idx);
    add_file_to_dir(new_dir_file_entry_idx, file_entry_idx, new_name);

    free(dir_path);
    free(name);
    free(new_dir_path);
    free(new_name);
    free_file_table();
    return true;

fail:
    free(dir_path);
    free(name);
    free(new_dir_path);
    free(new_name);
    free_file_table();
    return false;
}

char** filesystem_list_dir(const char* path, uint64_t* file_count) {
    // Read dir file
    dir_entry_t* entries;
    size_t data_size;
    uint32_t file_entry_idx;
    if (!filesystem_read_file(path, (uint8_t**)&entries, &data_size, &file_entry_idx))
        return false;

    // Allocate array
    size_t entries_count = data_size / sizeof(dir_entry_t);
    char** names = malloc(entries_count * sizeof(char*));

    // Fill array with allocated names
    for (uint64_t i = 0; i < entries_count; i++) {
        names[i] = strdup(entries[i].name);
    }

    *file_count = entries_count;
    return names;
}

void filesystem_print_all_entries() {
    load_file_table();

    screen_print("Printing All File Entries:\n");
    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        if (file_table[i].magic_number != MAGIC_NUMBER)
            break;

        char* id = num_to_str(i);
        char* sector = num_to_str(file_table[i].start_sector);
        char* size = num_to_str(file_table[i].size);
        char* strs[] = { "Id: ", id, "\n", "Sector: ", sector, "\n", "Size: ", size, "\n\n" };
        char* msg = str_concats(strs, sizeof(strs) / sizeof(strs[0]));
        screen_print(msg);
        free(id);
        free(sector);
        free(size);
        free(msg);
    }
    screen_print("------------------------------\n");

    free_file_table();
}

void print_tree_recursive(uint32_t dir_index, int depth) {
    file_entry_t dir_entry = file_table[dir_index];
    if (dir_entry.size == 0)
        return;

    dir_entry_t* entries = malloc(dir_entry.size);
    read_sectors(dir_entry.start_sector, entries, dir_entry.size);
    uint32_t count = dir_entry.size / sizeof(dir_entry_t);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = entries[i].file_table_index;
        file_entry_t entry = file_table[idx];
        if (entry.magic_number != MAGIC_NUMBER)
            continue;

        for (int d = 0; d < depth; d++)
            screen_print("  ");

        bool_t is_dir = entry.type == DIRECTORY || entry.type == ROOT_DIRECTORY;
        char* idx_str = num_to_str(idx);
        char* strs[] = { is_dir ? "[DIR] " : "[FILE] ", entries[i].name, " (idx ", idx_str, ")\n" };
        char* msg = str_concats(strs, 5);
        screen_print(msg);
        free(msg);
        free(idx_str);

        if (is_dir)
            print_tree_recursive(idx, depth + 1);
    }

    free(entries);
}

void filesystem_print_tree() {
    load_file_table();

    uint32_t root_index;
    if (!find_root(&root_index)) {
        screen_print("No root directory found\n");
        free_file_table();
        return;
    }

    screen_print("Filesystem Tree:\n");
    print_tree_recursive(root_index, 0);
    screen_print("------------------------------\n");

    free_file_table();
}