#include "vmm.h"

#include "pmm.h"
#include "eitan_lib.h"
#include "screen.h"

#define PAGE_SIZE 4096

#define PML4_INDEX(virt)  (((virt) >> 39) & 0x1FF)
#define PDPT_INDEX(virt)  (((virt) >> 30) & 0x1FF)
#define PD_INDEX(virt)    (((virt) >> 21) & 0x1FF)
#define PT_INDEX(virt)    (((virt) >> 12) & 0x1FF)

extern uint8_t kernel_end[];
#define KERNEL_START 0xffffffff80000000
#define KERNEL_END ((uint64_t)(uint8_t*)kernel_end)

PML4Table* current_PML4;

bool_t create_PML4(PML4Table** PML4_ptr) {
    void* addr = pmm_alloc_frame();
    if (addr == null)
        return false;

    memset(addr, 0, PAGE_SIZE);
    *PML4_ptr = (PML4Table*)addr;
    return true;
}

PML4Table* vmm_init(volatile struct limine_hhdm_request* hhdm_request) {
    PML4Table* kernel_PML4 = null;
    bool_t success = create_PML4(&kernel_PML4);

    if (!success) {
        screen_print("[vmm] vmm init failed: can't create kernel PML4\n");
        return null;
    }
    screen_print("[vmm] created kernel PML4\n");

    vmm_set_PML4(kernel_PML4);

    for (uint64_t virt = KERNEL_START; virt < KERNEL_END; virt += PAGE_SIZE) {
        uint64_t phys = virt - hhdm_request->response->offset;
        success = vmm_map_page(virt, phys, VMM_FLAGS_KERNEL_ALL);

        if (!success) {
            screen_print("[vmm] vmm init failed: can't page kernel\n");
            return null;
        }
    }
    screen_print("[vmm] paged kernel\n");

    uint64_t pmm_virt = (uint64_t)pmm_get_bitmap();
    for (uint64_t virt = pmm_virt; virt < pmm_virt + pmm_get_bitmap_size(); virt += PAGE_SIZE) {
        uint64_t phys = virt - hhdm_request->response->offset;
        success = vmm_map_page(virt, phys, VMM_FLAGS_KERNEL_RW);

        if (!success) {
            screen_print("[vmm] vmm init failed: can't page pmm bitmap\n");
            return null;
        }
    }
    screen_print("[vmm] paged pmm bitmap\n");

    success = vmm_map_page((uint64_t)kernel_PML4, (uint64_t)kernel_PML4 - hhdm_request->response->offset, VMM_FLAGS_KERNEL_RW);
    if (!success) {
        screen_print("[vmm] vmm init failed: can't page PML4\n");
        return null;
    }

    screen_print("[vmm] paged kernel PML4\n");

    vmm_load_cpu();
    screen_print("[vmm] loaded new kernel PML4 to cpu\n");

    return kernel_PML4;
}

void vmm_set_PML4(PML4Table* PML4) {
    current_PML4 = PML4;
}

