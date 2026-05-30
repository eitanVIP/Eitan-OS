#include "vmm.h"

#include "pmm.h"
#include "../util/util.h"
#include "../screen.h"

#define PML4_INDEX(virt)  (((virt) >> 39) & 0x1FF)
#define PDPT_INDEX(virt)  (((virt) >> 30) & 0x1FF)
#define PD_INDEX(virt)    (((virt) >> 21) & 0x1FF)
#define PT_INDEX(virt)    (((virt) >> 12) & 0x1FF)

extern uint8_t kernel_end[];
#define KERNEL_START 0xffffffff80000000
#define KERNEL_END ((uint64_t)(uint8_t*)kernel_end)

static PML4Table* current_PML4;
static uint64_t hhdm_offset;
static uint64_t kernel_start_phys;
static uint64_t kernel_end_phys;

bool_t vmm_create_PML4(PML4Table** PML4_ptr) {
    uint64_t addr = (uint64_t)pmm_alloc_frame() + hhdm_offset;
    if (addr == null)
        return false;

    memset((void*)addr, 0, PAGE_SIZE);
    *PML4_ptr = (PML4Table*)addr;
    return true;
}

void vmm_unmap_PML4(PML4Table* PML4) {
    for (int i_PML4 = 0; i_PML4 < 512; i_PML4++) {
        if (!PML4->entries[i_PML4].present)
            continue;

        PDPTable* PDPT = (PDPTable*)((PML4->entries[i_PML4].physical_addr << 12) + hhdm_offset);

        for (int i_PDPT = 0; i_PDPT < 512; i_PDPT++) {
            if (!PDPT->entries[i_PDPT].present)
                continue;

            PageDirectory* PD = (PageDirectory*)((PDPT->entries[i_PDPT].physical_addr << 12) + hhdm_offset);

            for (int i_PD = 0; i_PD < 512; i_PD++) {
                if (!PD->entries[i_PD].present)
                    continue;

                PageTable* PT = (PageTable*)((PD->entries[i_PD].physical_addr << 12) + hhdm_offset);

                vmm_unmap_page((uint64_t)PT);
            }

            vmm_unmap_page((uint64_t)PD);
        }

        vmm_unmap_page((uint64_t)PDPT);
    }

    vmm_unmap_page((uint64_t)PML4);
}

PML4Table* vmm_init(volatile struct limine_hhdm_request* hhdm_request, volatile struct limine_executable_address_request* kernel_address_request) {
    hhdm_offset = hhdm_request->response->offset;

    PML4Table* kernel_PML4 = null;
    bool_t success = vmm_create_PML4(&kernel_PML4);

    if (!success) {
        screen_print("[vmm] vmm init failed: can't create kernel PML4\n");
        return null;
    }
    screen_print("[vmm] created kernel PML4\n");

    kernel_start_phys = kernel_address_request->response->physical_base;
    kernel_end_phys = KERNEL_END - KERNEL_START + kernel_start_phys;

    PML4Table* limine_pml4;
    asm volatile("mov %%cr3, %0" : "=r"(limine_pml4));
    limine_pml4 = (PML4Table*)((uint64_t)limine_pml4 + hhdm_offset);
    for (int i = 256; i < 512; i++)
        kernel_PML4->entries[i] = limine_pml4->entries[i];

    vmm_set_PML4(kernel_PML4);

    // uint64_t phys = kernel_start_phys;
    // for (uint64_t virt = KERNEL_START; virt < KERNEL_END; virt += PAGE_SIZE) {
    //     success = vmm_map_page(virt, phys, VMM_FLAGS_KERNEL_ALL);
    //
    //     if (!success) {
    //         screen_print("[vmm] vmm init failed: can't map kernel\n");
    //         return null;
    //     }
    //
    //     phys += PAGE_SIZE;
    // }
    // screen_print("[vmm] mapped kernel\n");

    // uint64_t pmm_virt = (uint64_t)pmm_get_bitmap();
    // for (uint64_t virt = pmm_virt; virt < pmm_virt + pmm_get_bitmap_size(); virt += PAGE_SIZE) {
    //     uint64_t phys = virt - hhdm_offset;
    //     success = vmm_map_page(virt, phys, VMM_FLAGS_KERNEL_RW);
    //
    //     if (!success) {
    //         screen_print("[vmm] vmm init failed: can't map pmm bitmap\n");
    //         return null;
    //     }
    // }
    // screen_print("[vmm] mapped pmm bitmap\n");

    // success = vmm_map_page((uint64_t)kernel_PML4, (uint64_t)kernel_PML4 - hhdm_offset, VMM_FLAGS_KERNEL_RW);
    // if (!success) {
    //     screen_print("[vmm] vmm init failed: can't map PML4\n");
    //     return null;
    // }

    screen_print("[vmm] mapped all limine mappings to new kernel map\n");

    vmm_load_cpu();
    screen_print("[vmm] loaded new kernel PML4 to cpu\n");
    screen_print("[vmm] vmm init\n");

    return kernel_PML4;
}

void vmm_copy_kernel_PML4(PML4Table* to, PML4Table* from) {
    for (int i = 256; i < 512; i++)
        to->entries[i] = from->entries[i];
}

void vmm_set_PML4(PML4Table* PML4) {
    current_PML4 = PML4;
}

