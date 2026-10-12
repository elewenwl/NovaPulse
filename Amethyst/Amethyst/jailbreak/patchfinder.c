#include "jailbreak.h"
#include "utils.h"
#include "patchfinder.h"

static kpf_ctx_t *ctx = NULL;

static CFNumberRef CFNUM(uint32_t value) {
    return CFNumberCreate(NULL, kCFNumberIntType, (void *)&value);
}

void kpf_deinit(void) {
    if (ctx == NULL) return;
    if (ctx->data != NULL) free(ctx->data);
    ctx = NULL;
    free(ctx);
}

int kpf_init(void) {
    ctx = calloc(1, sizeof(kpf_ctx_t));
    if (ctx == NULL) return -1;

    struct mach_header_64 *hdr = calloc(1, 0x4000);
    if (hdr == NULL) {
        kpf_deinit();
        return -1;
    }
    
    kread_buf(kinfo->kernel_base, hdr, 0x4000);
    if (hdr->magic != MH_MAGIC_64) {
        kpf_deinit();
        free(hdr);
        return -1;
    }
    
    uint64_t ppl_data_start = 0;
    uint64_t ppl_data_end = 0;
    uint64_t boot_data_start = 0;
    uint64_t boot_data_end = 0;
    uint64_t kld_start = 0;
    uint64_t kld_end = 0;
    uint64_t prelink_info_start = 0;
    uint64_t prelink_info_end = 0;
    uint64_t linkedit_start = 0;
    uint64_t linkedit_end = 0;

    struct load_command *load_cmd = (struct load_command *)(hdr + 1);
    uint64_t min_va = -1;
    uint64_t max_va = 0;
    
    for (uint32_t i = 0; i < hdr->ncmds; i++) {
        if (load_cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *segment = (struct segment_command_64 *)load_cmd;
            if (segment->vmsize > 0) {
                if (min_va > segment->vmaddr) min_va = segment->vmaddr;
                if (max_va < segment->vmaddr + segment->vmsize) max_va = segment->vmaddr + segment->vmsize;
            }

            if (strcmp(segment->segname, "__TEXT_EXEC") == 0) {
                ctx->section.text.base = segment->vmaddr;
                ctx->section.text.size = segment->filesize;
            } else if (strcmp(segment->segname, "__PLK_TEXT_EXEC") == 0) {
                ctx->section.prelink_text.base = segment->vmaddr;
                ctx->section.prelink_text.size = segment->filesize;
            } else if (strcmp(segment->segname, "__PRELINK_TEXT") == 0) {
                struct section_64 *section = (struct section_64 *)(segment + 1);
                ctx->prelink_offset = segment->fileoff;
                ctx->prelink_size = (uint32_t)segment->filesize;

                for (uint32_t j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__text") == 0) {
                        ctx->section.prelink_string.base = section[j].addr;
                        ctx->section.prelink_string.size = section[j].size;
                        break;
                    }
                }
            } else if (strcmp(segment->segname, "__DATA") == 0) {
                struct section_64 *section = (struct section_64 *)(segment + 1);
                for (uint32_t j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__bss") == 0) {
                        ctx->section.bss.base = section[j].addr;
                        ctx->section.bss.size = section[j].size;
                        break;
                    }
                }
            } else if (strcmp(segment->segname, "__TEXT") == 0) {
                struct section_64 *section = (struct section_64 *)(segment + 1);
                for (uint32_t j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__cstring") == 0) {
                        ctx->section.cstring.base = section[j].addr;
                        ctx->section.cstring.size = section[j].size;
                    } else if (strcmp(section[j].sectname, "__os_log") == 0) {
                        ctx->section.os_log.base = section[j].addr;
                        ctx->section.os_log.size = section[j].size;
                    }
                }
            } else if (strcmp(segment->segname, "__PPLTEXT") == 0) {
                struct section_64 *section = (struct section_64 *)(segment + 1);
                for (uint32_t j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__text") == 0) {
                        ctx->section.ppl_text.base = section[j].addr;
                        ctx->section.ppl_text.size = section[j].size;
                    }
                }
            } else if (strcmp(segment->segname, "__PPLDATA") == 0) {
                ppl_data_start = segment->vmaddr;
                ppl_data_end = ppl_data_start + segment->filesize;
            } else if (strcmp(segment->segname, "__KLD") == 0) {
                kld_start = segment->vmaddr;
                kld_end = kld_start + segment->filesize;
            } else if (strcmp(segment->segname, "__BOOTDATA") == 0) {
                boot_data_start = segment->vmaddr;
                boot_data_end = boot_data_start + segment->filesize;
            } else if (strcmp(segment->segname, "__PRELINK_INFO") == 0) {
                prelink_info_start = segment->vmaddr;
                prelink_info_end = prelink_info_start + segment->filesize;
            } else if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                linkedit_start = segment->vmaddr;
                linkedit_end = linkedit_start + segment->filesize;
            }
        } else if (load_cmd->cmd == LC_UNIXTHREAD) {
              uint32_t *ptr = (uint32_t *)(load_cmd + 1);
              uint32_t flavor = ptr[0];
              struct {
                  uint64_t x[29];
                  uint64_t fp;
                  uint64_t lr;
                  uint64_t sp;
                  uint64_t pc;
                  uint32_t cpsr;
              } *thread = (void *)(ptr + 2);
              if (flavor == 6) {
                  if (thread->pc != 0) {
                      ctx->kernel_entry = thread->pc + kinfo->kernel_slide;
                  }
              }
          
        }
        load_cmd = (struct load_command *)((uintptr_t)load_cmd + load_cmd->cmdsize);
    }
    
    free(hdr);
    if (ctx->section.prelink_string.size == 0) {
        ctx->section.prelink_string.base = ctx->section.cstring.base;
        ctx->section.prelink_string.size = ctx->section.cstring.size;
    }
    
    if (ctx->section.prelink_text.size == 0) {
        ctx->section.prelink_text.base = ctx->section.text.base;
        ctx->section.prelink_text.size = ctx->section.text.size;
    }
    
    if (ctx->section.ppl_text.size == 0) {
        ctx->section.ppl_text.base = ctx->section.text.base;
        ctx->section.ppl_text.size = ctx->section.text.size;
    }
    
    min_va = (min_va + (kinfo->vm_page_size-1)) & ~((uint64_t)kinfo->vm_page_size-1);
    max_va = (max_va + (kinfo->vm_page_size-1)) & ~((uint64_t)kinfo->vm_page_size-1) - kinfo->vm_page_size;
    ctx->search_base = min_va;
    ctx->search_size = (uint32_t)(max_va - min_va);

    if (ctx->section.text.base != 0) ctx->section.text.base -= ctx->search_base;
    if (ctx->section.cstring.base != 0) ctx->section.cstring.base -= ctx->search_base;
    if (ctx->section.os_log.base != 0) ctx->section.os_log.base -= ctx->search_base;
    if (ctx->section.bss.base != 0) ctx->section.bss.base -= ctx->search_base;
    if (ctx->section.prelink_text.base != 0) ctx->section.prelink_text.base -= ctx->search_base;
    if (ctx->section.prelink_string.base != 0) ctx->section.prelink_string.base -= ctx->search_base;
    if (ctx->section.ppl_text.base != 0) ctx->section.ppl_text.base -= ctx->search_base;

    ctx->data = calloc(1, ctx->search_size);
    if (ctx->data == NULL) return -1;
    
    if (ppl_data_start == 0) {
        kread_buf(ctx->search_base, ctx->data, ctx->search_size);
    } else {
        for (uint64_t offset = 0; offset < ctx->search_size; offset+=kinfo->vm_page_size) {
            uint64_t min_addr = ctx->search_base + offset;
            uint64_t max_addr = ctx->search_base + offset + kinfo->vm_page_size;
            
            if (((min_addr < ppl_data_end) && (max_addr > ppl_data_start)) ||
                ((min_addr < boot_data_end) && (max_addr > boot_data_start)) ||
                ((min_addr < prelink_info_end) && (max_addr > prelink_info_start)) ||
                ((min_addr < linkedit_end) && (max_addr > linkedit_start)) ||
                ((min_addr < kld_end) && (max_addr > kld_start))) {
            } else {
                kread_buf(ctx->search_base + offset, ctx->data + offset, kinfo->vm_page_size);
            }
        }
    }

    ctx->hdr = (struct mach_header_64 *)(ctx->data + (kinfo->kernel_base - ctx->search_base));
    return 0;
}

