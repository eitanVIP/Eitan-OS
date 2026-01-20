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

static void filesystem_wait_busy() {
    while (io_inb(ATA_COMMAND_STATUS) & ATA_STATUS_BUSY);
}

static void filesystem_wait_data_request_ready() {
    while (!(io_inb(ATA_COMMAND_STATUS) & ATA_STATUS_DATA_REQUEST_READY));
}

static void filesystem_identify_drive() {
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

    filesystem_wait_busy();

    // Check if the drive is ATA or something else (like ATAPI/CD-ROM)
    writable_drive = 1;
    if (io_inb(ATA_LBA_MID) != 0 || io_inb(ATA_LBA_HIGH) != 0) {
        writable_drive = 0;
        screen_print("Not a writable drive");
        return;
    }

    filesystem_wait_data_request_ready();

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

    filesystem_wait_busy();

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
        filesystem_wait_busy();
        filesystem_wait_data_request_ready();

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

    filesystem_wait_busy();

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
        filesystem_wait_busy();
        filesystem_wait_data_request_ready();

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

static void filesystem_init_superblock() {
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
    filesystem_identify_drive();
    filesystem_init_superblock();
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

int filesystem_write_file(char* name, unsigned char* data, size_t size) {
    for (int i = 1; i < FILE_TABLE_SECTORS + 1; i++) {
        unsigned char sector_buf[512] = {};
        filesystem_read_sectors(i, sector_buf, 512);

        for (int j = 0; j < 4; j++) {
            file_entry_t entry = ((file_entry_t*)&sector_buf)[j];
            if (entry.magic_number != MAGIC_NUMBER)
                continue;

            file_entry_t next_entry;
            unsigned char found_next_entry = 0;

            // Before running this loop, check if next entry is in the current sector

            for (int k = i + 1; k < FILE_TABLE_SECTORS + 1; k++) {
                unsigned char next_sector_buf[512] = {};
                filesystem_read_sectors(k, sector_buf, 512);

                for (int l = 0; l < 4; l++) {
                    next_entry = ((file_entry_t*)&next_sector_buf)[k];
                    if (entry.magic_number != MAGIC_NUMBER)
                        continue;

                    found_next_entry = 1;
                    break;
                }

                if (found_next_entry)
                    break;
            }

            if (found_next_entry) {
                unsigned int space = next_entry.start_sector - (entry.start_sector + entry.size);
                if (space >= size) {
                    // Place at entry.start_sector + entry.size
                }
            } else {
                // Check if can place in end
            }
        }
    }

    return 0;
}