bool_t vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present) {
        pdpt_addr = (PDPTable*)((uint64_t)pmm_alloc_frame() + hhdm_offset);
        if (pdpt_addr == null)
            return false;
        memset(pdpt_addr, 0, PAGE_SIZE);

        uint64_t table_flags = VMM_FLAGS_TABLE;
        memcpy(&current_PML4->entries[PML4_INDEX(virt)], &table_flags, sizeof(page_entry_t));
        current_PML4->entries[PML4_INDEX(virt)].physical_addr = ((uint64_t)pdpt_addr - hhdm_offset) >> 12;
    } else {
        pdpt_addr = (PDPTable*)((current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12) + hhdm_offset);
    }

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present) {
        pd_addr = (PageDirectory*)((uint64_t)pmm_alloc_frame() + hhdm_offset);
        if (pd_addr == null)
            return false;
        memset(pd_addr, 0, PAGE_SIZE);

        uint64_t table_flags = VMM_FLAGS_TABLE;
        memcpy(&pdpt_addr->entries[PDPT_INDEX(virt)], &table_flags, sizeof(page_entry_t));
        pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr = ((uint64_t)pd_addr - hhdm_offset) >> 12;
    } else {
        pd_addr = (PageDirectory*)((pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12) + hhdm_offset);
    }

    if (!pd_addr->entries[PD_INDEX(virt)].present) {
        pt_addr = (PageTable*)((uint64_t)pmm_alloc_frame() + hhdm_offset);
        if (pt_addr == null)
            return false;
        memset(pt_addr, 0, PAGE_SIZE);

        uint64_t table_flags = VMM_FLAGS_TABLE;
        memcpy(&pd_addr->entries[PD_INDEX(virt)], &table_flags, sizeof(page_entry_t));
        pd_addr->entries[PD_INDEX(virt)].physical_addr = ((uint64_t)pt_addr - hhdm_offset) >> 12;
    } else {
        pt_addr = (PageTable*)((pd_addr->entries[PD_INDEX(virt)].physical_addr << 12) + hhdm_offset);
    }

    if (pt_addr->entries[PT_INDEX(virt)].present)
        return false;

    if (!pmm_is_reserved((void*)phys))
        if (!pmm_reserve_frame((void*)phys))
            return false;

    memcpy(&pt_addr->entries[PT_INDEX(virt)], &flags, sizeof(page_entry_t));
    pt_addr->entries[PT_INDEX(virt)].physical_addr = phys >> 12;

    return true;
}

bool_t vmm_unmap_page(uint64_t virt) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present)
        return false;
    pdpt_addr = (PDPTable*)((current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return false;
    pd_addr = (PageDirectory*)((pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return false;
    pt_addr = (PageTable*)((pd_addr->entries[PD_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pt_addr->entries[PT_INDEX(virt)].present)
        return false;

    uint64_t phys = pt_addr->entries[PT_INDEX(virt)].physical_addr << 12;
    pt_addr->entries[PT_INDEX(virt)].present = false;

    if (pmm_is_reserved((void*)phys))
        pmm_free_frame((void*)phys);

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");

    return true;
}

bool_t vmm_is_mapped(uint64_t virt) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present)
        return false;
    pdpt_addr = (PDPTable*)((current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return false;
    pd_addr = (PageDirectory*)((pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return false;
    pt_addr = (PageTable*)((pd_addr->entries[PD_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pt_addr->entries[PT_INDEX(virt)].present)
        return false;

    return true;
}

bool_t vmm_are_mapped(uint64_t start_virt, uint64_t end_virt) {
    for (uint64_t virt = start_virt; virt <= end_virt; virt += PAGE_SIZE) {
        if (!vmm_is_mapped(virt))
            return false;
    }

    return true;
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present)
        return null;
    pdpt_addr = (PDPTable*)((current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return null;
    pd_addr = (PageDirectory*)((pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return null;
    pt_addr = (PageTable*)((pd_addr->entries[PD_INDEX(virt)].physical_addr << 12) + hhdm_offset);

    if (!pt_addr->entries[PT_INDEX(virt)].present)
        return null;

    uint64_t phys = pt_addr->entries[PT_INDEX(virt)].physical_addr << 12;
    if (!pmm_is_reserved((void*)phys))
        return null;

    return phys;
}

void vmm_load_cpu() {
    uint64_t PML4_phys = (uint64_t)current_PML4 - hhdm_offset;
    asm volatile("mov %0, %%cr3" :: "r"(PML4_phys) : "memory");
}

bool_t vmm_alloc(uint64_t start_virt, uint64_t end_virt, uint64_t flags) {
    uint64_t amount = end_virt / PAGE_SIZE - start_virt / PAGE_SIZE + 1;
    uint64_t failed_i = amount;

    for (uint64_t i = 0; i < amount; i++) {
        uint64_t frame_addr = (uint64_t)pmm_alloc_frame();
        if (frame_addr == null) {
            failed_i = i;
            break;
        }

        bool_t success = vmm_map_page(start_virt + i * PAGE_SIZE, frame_addr, flags);
        if (!success) {
            pmm_free_frame((void*)(frame_addr));
            failed_i = i;
            break;
        }
    }

    if (failed_i < amount) {
        for (uint64_t i = 0; i < failed_i; i++)
            vmm_unmap_page(start_virt + i * PAGE_SIZE);
        return false;
    }

    return true;
}

void vmm_free(uint64_t start_virt, uint64_t end_virt) {
    uint64_t amount = end_virt / PAGE_SIZE - start_virt / PAGE_SIZE + 1;
    for (uint64_t i = 0; i < amount; i++)
        vmm_unmap_page(start_virt + i * PAGE_SIZE);
}