static uint8_t *kpf_memmem(uint8_t *data, uint32_t data_len, uint8_t *target, uint32_t target_len) {
    uint32_t last = 0;
    uint32_t scan = 0;
    size_t skip[256] = {0};
    
    for (scan = 0; scan <= 255; scan = scan + 1) skip[scan] = target_len;
    last = target_len - 1;
    for (scan = 0; scan < last; scan = scan + 1) skip[target[scan]] = last - scan;

    while (data_len >= target_len) {
        for (scan = last; data[scan] == target[scan]; scan = scan - 1) {
            if (scan == 0) return (uint8_t *)data;
        }

        data_len -= skip[data[last]];
        data += skip[data[last]];
    }
    return NULL;
}

static uint64_t kpf_find_ref(uint64_t to, kernel_section_t *section) {
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    
    if (section != NULL) {
        start_addr = section->base;
        end_addr = start_addr + section->size;
    }
    
    if (start_addr == 0) {
        start_addr = ctx->section.text.base;
        end_addr = start_addr + ctx->section.text.size;
    }
    
    end_addr &= ~0x3;
    to -= ctx->search_base;
    uint64_t ref_addr = 0;
    
    for (uint32_t i = 0; i < 2; i++) {
        uint64_t current_addr = start_addr & ~0x3;
        uint64_t state[32] = {0};
        ref_addr = 0;
        
        for (; current_addr < end_addr; current_addr += 0x4) {
            uint32_t current_inst = *(uint32_t *)(ctx->data + current_addr);
            uint32_t current_reg = current_inst & 0x1f;
            
            if ((current_inst & A64_ADRP_MASK) == A64_ADRP_OPCODE) {
                int32_t adr = ((current_inst & 0x60000000) >> 18) | ((current_inst & 0xFFFFE0) << 8);
                state[current_reg] = ((int64_t)adr << 1) + (current_addr & ~0xFFF);
                continue;
            } else if ((current_inst & A64_ADD_MASK) == A64_ADD_OPCODE) {
                uint32_t rn = (current_inst >> 5) & 0x1F;
                uint32_t shift = (current_inst >> 22) & 3;
                uint32_t imm = (current_inst >> 10) & 0xFFF;
                
                if (shift == 1) imm <<= 12;
                else if (shift > 1) continue;
                state[current_reg] = state[rn] + imm;
            } else if (i != 0 && ((current_inst & 0xF9C00000) == 0xF9400000)) {
              //  uint32_t rn = (current_inst >> 5) & 0x1F;
              //  uint32_t imm = ((current_inst >> 10) & 0xFFF) << 3;
              //  if (!imm) continue;
              //  state[current_reg] = state[rn] + imm;
            } else if ((current_inst & A64_ADR_MASK) == A64_ADR_OPCODE) {
                int32_t adr = ((current_inst & 0x60000000) >> 18) | ((current_inst & 0xFFFFE0) << 8);
                state[current_reg] = ((int64_t)adr >> 11) + current_addr;
            } else if (i != 0 && ((current_inst & A64_LDR_X_LITERAL_MASK) == A64_LDR_X_LITERAL_OPCODE)) {
             //   uint32_t adr = (current_inst & 0xFFFFE0) >> 3;
             //   state[current_reg] = adr + current_addr;
            }
            
            if (state[current_reg] == to) {
                ref_addr = current_addr;
                break;
            }
        }
    }

    if (ref_addr == 0) return 0;
    return ref_addr + ctx->search_base;
}

