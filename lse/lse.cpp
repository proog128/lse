#include <stdio.h>
#include <windows.h>
#include <stdexcept>
#include <vector>
#include <io.h>
#include "elf.h"
#include "error.h"

bool IsELFBinary(const Elf32_Ehdr& header)
{
    return header.e_ident[EI_MAG0] == ELFMAG0 &&
           header.e_ident[EI_MAG1] == ELFMAG1 &&
           header.e_ident[EI_MAG2] == ELFMAG2 &&
           header.e_ident[EI_MAG3] == ELFMAG3 &&
           header.e_ident[EI_CLASS] == ELFCLASS32 &&
           header.e_ident[EI_DATA] == ELFDATA2LSB &&
           header.e_ident[EI_VERSION] == EV_CURRENT;
}

unsigned int Ceil64K(unsigned int addr)
{
    return (addr+0xFFFF)&~0xFFFF;
}

unsigned int Floor64K(unsigned int addr)
{
    return Ceil64K(addr)-0x10000;
}

unsigned int LoadELF(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if(!fp)
        throw Error("Could not open file");

    Elf32_Ehdr elfHeader;
    fread(&elfHeader, sizeof(Elf32_Ehdr), 1, fp);

    // check file header
    if(!IsELFBinary(elfHeader))
        throw Error("%s is not an ELF binary", filename);

    // read program header
    std::vector<Elf32_Phdr> programHeaders(elfHeader.e_phnum);
    fseek(fp, elfHeader.e_phoff, 0);
    fread(programHeaders.data(), sizeof(Elf32_Phdr), elfHeader.e_phnum, fp);

    for(size_t i=0; i<elfHeader.e_phnum; ++i)
    {
        Elf32_Phdr programHeader;
        fseek(fp, elfHeader.e_phoff + i*sizeof(Elf32_Phdr), 0);
        fread(&programHeader, sizeof(Elf32_Phdr), 1, fp);

        // check if segment should be loaded
        if(programHeader.p_type == PT_LOAD)
        {
            printf("Loading segment from file at 0x%08x (size 0x%08x) to memory at 0x%08x (size 0x%08x)\n", 
                programHeader.p_offset, programHeader.p_filesz, 
                programHeader.p_vaddr, programHeader.p_memsz);

            if(!(programHeader.p_memsz >= programHeader.p_filesz))
                throw Error("p_memsz must be equal or greater than p_filesz\n");

            // allocate 64k-aligned memory in range p_vaddr:p_memsz and set to zero
            VirtualAlloc((void*)Floor64K(programHeader.p_vaddr), Ceil64K(programHeader.p_memsz), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

            std::vector<unsigned char> segment(programHeader.p_filesz);
            
            fseek(fp, programHeader.p_offset, 0);
            fread(segment.data(), programHeader.p_filesz, 1, fp);

            // copy from file to memory
            SIZE_T written;
            if(!WriteProcessMemory(GetCurrentProcess(), (void*)programHeader.p_vaddr, segment.data(), programHeader.p_filesz, &written))
                throw Error("writing segment from file to memory failed (%d)\n", GetLastError());

            // flush cache
            FlushInstructionCache(GetCurrentProcess(), (void*)programHeader.p_vaddr, programHeader.p_memsz);
        }
    }
    return elfHeader.e_entry;
}

void Run(unsigned int addr)
{
    printf("Starting execution...\n\n");
    __asm {
        jmp addr
    }
}

LONG CALLBACK VectoredExceptionHandler(PEXCEPTION_POINTERS info)
{
    if(!info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    unsigned char* instr = (unsigned char*)info->ContextRecord->Eip;
    if(instr[0] == 0xcd && // int
       instr[1] == 0x80)   // 80
    {
        switch(info->ContextRecord->Eax)
        {
        case 1: // SYS_exit
            {
                int code = info->ContextRecord->Ebx;
                exit(code);
                break;
            }
        case 4: // SYS_write
            {
                int fd = info->ContextRecord->Ebx;
                const char* str = (const char*)info->ContextRecord->Ecx;
                size_t len = info->ContextRecord->Edx;
                int written = _write(fd, str, len);
                info->ContextRecord->Eax = written;
                break;
            }
        default:
            printf("Unknown syscall %d\n", info->ContextRecord->Eax);
            break;
        }

        info->ContextRecord->Eip+=2;    // "int 80" is two bytes long
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char* argv[])
{
    try
    {
        if(argc < 2)
            throw Error("Usage: %s filename\n", (argc>=1)?argv[0]:"lse");

        AddVectoredExceptionHandler(1, VectoredExceptionHandler);
        
        unsigned int entryAddress = LoadELF(argv[1]);
        Run(entryAddress);
    }
    catch(const Error& e)
    {
        printf("%s\n", e.message);
    }
}
