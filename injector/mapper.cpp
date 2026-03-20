#include "mapper.h"
#include <vector>

static bool read_file(const wchar_t* path, std::vector<BYTE>& out) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;

    DWORD size = GetFileSize(f, nullptr);
    out.resize(size);
    DWORD read = 0;
    ReadFile(f, out.data(), size, &read, nullptr);
    CloseHandle(f);
    return read == size;
}

static BYTE* rva_to_raw(BYTE* base, IMAGE_SECTION_HEADER* sections, WORD count, DWORD rva) {
    for (WORD i = 0; i < count; i++) {
        DWORD va = sections[i].VirtualAddress;
        DWORD sz = sections[i].SizeOfRawData;
        if (sz == 0) sz = sections[i].Misc.VirtualSize;
        if (rva >= va && rva < va + sz)
            return base + sections[i].PointerToRawData + (rva - va);
    }
    return nullptr;
}

bool mapper::inject(HANDLE process, const wchar_t* dll_path) {
    std::vector<BYTE> raw;
    if (!read_file(dll_path, raw))
        return false;

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(raw.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(raw.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;

    auto* section = IMAGE_FIRST_SECTION(nt);
    WORD sec_count = nt->FileHeader.NumberOfSections;
    SIZE_T image_size = nt->OptionalHeader.SizeOfImage;

    void* remote_image = VirtualAllocEx(process, nullptr, image_size,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remote_image)
        return false;

    WriteProcessMemory(process, remote_image, raw.data(),
        nt->OptionalHeader.SizeOfHeaders, nullptr);

    for (WORD i = 0; i < sec_count; i++) {
        if (section[i].SizeOfRawData == 0) continue;
        void* dest = reinterpret_cast<BYTE*>(remote_image) + section[i].VirtualAddress;
        WriteProcessMemory(process, dest,
            raw.data() + section[i].PointerToRawData,
            section[i].SizeOfRawData, nullptr);
    }

    UINT64 reloc_delta = reinterpret_cast<UINT64>(remote_image) -
        nt->OptionalHeader.ImageBase;

    auto& reloc_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (reloc_delta != 0 && reloc_dir.Size > 0) {
        BYTE* reloc_raw = rva_to_raw(raw.data(), section, sec_count, reloc_dir.VirtualAddress);
        if (!reloc_raw) return false;

        auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reloc_raw);
        auto* reloc_end = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reloc_raw + reloc_dir.Size);

        while (reloc < reloc_end && reloc->SizeOfBlock > 0) {
            DWORD entry_count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            auto* entries = reinterpret_cast<WORD*>(reloc + 1);

            for (DWORD i = 0; i < entry_count; i++) {
                int type = entries[i] >> 12;
                int offset = entries[i] & 0xFFF;
                DWORD rva = reloc->VirtualAddress + offset;

                if (type == IMAGE_REL_BASED_DIR64) {
                    BYTE* p = rva_to_raw(raw.data(), section, sec_count, rva);
                    if (p) {
                        UINT64 val = *reinterpret_cast<UINT64*>(p) + reloc_delta;
                        WriteProcessMemory(process,
                            reinterpret_cast<BYTE*>(remote_image) + rva,
                            &val, sizeof(val), nullptr);
                    }
                } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                    BYTE* p = rva_to_raw(raw.data(), section, sec_count, rva);
                    if (p) {
                        DWORD val = *reinterpret_cast<DWORD*>(p) + static_cast<DWORD>(reloc_delta);
                        WriteProcessMemory(process,
                            reinterpret_cast<BYTE*>(remote_image) + rva,
                            &val, sizeof(val), nullptr);
                    }
                }
            }

            reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                reinterpret_cast<BYTE*>(reloc) + reloc->SizeOfBlock);
        }
    }

    auto& import_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir.Size > 0) {
        BYTE* imp_raw = rva_to_raw(raw.data(), section, sec_count, import_dir.VirtualAddress);
        if (!imp_raw) return false;

        auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(imp_raw);

        while (imp->Name) {
            char* dll_name = reinterpret_cast<char*>(
                rva_to_raw(raw.data(), section, sec_count, imp->Name));
            if (!dll_name) { imp++; continue; }

            HMODULE mod = LoadLibraryA(dll_name);
            if (!mod) { imp++; continue; }

            DWORD oft_rva = imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk;
            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(
                rva_to_raw(raw.data(), section, sec_count, oft_rva));
            DWORD iat_rva = imp->FirstThunk;

            while (thunk && thunk->u1.AddressOfData) {
                UINT64 func_addr = 0;

                if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
                    func_addr = reinterpret_cast<UINT64>(
                        GetProcAddress(mod, MAKEINTRESOURCEA(IMAGE_ORDINAL64(thunk->u1.Ordinal))));
                } else {
                    auto* by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        rva_to_raw(raw.data(), section, sec_count,
                            static_cast<DWORD>(thunk->u1.AddressOfData)));
                    if (by_name)
                        func_addr = reinterpret_cast<UINT64>(GetProcAddress(mod, by_name->Name));
                }

                WriteProcessMemory(process,
                    reinterpret_cast<BYTE*>(remote_image) + iat_rva,
                    &func_addr, sizeof(func_addr), nullptr);

                thunk++;
                iat_rva += static_cast<DWORD>(sizeof(IMAGE_THUNK_DATA64));
            }

            imp++;
        }
    }

    UINT64 entry_point = reinterpret_cast<UINT64>(remote_image) +
        nt->OptionalHeader.AddressOfEntryPoint;

    unsigned char stub[] = {
        0x48, 0x83, 0xEC, 0x28,
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xBA, 0x01, 0x00, 0x00, 0x00,
        0x4D, 0x31, 0xC0,
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,
        0x48, 0x83, 0xC4, 0x28,
        0xC3,
    };

    UINT64 img_base = reinterpret_cast<UINT64>(remote_image);
    memcpy(stub + 6, &img_base, 8);
    memcpy(stub + 24, &entry_point, 8);

    void* remote_stub = VirtualAllocEx(process, nullptr, sizeof(stub),
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remote_stub) return false;

    WriteProcessMemory(process, remote_stub, stub, sizeof(stub), nullptr);

    HANDLE thread = CreateRemoteThread(process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_stub),
        nullptr, 0, nullptr);
    if (!thread) return false;

    WaitForSingleObject(thread, 15000);
    CloseHandle(thread);
    VirtualFreeEx(process, remote_stub, 0, MEM_RELEASE);

    return true;
}
