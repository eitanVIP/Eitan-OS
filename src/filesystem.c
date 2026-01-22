//
// Created by eitan on 1/20/26.
//

#include "filesystem.h"

#include "eitan_lib.h"
#include "io.h"
#include "screen.h"

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
    unsigned int magic_number;
    unsigned short version;
    unsigned int sectors;
    unsigned int file_table_start;
    unsigned int file_table_size;
    unsigned short block_size;
} superblock_t;

typedef struct {
    unsigned int magic_number;
    char name[52];
    unsigned int start_sector;
    unsigned int size;
} file_entry_t;

static char model[41];
static unsigned short sector_count;
static unsigned char writable_drive;

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

    unsigned char status = io_inb(ATA_COMMAND_STATUS);
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

    unsigned short data[256];
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

void filesystem_read_sectors(unsigned int lba, void* target, size_t size) {
    const unsigned char count = ceil((double)size / SECTOR_SIZE);

    if (count < 1 || lba + count >= sector_count)
        return;

    wait_busy();

    // Set up the Drive/Head Register
    // 1, Use LBA, 1, Master, LBA
    // 1  1        1  0       0000
    // LBA is 28 bits, I put here the last 4: 24-27
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_SELECT_SLAVE_LBA | ((lba >> 24) & 0b1111));

    io_outb(ATA_SECTORS, count);                       // Sector count
    io_outb(ATA_LBA_LOW, (unsigned char)lba);          // LBA Low (bits 0-7)
    io_outb(ATA_LBA_MID, (unsigned char)(lba >> 8));   // LBA Mid (bits 8-15)
    io_outb(ATA_LBA_HIGH, (unsigned char)(lba >> 16)); // LBA High (bits 16-23)

    io_outb(ATA_COMMAND_STATUS, 0x20); //0x20 READ command

    unsigned short* t = target;
    for (int j = 0; j < count; j++) {
        wait_busy();
        wait_data_request_ready();

        for (int i = 0; i < 256; i++) {
            int word_idx = j * 256 + i;
            int byte_idx = word_idx * 2;

            unsigned short data = io_inw(ATA_DATA);
            if (byte_idx < size) {
                if (byte_idx + 1 >= size)
                    t[word_idx] = data & 0xFF;
                else
                    t[word_idx] = data;
            }
        }
    }
}

