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

#define FILE_TABLE_SECTORS 20
#define MAX_FILES (512 / 64 * FILE_TABLE_SECTORS)
#define MAGIC_NUMBER 0xE17A9055

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
    // memcpy(model, &data[27], 40);
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
    char* size_str = num_to_str((double)sector_count * 512);
    char* strs2[] = { "Sector count: ", sector_count_str, ", Disk size: ", size_str, "\n" };
    msg = str_concats(strs2, 5);
    screen_print(msg);
    free(msg);
    free(sector_count_str);
    free(size_str);
}

void filesystem_read_sectors(unsigned int lba, void* target, size_t size) {
    const unsigned char count = ceil(size / 512.0);

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

void filesystem_write_sectors(unsigned int lba, void* src, size_t size) {
    const unsigned char count = ceil(size / 512.0);

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
        512
    };
    filesystem_write_sectors(0, &superblock, sizeof(superblock));
}

void filesystem_init(void) {
    identify_drive();
    init_superblock();
}

int filesystem_read_file(char* name, unsigned char** data_ptr) {
    file_entry_t file_entry;
    unsigned char found = 0;

    for (int i = 1; i < FILE_TABLE_SECTORS + 1; i++) {
        unsigned char sector_buf[512] = {};
        filesystem_read_sectors(i, sector_buf, 512);

        for (int j = 0; j < 4; j++) {
            file_entry = ((file_entry_t*)&sector_buf)[j];
            if (file_entry.magic_number != MAGIC_NUMBER)
                continue;

            if (strcmp(file_entry.name, name)) {
                found = 1;
                break;
            }
        }

        if (found)
            break;
    }

    if (!found)
        return 1;

    unsigned char* data = malloc(file_entry.size);
    filesystem_read_sectors(file_entry.start_sector, data, file_entry.size);
    *data_ptr = data;

    return 0;
}

static bool_t find_next_entry(int entry_sector, const uint8_t* entry_sector_buf, int entry_idx, file_entry_t* next_entry) {
    for (int i = entry_idx + 1; i < 4; i++) {
        *next_entry = ((file_entry_t*)&entry_sector_buf)[i];
        if (next_entry->magic_number != MAGIC_NUMBER)
            continue;

        return 1;
    }

    for (int i = entry_sector; i < FILE_TABLE_SECTORS + 1; i++) {
        unsigned char sector_buf[512] = {};
        filesystem_read_sectors(i, sector_buf, 512);

        for (int j = 0; j < 4; j++) {
            *next_entry = ((file_entry_t*)&sector_buf)[j];
            if (next_entry->magic_number != MAGIC_NUMBER)
                continue;

            return 1;
        }
    }

    return 0;
}

static bool_t find_space_for_file(size_t file_size, uint32_t* file_start_sector, uint32_t* file_entry_sector, uint8_t* file_entry_idx) {
    bool_t are_there_entries = 0;

    for (int i = 1; i < FILE_TABLE_SECTORS + 1; i++) {
        unsigned char sector_buf[512] = {};
        filesystem_read_sectors(i, sector_buf, 512);

        for (int j = 0; j < 4; j++) {
            file_entry_t entry = ((file_entry_t*)&sector_buf)[j];
            if (entry.magic_number != MAGIC_NUMBER)
                continue;

            are_there_entries = 1;

            file_entry_t next_entry = {};
            bool_t found_next = find_next_entry(i, sector_buf, j, &next_entry);

            // Find how much space between end of current file and (start of next file or end of disk)
            uint32_t file_end_sector = entry.start_sector + (uint32_t)ceil(entry.size / 512);
            uint32_t space = 0;
            if (found_next)
                space = 512 * (next_entry.start_sector - file_end_sector);
            else
                space = 512 * (sector_count - 1 - file_end_sector);

            if (space >= file_size) {
                *file_start_sector = file_end_sector;
                if (j + 1 < 4) {
                    *file_entry_sector = i;
                    *file_entry_idx = j + 1;
                } else {
                    *file_entry_sector = i + 1;
                    *file_entry_idx = 0;
                }

                return 1;
            }
        }
    }

    if (!are_there_entries) {
        if ((sector_count - 1 - FILE_TABLE_SECTORS) * 512 >= file_size) {
            *file_start_sector = FILE_TABLE_SECTORS + 1;
            *file_entry_sector = 1;
            *file_entry_idx = 0;

            return 1;
        }
    }

    return 0;
}

static void shift_file_entries_forward(uint32_t sector_start, uint32_t idx_start) {
    file_entry_t next_entry = {};

    file_entry_t entries[512 / sizeof(file_entry_t)] = {};
    filesystem_read_sectors(sector_start, entries, 512);

    for (uint8_t i = idx_start; i < 4; i++) {
        file_entry_t entry = entries[i];

        if (i < 3) {
            file_entry_t temp = entries[i];
            entries[i] = {};
            next_entry = entries[i + 1];
            entries[i + 1] = next_entry;
        } else {
            file_entry_t temp = entries[i];
            entries[i] = next_entry;
            next_entry = entries[i + 1];
            entries[i + 1] = temp;
        }
    }

    for (uint32_t i = sector_start + 1; i < FILE_TABLE_SECTORS + 1; i++) {
        unsigned char sector_buf[512] = {};
        filesystem_read_sectors(i, sector_buf, 512);

        for (uint8_t j = 0; j < 4; j++) {
            file_entry_t entry = ((file_entry_t*)&sector_buf)[j];
            if (entry.magic_number != MAGIC_NUMBER)
                continue;
        }
    }
}

bool_t filesystem_write_file(char* name, unsigned char* data, size_t size) {
    uint32_t file_start_sector = 0;
    uint32_t file_entry_sector = 0;
    uint8_t file_entry_idx = 0;
    if (find_space_for_file(size, &file_start_sector, &file_entry_sector, &file_entry_idx)) {

    }

    return 0;
}