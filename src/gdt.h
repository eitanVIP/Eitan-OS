//
// Created by eitan on 10/2/25.
//

#pragma once

void gdt_init();
unsigned short gdt_get_index(unsigned char index, unsigned char TI, unsigned char RPL);