void filesystem_write_sectors(unsigned int lba, const void* src, size_t size) {
    const unsigned char count = ceil((double)size / SECTOR_SIZE);

    if (!writable_drive || count < 1 || lba + count >= sector_count)
        return;

    wait_busy();

    // Set up the Drive/Head Register
    // 1, Use LBA, 1, Master, LBA
    // 1  1        1  0       0000
    // LBA is 28 bits, I put here the last 4: 24-27
    io_outb(ATA_DRIVE_SELECT, ATA_DRIVE_SELECT_SLAVE_LBA | ((lba >> 24) & 0b1111));

    io_outb(ATA_SECTORS, count);                       // Sector count
    io_outb(ATA_LBA_LOW, (unsigned char)lba);          // LBA Low (bits 0-7)
    io_outb(ATA_LBA_MID, (unsigned char)(lba >> 8));   // LBA Mid (bits 8-15)
    io_outb(ATA_LBA_HIGH, (unsigned char)(lba >> 16)); // LBA High (bits 16-23)

    io_outb(ATA_COMMAND_STATUS, 0x30); //0x30 WRITE command

    unsigned short* s = src;
    for (int j = 0; j < count; j++) {
        wait_busy();
        wait_data_request_ready();

        for (int i = 0; i < 256; i++) {
            int word_idx = j * 256 + i;
            int byte_idx = word_idx * 2;

            if (byte_idx < size) {
                if (byte_idx + 1 >= size) {
                    unsigned short data = ((unsigned char*)s)[byte_idx];
                    io_outw(ATA_DATA, data);
                }
                else {
                    unsigned short data = s[word_idx];
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
    filesystem_write_sectors(0, &superblock, sizeof(superblock));
}

void filesystem_init(void) {
    identify_drive();
    init_superblock();
}

bool_t filesystem_read_file(const char* name, uint8_t** data_ptr) {
    file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    filesystem_read_sectors(1, file_table, FILE_TABLE_SIZE);

    file_entry_t file_entry = (file_entry_t){};
    bool_t found = 0;

    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        file_entry = file_table[i];
        if (file_entry.magic_number != MAGIC_NUMBER)
            continue;

        if (strcmp(file_entry.name, name)) {
            found = 1;
            break;
        }
    }

    free(file_table);

    if (!found)
        return 0;

    uint8_t* data = malloc(file_entry.size);
    filesystem_read_sectors(file_entry.start_sector, data, file_entry.size);
    *data_ptr = data;

    return 1;
}

// static bool_t find_next_entry(int entry_sector, const uint8_t* entry_sector_buf, int entry_idx, file_entry_t* next_entry) {
//     for (int i = entry_idx + 1; i < 4; i++) {
//         *next_entry = ((file_entry_t*)&entry_sector_buf)[i];
//         if (next_entry->magic_number != MAGIC_NUMBER)
//             continue;
//
//         return 1;
//     }
//
//     for (int i = entry_sector; i < FILE_TABLE_SECTORS + 1; i++) {
//         unsigned char sector_buf[SECTOR_SIZE] = {};
//         filesystem_read_sectors(i, sector_buf, SECTOR_SIZE);
//
//         for (int j = 0; j < 4; j++) {
//             *next_entry = ((file_entry_t*)&sector_buf)[j];
//             if (next_entry->magic_number != MAGIC_NUMBER)
//                 continue;
//
//             return 1;
//         }
//     }
//
//     return 0;
// }

static bool_t find_space_for_file(file_entry_t* file_table, size_t file_size, uint32_t* file_start_sector, uint8_t* file_entry_idx) {
    bool_t are_there_entries = 0;

    for (int i = 0; i < FILE_TABLE_ENTRIES; i++) {
        file_entry_t entry = file_table[i];
        if (entry.magic_number != MAGIC_NUMBER)
            break;
        are_there_entries = 1;

        // file_entry_t next_entry = {};
        // bool_t found_next = find_next_entry(i, sector_buf, j, &next_entry);

        uint32_t file_end_sector = entry.start_sector + (uint32_t)ceil((double)entry.size / SECTOR_SIZE);
        uint32_t space = 0;
        if (i + 1 < FILE_TABLE_ENTRIES && file_table[i + 1].magic_number == MAGIC_NUMBER) { // Is there next entry
            file_entry_t next_entry = file_table[i + 1];
            space = SECTOR_SIZE * (next_entry.start_sector - file_end_sector);
        } else {
            space = SECTOR_SIZE * (sector_count - 1 - file_end_sector);
        }

        if (space >= file_size) {
            *file_start_sector = file_end_sector;
            *file_entry_idx = i + 1;

            return 1;
        }
    }

    if (!are_there_entries) {
        if ((sector_count - 1 - FILE_TABLE_ENTRIES) * SECTOR_SIZE >= file_size) {
            *file_start_sector = FILE_TABLE_ENTRIES + 1;
            *file_entry_idx = 0;

            return 1;
        }
    }

    return 0;
}

static void shift_file_entries_forward(file_entry_t* file_table, uint8_t idx_start) {
    for (uint32_t i = FILE_TABLE_ENTRIES - 1; i > idx_start; i--) {
        file_table[i] = file_table[i - 1];
    }

    file_table[idx_start] = (file_entry_t){};

    filesystem_write_sectors(1, file_table, FILE_TABLE_SIZE);
}

static void shift_file_entries_backward(file_entry_t* file_table, uint8_t idx_start) {
    for (uint32_t i = max(idx_start - 1, 0); i < FILE_TABLE_ENTRIES - 1; i++) {
        file_table[i] = file_table[i + 1];
    }

    file_table[FILE_TABLE_ENTRIES - 1] = (file_entry_t){};

    filesystem_write_sectors(1, file_table, FILE_TABLE_SIZE);
}

bool_t filesystem_write_file(const char* name, const uint8_t* data, size_t size) {
    uint8_t* _;
    if (filesystem_read_file(name, &_)) {
        filesystem_delete_file(name);
    }

    file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    filesystem_read_sectors(1, file_table, FILE_TABLE_SIZE);

    uint32_t file_start_sector = 0;
    uint8_t file_entry_idx = 0;
    if (find_space_for_file(file_table, size, &file_start_sector, &file_entry_idx)) {
        shift_file_entries_forward(file_table, file_entry_idx);

        file_entry_t new_file_entry = {};
        new_file_entry.magic_number = MAGIC_NUMBER;
        new_file_entry.start_sector = file_start_sector;
        new_file_entry.size = size;
        memcpy(new_file_entry.name, name, min(strlen(name), 52));

        file_table[file_entry_idx] = new_file_entry;

        filesystem_write_sectors(file_start_sector, data, size);
        filesystem_write_sectors(1, file_table, FILE_TABLE_SIZE);
        free(file_table);

        return 1;
    }

    return 0;
}

bool_t filesystem_delete_file(const char* name) {
    file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    filesystem_read_sectors(1, file_table, FILE_TABLE_SIZE);

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

void filesystem_print_all_entries() {
    file_entry_t* file_table = malloc(FILE_TABLE_SIZE);
    filesystem_read_sectors(1, file_table, FILE_TABLE_SIZE);

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