static uint64_t kpf_register_value(uint64_t start, uint32_t size, uint32_t reg) {
    uint64_t start_addr = start - ctx->search_base;
    uint64_t end_addr = (start_addr + size) & ~0x3;
    uint64_t current_addr = start_addr & ~0x3;
    uint64_t state[32] = {0};
    
    for (; current_addr < end_addr; current_addr += 0x4) {
        uint32_t current_inst = *(uint32_t *)(ctx->data + current_addr);
        uint32_t current_reg = current_inst & 0x1f;
        
        if ((current_inst & A64_ADRP_MASK) == A64_ADRP_OPCODE) {
            int32_t adr = ((current_inst & 0x60000000) >> 18) | ((current_inst & 0xFFFFE0) << 8);
            state[current_reg] = ((int64_t)adr << 1) + (current_addr & ~0xFFF);
        } else if ((current_inst & A64_ADD_MASK) == A64_ADD_OPCODE) {
            uint32_t rn = (current_inst >> 5) & 0x1F;
            uint32_t shift = (current_inst >> 22) & 3;
            uint32_t imm = (current_inst >> 10) & 0xFFF;
            
            if (shift == 1) imm <<= 12;
            else if (shift > 1) continue;
            state[current_reg] = state[rn] + imm;
        } else if ((current_inst & 0xF9C00000) == 0xF9400000) {
            uint32_t rn = (current_inst >> 5) & 0x1F;
            uint32_t imm = ((current_inst >> 10) & 0xFFF) << 3;
            if (!imm) continue;
            state[current_reg] = state[rn] + imm;
        } else if ((current_inst & 0xF9C00000) == 0xF9000000) {
            uint32_t rn = (current_inst >> 5) & 0x1F;
            uint32_t imm = ((current_inst >> 10) & 0xFFF) << 3;
            if (!imm) continue;
            state[rn] = state[rn] + imm;
        } else if ((current_inst & A64_ADR_MASK) == A64_ADR_OPCODE) {
            int32_t adr = ((current_inst & 0x60000000) >> 18) | ((current_inst & 0xFFFFE0) << 8);
            state[current_reg] = ((int64_t)adr >> 11) + current_addr;
        } else if ((current_inst & A64_LDR_X_LITERAL_MASK) == A64_LDR_X_LITERAL_OPCODE) {
            uint32_t adr = (current_inst & 0xFFFFE0) >> 3;
            state[current_reg] = adr + current_addr;
        }
    }
    
    uint64_t value = state[reg];
    if (value == 0) return 0;
    return value + ctx->search_base;
}

static uint64_t kpf_find_str(const char *str, kernel_section_t *section) {
    uint64_t start = 0;
    uint32_t size = 0;
    
    if (section != NULL) {
        start = (uint64_t)ctx->data + section->base;
        size = (uint32_t)section->size;
    }
    
    if (start == 0) {
        start = (uint64_t)ctx->data;
        size = ctx->search_size;
    }
    
    uint8_t *loc = kpf_memmem((uint8_t *)start, size, (uint8_t *)str, (uint32_t)strlen(str));
    if (loc == NULL) return 0;
    return ((uint64_t)loc - (uint64_t)ctx->data) + ctx->search_base;
}