bool_t vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present) {
        pdpt_addr = (PDPTable*)pmm_alloc_frame();
        if (pdpt_addr == null)
            return false;
        memset(pdpt_addr, 0, PAGE_SIZE);

        uint64_t table_flags = VMM_FLAGS_TABLE;
        memcpy(&current_PML4->entries[PML4_INDEX(virt)], &table_flags, sizeof(page_entry_t));
        current_PML4->entries[PML4_INDEX(virt)].physical_addr = (uint64_t)pdpt_addr >> 12;
    } else {
        pdpt_addr = (PDPTable*)(current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12);
    }

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present) {
        pd_addr = (PageDirectory*)pmm_alloc_frame();
        if (pd_addr == null)
            return false;
        memset(pd_addr, 0, PAGE_SIZE);

        uint64_t table_flags = VMM_FLAGS_TABLE;
        memcpy(&pdpt_addr->entries[PDPT_INDEX(virt)], &table_flags, sizeof(page_entry_t));
        pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr = (uint64_t)pd_addr >> 12;
    } else {
        pd_addr = (PageDirectory*)(pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12);
    }

    if (!pd_addr->entries[PD_INDEX(virt)].present) {
        pt_addr = (PageTable*)pmm_alloc_frame();
        if (pt_addr == null)
            return false;
        memset(pt_addr, 0, PAGE_SIZE);

        uint64_t table_flags = VMM_FLAGS_TABLE;
        memcpy(&pd_addr->entries[PD_INDEX(virt)], &table_flags, sizeof(page_entry_t));
        pd_addr->entries[PD_INDEX(virt)].physical_addr = (uint64_t)pt_addr >> 12;
    } else {
        pt_addr = (PageTable*)(pd_addr->entries[PD_INDEX(virt)].physical_addr << 12);
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
    pdpt_addr = (PDPTable*)(current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return false;
    pd_addr = (PageDirectory*)(pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return false;
    pt_addr = (PageTable*)(pd_addr->entries[PD_INDEX(virt)].physical_addr << 12);

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
    pdpt_addr = (PDPTable*)(current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return false;
    pd_addr = (PageDirectory*)(pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return false;
    pt_addr = (PageTable*)(pd_addr->entries[PD_INDEX(virt)].physical_addr << 12);

    if (!pt_addr->entries[PT_INDEX(virt)].present)
        return false;

    return true;
}

bool_t vmm_can_map(uint64_t virt, uint64_t phys) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present)
        return false;
    pdpt_addr = (PDPTable*)(current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return false;
    pd_addr = (PageDirectory*)(pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return false;
    pt_addr = (PageTable*)(pd_addr->entries[PD_INDEX(virt)].physical_addr << 12);

    if (pt_addr->entries[PT_INDEX(virt)].present)
        return false;

    if (pmm_is_reserved((void*)phys))
        return false;

    return true;
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    PDPTable* pdpt_addr;
    PageDirectory* pd_addr;
    PageTable* pt_addr;

    if (!current_PML4->entries[PML4_INDEX(virt)].present)
        return null;
    pdpt_addr = (PDPTable*)(current_PML4->entries[PML4_INDEX(virt)].physical_addr << 12);

    if (!pdpt_addr->entries[PDPT_INDEX(virt)].present)
        return null;
    pd_addr = (PageDirectory*)(pdpt_addr->entries[PDPT_INDEX(virt)].physical_addr << 12);

    if (!pd_addr->entries[PD_INDEX(virt)].present)
        return null;
    pt_addr = (PageTable*)(pd_addr->entries[PD_INDEX(virt)].physical_addr << 12);

    if (!pt_addr->entries[PT_INDEX(virt)].present)
        return null;

    uint64_t phys = pt_addr->entries[PT_INDEX(virt)].physical_addr << 12;
    if (!pmm_is_reserved((void*)phys))
        return null;

    return phys;
}

void vmm_load_cpu() {
    asm volatile("mov %0, %%cr3" :: "r"((uint64_t)current_PML4) : "memory");
}

bool_t vmm_alloc(void* virt, uint64_t amount, uint64_t flags) {
    uint64_t failed_i = amount;

    for (uint64_t i = 0; i < amount; i++) {
        void* frame_addr = pmm_alloc_frame();
        if (frame_addr == null) {
            failed_i = i;
            break;
        }

        bool_t success = vmm_map_page((uint64_t)virt + i * PAGE_SIZE, (uint64_t)frame_addr, flags);
        if (!success) {
            pmm_free_frame(frame_addr);
            failed_i = i;
            break;
        }
    }

    if (failed_i < amount) {
        for (uint64_t i = 0; i < failed_i; i++)
            vmm_unmap_page((uint64_t)virt + i * PAGE_SIZE);
        return false;
    }

    return true;
}

void vmm_free(void* virt, uint64_t amount) {
    for (uint64_t i = 0; i < amount; i++)
        vmm_unmap_page((uint64_t)virt + i * PAGE_SIZE);
}