static uint64_t kpf_find_strref(const char *str, kernel_section_t *string_section, kernel_section_t *text_section) {
    uint64_t str_addr = 0;
    uint64_t text_addr = 0;

    if (string_section == NULL) {
        if (ctx->section.cstring.size >= 0) str_addr = kpf_find_str(str, &ctx->section.cstring);
        if (str_addr == 0 && ctx->section.prelink_string.size >= 0) str_addr = kpf_find_str(str, &ctx->section.prelink_string);
        if (str_addr == 0 && ctx->section.os_log.size >= 0) str_addr = kpf_find_str(str, &ctx->section.os_log);
        if (str_addr == 0) str_addr = kpf_find_str(str, NULL);
    } else {
        str_addr = kpf_find_str(str, string_section);
    }
    
    if (str_addr == 0) return 0;
    if (text_section == NULL) {
        if (ctx->section.text.size >= 0) text_addr = kpf_find_ref(str_addr, &ctx->section.text);
        if (text_addr == 0 && ctx->section.prelink_text.size >= 0) text_addr = kpf_find_ref(str_addr, &ctx->section.prelink_text);
        if (text_addr == 0 && ctx->section.ppl_text.size >= 0) text_addr = kpf_find_ref(str_addr, &ctx->section.ppl_text);
    } else {
        text_addr = kpf_find_ref(str_addr, text_section);
    }
    return text_addr;
}

static uint64_t kpf_find_next(uint64_t start, uint32_t size, uint32_t opcode, uint32_t mask) {
    uint64_t start_addr = (uint64_t)ctx->data + (start - ctx->search_base);
    uint64_t end_addr = start_addr + size;
    
    while (start_addr < end_addr) {
        uint32_t value = *(uint32_t *)start_addr;
        if ((value & mask) == opcode) {
            return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
        }
        start_addr += 0x4;
    }
    return 0;
}

static uint64_t kpf_find_prev(uint64_t start, uint32_t size, uint32_t opcode, uint32_t mask) {
    uint64_t start_addr = (uint64_t)ctx->data + (start - ctx->search_base);
    uint64_t end_addr = start_addr - size;
    
    while (start_addr >= end_addr) {
        uint32_t value = *(uint32_t *)start_addr;
        if ((value & mask) == opcode) {
            return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
        }
        start_addr -= 0x4;
    }
    return 0;
}

static uint64_t kpf_find_prev_branch(uint64_t start, uint32_t size, bool link_only) {
    uint64_t start_addr = (uint64_t)ctx->data + (start - ctx->search_base);
    uint64_t end_addr = start_addr - size;
    
    while (start_addr >= end_addr) {
        uint32_t value = *(uint32_t *)start_addr;
        if (link_only) {
            if (((value & A64_BL_MASK) == A64_BL_OPCODE)) {
                return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
            }
        } else {
            if (A64_IS_BRANCH(value)) {
                return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
            }
        }
        start_addr -= 0x4;
    }
    return 0;
}

static uint64_t kpf_find_next_branch(uint64_t start, uint32_t size, bool link_only) {
    uint64_t start_addr = (uint64_t)ctx->data + (start - ctx->search_base);
    uint64_t end_addr = start_addr + size;
    
    while (start_addr < end_addr) {
        uint32_t value = *(uint32_t *)start_addr;
        if (link_only) {
            if (((value & A64_BL_MASK) == A64_BL_OPCODE)) {
                return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
            }
        } else {
            if (A64_IS_BRANCH(value)) {
                return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
            }
        }
        start_addr += 0x4;
    }
    return 0;
}

static uint64_t kpf_decode_branch(uint64_t addr) {
    uint32_t inst = *(uint32_t *)((uint64_t)ctx->data + (addr - ctx->search_base));
    if ((inst & A64_B_MASK) == A64_B_OPCODE || (inst & A64_BL_MASK) == A64_BL_OPCODE) {
        return addr + ((((int64_t)((uint64_t)(inst & 0x3ffffff) << 38)) >> 38) * 0x4);
    } else if ((inst & A64_B_COND_MASK) == A64_B_COND_OPCODE) {
        return addr + ((((int64_t)((uint64_t)((inst >> 5) & 0x7ffff) << 45)) >> 45) * 0x4);
    }
    return 0;
}

static uint64_t kpf_decode_adrp_ldr_str(uint64_t addr, uint8_t shift) {
    uint64_t local_addr = addr - ctx->search_base;
    uint32_t adrp_op = *(uint32_t *)(ctx->data + local_addr);
    uint32_t ldr_str_op = *(uint32_t *)(ctx->data + local_addr + 0x4);

    int32_t adr_imm = ((adrp_op & 0x60000000) >> 18) | ((adrp_op & 0xFFFFE0) << 8);
    uint64_t page = ((uint64_t)adr_imm << 1) + (local_addr & ~0xFFF);
    uint64_t pageoff = (uint64_t)(((ldr_str_op >> 10) & 0xFFF) << shift);
    if (page == 0 && pageoff == 0) return 0;
    return page + pageoff + ctx->search_base;
}

static uint64_t kpf_find_func_start(uint64_t addr, uint32_t size) {
    uint64_t start_addr = (uint64_t)ctx->data + (addr - ctx->search_base);
    uint64_t end_addr = start_addr - size;
    
    for (; start_addr >= end_addr; start_addr -= 4) {
        uint32_t value = *(uint32_t *)start_addr;
        
        if ((value & 0xFFC003FF) == 0x910003FD) {
            uint32_t delta = (value >> 10) & 0xFFF;
            
            if ((delta & 0xF) == 0) {
                uint32_t au = *(uint32_t *)(start_addr - ((delta >> 4) + 1) * 4);
                if ((au & 0xFFC003E0) == 0xA98003E0) {
                    return ((start_addr - ((delta >> 4) + 1) * 4) - (uint64_t)ctx->data) + ctx->search_base;
                }
                
                while (start_addr > end_addr) {
                    start_addr -= 4;
                    au = *(uint32_t *)start_addr;
                    if ((au & 0xFFC003FF) == 0xD10003FF && ((au >> 10) & 0xFFF) == delta + 0x10) return (start_addr - (uint64_t)ctx->data) + ctx->search_base;
                    if ((au & 0xFFC003E0) != 0xA90003E0) {
                        start_addr += 4;
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

uint64_t kpf_find_dynamic_trustcache(void) {
    if (kinfo->version[0] == 12) {
        uint64_t ref = kpf_find_strref("\"loadable trust cache buffer too small (%ld) for entries claimed (%d)\"", NULL, NULL);
        if (ref != 0) {
            if (!kinfo->protections.ppl) {
                uint64_t addr = kpf_register_value(ref-0x30, 0x10, 8);
                if (addr != 0) return addr;
            }
            
            uint64_t adrp = kpf_find_prev(ref-0x8, 0x100, A64_ADRP_OPCODE, A64_ADRP_MASK);
            if (adrp != 0) {
                uint64_t target = kpf_decode_adrp_ldr_str(adrp, 3);
                if (target != 0) return target;
            }
        }
    
        ref = kpf_find_strref("%s: only allowed process can check the trust cache", NULL, NULL);
        if (ref == 0) return 0;
        
        uint64_t branch = kpf_find_prev_branch(ref, 0x30, true);
        if (branch == 0) return 0;
        
        uint64_t func = kpf_decode_branch(branch);
        if (func == 0) return 0;
        
        branch = kpf_find_next_branch(func, 0x30, true);
        if (branch == 0) return 0;
        
        branch = kpf_find_next_branch(branch+0x4, 0x30, true);
        if (branch == 0) return 0;
        
        func = kpf_decode_branch(branch);
        if (func == 0) return 0;
        
        branch = kpf_find_next_branch(func, 0x40, true);
        if (branch == 0) return 0;
        return kpf_register_value(branch, 0x18, 21);
    } else {
        uint64_t ref = kpf_find_strref("\"loadable trust cache overflow: %p/%ld\"", NULL, NULL);
        if (ref == 0) return 0;
        
        if (kinfo->protections.pac) {
            uint64_t tpidr_el1 = kpf_find_prev(ref, 0x100, 0xD538D088, A64_ALL_MASK); // mrs x8, TPIDR_EL1
            if (tpidr_el1 == 0) return 0;
            
            uint64_t retab = kpf_find_prev(tpidr_el1-0x4, 0x400, A64_RETAB, A64_ALL_MASK);
            if (retab == 0) return 0;
            
            uint64_t adrp = kpf_find_next(retab+0x8, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
            if (adrp == 0) return 0;
            return kpf_register_value(adrp, 0x8, 8);
        } else {
            uint64_t tpidr_el1 = kpf_find_prev(ref, 0x200, 0xD538D088, A64_ALL_MASK); // mrs x8, TPIDR_EL1
            if (tpidr_el1 == 0) goto fallback;
            
            uint64_t wfe = kpf_find_prev(tpidr_el1-0x4, 0x400, 0xD503205F, A64_ALL_MASK); // wfe
            if (wfe == 0) goto fallback;
            
            uint64_t branch = kpf_find_next_branch(wfe+0x8, 0x80, true);
            if (branch == 0) goto fallback;
            
            uint64_t adrp = kpf_find_next(branch+0x4, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
            if (adrp == 0) goto fallback;
            
            uint64_t addr = kpf_register_value(adrp, 0x8, 8);
            if (addr != 0) return addr;
        
fallback:
            {
                uint64_t mov = kpf_find_prev(ref, 0x200, 0xD2800002, A64_ALL_MASK); // mov x2, #0
                if (mov == 0) return 0;
                
                adrp = kpf_find_next(mov+0x4, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
                if (adrp == 0) return 0;
                return kpf_decode_adrp_ldr_str(adrp, 3);
            }
        }
    }
    return 0;
}

uint64_t kpf_find_static_trustcache(void) {
    uint64_t ref = kpf_find_strref("chosen/memory-map", NULL, NULL);
    if (ref == 0) return 0;
    
    uint64_t adrp = kpf_find_prev(ref-0x10, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    
    if (kinfo->version[0] == 13) {
        adrp = kpf_find_prev(adrp-0x4, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
        if (adrp == 0) return 0;
        
        adrp = kpf_find_prev(adrp-0x4, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
        if (adrp == 0) return 0;
    }
    
    uint64_t segEXTRADATA = kpf_decode_adrp_ldr_str(adrp, 3);
    if (segEXTRADATA == 0) return 0;

    uint64_t addr = kread64(segEXTRADATA);
    if ((addr & 0xfffffff000000fff) != 0xfffffff000000000) return 0;
    if (kread32(addr) != 1) return 0;
    return addr + 0x8;
}

uint64_t kpf_find_ptov_table(void) {
    uint64_t ref = kpf_find_strref("\"ml_static_vtop(): illegal VA:", NULL, NULL);
    if (ref != 0) {
        uint64_t start = kpf_find_func_start(ref, 0x400);
        if (start != 0) return kpf_register_value(start, 0x30, 8);
    }
    
    ref = kpf_find_strref("\"%s: cpte=%#llx is not empty, \" \"vaddr=%#lx, pte=%#llx\"", NULL, NULL);
    if (ref == 0) goto fallback;
    
    uint64_t tlbi = kpf_find_prev(ref, 0x800, 0xD508831F, A64_ALL_MASK); // TLBI VMALLE1IS
    if (tlbi == 0) goto fallback;

    uint64_t branch = kpf_find_prev_branch(tlbi - 0x30, 0x80, true);
    if (branch == 0) goto fallback;

    uint64_t adrp = kpf_find_next(branch, 0x30, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) goto fallback;

    uint64_t addr = kpf_register_value(adrp, 0x8, 8);
    if (addr == 0) goto fallback;
    
    uint64_t test_pa = kread64(addr);
    uint64_t test_va = kread64(addr + 0x8);
    uint64_t test_len = kread64(addr + 0x10);
    if ((test_pa & 0x800000000) == 0x800000000 && (test_va & 0xffff000000000000) == 0xffff000000000000) {
        if (test_len > 0 && test_len < 0xffffff000) return addr;
    }
    
fallback:
    ref = kpf_find_strref("\"Unsupported memory configuration %lx\\n\"", NULL, NULL);
    if (ref == 0) return 0;
    
    tlbi = kpf_find_prev(ref, 0x800, 0xD508831F, A64_ALL_MASK); // TLBI VMALLE1IS
    if (tlbi == 0) return 0;
    
    uint64_t stp = kpf_find_prev(tlbi, 0x80, 0xAD000000, 0xff000000); // stp q?, q?, ...
    if (stp == 0) return 0;

    adrp = kpf_find_prev(stp, 0x80, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    return kpf_register_value(adrp, 0x8, 8);
}

uint64_t kpf_find_pidhash_ptr(void) {
    uint64_t ref = kpf_find_strref("sigrestrict", NULL, NULL);
    if (ref == 0) {
        if (kinfo->version[0] >= 13) {
            ref = kpf_find_strref("-no_vnode_drain", NULL, NULL);
        } else {
            ref = kpf_find_strref("\"startup: kauth_cred_init\"", NULL, NULL);
        }
    }
    if (ref == 0) return 0;

    uint64_t mov = 0;
    if (kinfo->version[0] >= 13) {
        uint64_t first_mov = kpf_find_next(ref, 0x100, 0x52800082, A64_ALL_MASK); // mov, w2, #0x4
        if (first_mov == 0) first_mov = kpf_find_next(ref, 0x100, 0x52800003, A64_ALL_MASK); // mov, w3, #0
        if (first_mov == 0) return 0;
        
        uint64_t branch = kpf_find_next_branch(first_mov, 0x40, true);
        for (uint32_t i = 0; i < 4; i++) {
            uint64_t func = kpf_decode_branch(branch);
            if (func == 0) continue;
            
            mov = kpf_find_next(func, 0x100, 0x52800521, A64_ALL_MASK); // mov, w1, #0x29
            if (mov != 0) break;
            
            branch = kpf_find_next_branch(branch+0x4, 0x20, true);
            if (branch == 0) break;
        }
    } else {
        mov = kpf_find_next(ref, 0x400, 0x52800521, A64_ALL_MASK); // mov, w1, #0x29
    }
    if (mov == 0) return 0;

    uint64_t cmp = kpf_find_prev(mov, 0x30, 0x7100011f, A64_ALL_MASK); // cmp, w8, #0
    if (cmp == 0) cmp = kpf_find_prev(mov, 0x30, 0x7100013f, A64_ALL_MASK); // cmp, w9, #0
    if (cmp == 0) return 0;
    
    uint64_t adrp = kpf_find_next(cmp, 0x80, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    return kpf_register_value(adrp, 0x8, 2);
}

uint64_t kpf_find_pidhashtbl_ptr(void) {
    uint64_t ref = kpf_find_strref("sigrestrict", NULL, NULL);
    if (ref == 0) {
        if (kinfo->version[0] >= 13) {
            ref = kpf_find_strref("-no_vnode_drain", NULL, NULL);
        } else {
            ref = kpf_find_strref("\"startup: kauth_cred_init\"", NULL, NULL);
        }
    }
    if (ref == 0) return 0;

    uint64_t mov = 0;
    if (kinfo->version[0] >= 13) {
        uint64_t first_mov = kpf_find_next(ref, 0x100, 0x52800082, A64_ALL_MASK); // mov, w2, #0x4
        if (first_mov == 0) first_mov = kpf_find_next(ref, 0x100, 0x52800003, A64_ALL_MASK); // mov, w3, #0
        if (first_mov == 0) return 0;
        
        uint64_t branch = kpf_find_next_branch(first_mov, 0x40, true);
        for (uint32_t i = 0; i < 4; i++) {
            uint64_t func = kpf_decode_branch(branch);
            if (func == 0) continue;
            
            mov = kpf_find_next(func, 0x100, 0x52800521, A64_ALL_MASK); // mov, w1, #0x29
            if (mov != 0) break;
            
            branch = kpf_find_next_branch(branch+0x4, 0x20, true);
            if (branch == 0) break;
        }
    } else {
        mov = kpf_find_next(ref, 0x400, 0x52800521, A64_ALL_MASK); // mov, w1, #0x29
    }
    if (mov == 0) return 0;

    uint64_t cmp = kpf_find_prev(mov, 0x30, 0x7100011f, A64_ALL_MASK); // cmp, w8, #0
    if (cmp == 0) cmp = kpf_find_prev(mov, 0x30, 0x7100013f, A64_ALL_MASK); // cmp, w9, #0
    if (cmp == 0) return 0;
    
    uint64_t adrp = kpf_find_next(cmp, 0x80, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    
    adrp = kpf_find_next(adrp+0x8, 0x40, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    return kpf_decode_adrp_ldr_str(adrp, 3);
}

uint64_t kpf_find_all_proc(void) {
    if (kinfo->kern_proc_addr == 0) return 0;
    uint64_t current_proc = kinfo->kern_proc_addr;
    uint64_t all_proc = 0;
    
    while (current_proc != 0) {
        if (kread64(kread64(current_proc + 0x8)) != current_proc) {
            if (current_proc > kinfo->kernel_base && current_proc <= (kinfo->kernel_base + 0x4000000)) {
                all_proc = current_proc;
                break;
            }
        }
        current_proc = kread64(current_proc + 0x8);
    }
    return all_proc;
}

uint64_t kpf_find_cpu_ttep_ptr(void) {
    if (ctx->kernel_entry == 0) return 0;
    uint64_t func = kpf_decode_branch(ctx->kernel_entry);
    if (func == 0) return 0;
    
    uint64_t mov = kpf_find_next(func, 0x600, 0xd2c08001, A64_ALL_MASK); // mov x1, #0x40000000000
    if (mov == 0) return 0;
    
    uint64_t adrp = kpf_find_next(mov, 0x80, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    return kpf_register_value(adrp, 0x8, 0);
}

uint64_t kpf_find_cpu_ttep(void) {
    uint64_t ptr = kpf_find_cpu_ttep_ptr();
    if (ptr != 0) {
        uint64_t cpu_ttep = kread64(ptr);
        if (cpu_ttep != 0) return cpu_ttep;
    }
    
    if (kinfo->kern_task_addr == 0) return 0;
    uint64_t kern_vm_map = kread64(kinfo->kern_task_addr + koffsetof(task, vm_map));
    if (kern_vm_map == 0) return 0;
    
    uint64_t kern_pmap = kread64(kern_vm_map + koffsetof(vm_map, pmap));
    if (kern_pmap == 0) return 0;
    return kread64(kern_pmap + koffsetof(pmap, ttep));
}

uint64_t kpf_find_self_ttep(void) {
    if (kinfo->self_task_addr == 0) return 0;
    uint64_t self_vm_map = kread64(kinfo->self_task_addr + koffsetof(task, vm_map));
    if (self_vm_map == 0) return 0;
    
    uint64_t self_pmap = kread64(self_vm_map + koffsetof(vm_map, pmap));
    if (self_pmap == 0) return 0;
    return kread64(self_pmap + koffsetof(pmap, ttep));
}

uint64_t kpf_find_pv_head_table_ptr(void) {
    uint64_t ref = 0;
    uint64_t start = 0;
    uint64_t adrp = 0;
    
    ref = kpf_find_strref("\"%s: unexpected PV head %p, pte_p=%p pmap=%p pv_h=%p\"", NULL, NULL);
    if (ref == 0) ref = kpf_find_strref("%s: unexpected PV head %p, ptep=%p pmap=%p pvh=%p pai=0x%x @%s:%d", NULL, NULL);
    if (ref == 0) goto fallback;
    
    start = kpf_find_func_start(ref, 0x800);
    if (start == 0) goto fallback;
    
    adrp = kpf_find_next(start, 0x180, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp != 0) goto decode;
    
fallback:
    ref = kpf_find_strref("pmap_iommu_ioctl_internal", NULL, NULL);
    if (ref == 0) ref = kpf_find_strref("\"%s: in_data wraps around\"", NULL, NULL);
    if (ref == 0) return 0;
    
    start = kpf_find_func_start(ref, 0x200);
    if (start == 0) return 0;
    
    adrp = kpf_find_next(start, 0x180, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;

decode:
    return kpf_decode_adrp_ldr_str(adrp, 3);
}

uint64_t kpf_find_pv_head_table(void) {
    uint64_t ptr = kpf_find_pv_head_table_ptr();
    if (ptr == 0) return 0;
    return kread64(ptr);
}

uint64_t kpf_find_pe_state(void) {
    uint64_t ref = kpf_find_strref("BBBBBBBBGGGGGGGGRRRRRRRR", NULL, NULL);
    if (ref == 0) goto fallback;
    
    uint64_t start = kpf_find_func_start(ref, 0x200);
    if (start == 0) goto fallback;

    uint64_t adrp = kpf_find_next(start, 0x160, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) goto fallback;
    
    uint64_t addr = kpf_decode_adrp_ldr_str(adrp, 2);
    if (addr != 0) return addr;
    
fallback:
    ref = kpf_find_strref("\"bsd_init: cannot find root vnode: %s\"", NULL, NULL);
    if (ref == 0) return 0;
    
    adrp = kpf_find_prev(ref-0x8, 0x80, A64_ADRP_OPCODE, A64_ADRP_MASK);
    if (adrp == 0) return 0;
    return kpf_register_value(adrp, 0x8, 8);
}

uint64_t kpf_find_bootargs(void) {
    if (kinfo->version[0] == 13) {
        uint64_t ref = kpf_find_strref("Boot args: %s\n", NULL, NULL);
        if (ref == 0) goto fallback;

        uint64_t adrp = kpf_find_prev(ref-0x8, 0x40, A64_ADRP_OPCODE, A64_ADRP_MASK);
        if (adrp == 0) goto fallback;
        
        uint64_t ptr = kpf_decode_adrp_ldr_str(adrp, 3);
        if (ptr == 0) goto fallback;
        return kread64(ptr);
    }

    
fallback:;
    uint64_t pe_state = kpf_find_pe_state();
    if (pe_state == 0) return 0;
    return kread64(pe_state + 0xa0);
}

uint64_t kpf_find_gfx_mapping_base(void) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    uint64_t oob_size = 0x4000 - 0x8000;
    memory_object_size_t mem_size = 0x20000;
    uint32_t width_height = 32;
    
    CFDictionarySetValue(dict, CFSTR("IOSurfacePixelFormat"), CFNUM((int)'ARGB'));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceWidth"), CFNUM(width_height));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceHeight"), CFNUM(width_height));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBufferTileMode"), kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerRow"), CFNUM(width_height*4));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerElement"), CFNUM(4));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceAllocSize"), CFNUM((uint32_t)mem_size));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);
    CFDictionarySetValue(dict, CFSTR("IOSurfaceMemoryRegion"), CFSTR("PurpleGfxMem"));
    
    void *surface = IOSurfaceCreate(dict);
    CFRelease(dict);
    if (surface == NULL) return -1;

    memory_object_offset_t base = (memory_object_offset_t)IOSurfaceGetBaseAddress(surface);
    memset((void *)base, 0x41, mem_size);
    
    mach_port_t main_entry = MACH_PORT_NULL;
    mach_port_t oob_entry = MACH_PORT_NULL;
    vm_prot_t prot = VM_PROT_DEFAULT;
    SET_MAP_MEM(MAP_MEM_IO, prot);
    
    mach_make_memory_entry_64(mach_task_self(), &mem_size, base, prot, &main_entry, 0);
    mach_make_memory_entry_64(mach_task_self(), &oob_size, 0x8000, prot, &oob_entry, main_entry);
    CFRelease(surface);
    
    prot = VM_PROT_READ | VM_PROT_WRITE;
    uint64_t mapped = 0;
    uint64_t mapping_base = 0;

    mach_vm_map(mach_task_self(), &mapped, kinfo->vm_page_size, 0, 1, oob_entry, 0, 0, prot, prot, 0);
    if (mapped != 0) {
        *(uint64_t *)mapped = 0x4242424242424242;
        asm volatile ("dsb sy");
        usleep(1000);
        
        mapping_base = vtophys(mapped);
        mach_vm_deallocate(mach_task_self(), mapped, kinfo->vm_page_size);
    }

    mach_port_deallocate(mach_task_self(), oob_entry);
    mach_port_deallocate(mach_task_self(), main_entry);
    return mapping_base;
}

