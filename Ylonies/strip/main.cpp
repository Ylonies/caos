#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>

namespace errors {
void Report(const char* context, const char* message) {
    std::fprintf(stderr, "ERROR %s: %s\n", context,
                 (message != nullptr) ? message : "unknown error");
}

void PErr(const char* context) {
    std::fprintf(stderr, "ERROR %s: ", context);
    std::perror(nullptr);
}

void Exit(const char* context, const char* message) {
    Report(context, message);
    std::exit(EXIT_FAILURE);
}

void PExit(const char* context) {
    PErr(context);
    std::exit(EXIT_FAILURE);
}
}  // namespace errors

struct CommandInfo {
    bool flag_d;
    const char* filename;
};

struct FileManager {
    int fd = -1;
    int new_fd = -1;
    Elf* elf = nullptr;
    Elf* new_elf = nullptr;
    std::vector<void*> allocated_buffers;
};

CommandInfo ReadArgc(int argc, char** argv);
void FileManagerOpen(FileManager* file_manager, const char* filename);
void FileManagerClose(FileManager* file_manager);
bool SkipSections(const char* name, CommandInfo* cmd);
void GetElfSectionInfo(Elf* elf, size_t* shnum, size_t* shstrndx);
void InitEhdr(FileManager* file_manager, GElf_Ehdr* ehdr);
void CloneSections(FileManager* file_manager, CommandInfo* cmd, size_t shstrndx, size_t* old_to_new,
                   size_t* new_shstrndx);
void UpdateSectionLinks(FileManager* file_manager, size_t shnum, size_t* old_to_new);
void EndElf(FileManager* file_manager, GElf_Ehdr* ehdr, size_t new_shstrndx, size_t* old_to_new);
void ClonePhdrs(FileManager* file_manager);

CommandInfo ReadArgc(int argc, char** argv) {
    CommandInfo cmd{false, nullptr};
    if (argc == 2) {
        cmd.filename = argv[1];
    } else if (argc == 3 && std::strcmp(argv[1], "-d") == 0) {
        cmd.flag_d = true;
        cmd.filename = argv[2];
    } else {
        std::fprintf(stderr, "Usage: %s [-d] filename\n", argv[0]);
        std::exit(EXIT_FAILURE);
    }
    return cmd;
}

void FileManagerOpen(FileManager* file_manager, const char* filename) {
    if (file_manager == nullptr) {
        errors::Exit("fileManagerOPen", "Is nullptr");
    }
    struct stat st{};
    if (stat(filename, &st) != 0) {
        errors::PExit("stat");
    }

    file_manager->fd = open(filename, O_RDONLY);
    if (file_manager->fd < 0) {
        errors::PExit("open (read)");
    }

    file_manager->elf = elf_begin(file_manager->fd, ELF_C_READ, nullptr);
    if (file_manager->elf == nullptr || elf_kind(file_manager->elf) != ELF_K_ELF) {
        close(file_manager->fd);
        errors::Exit("elf_begin", "Not a valid ELF file");
    }

    unlink("new_elf.tmp");
    file_manager->new_fd = open("new_elf.tmp", O_CREAT | O_WRONLY | O_TRUNC, st.st_mode);
    if (file_manager->new_fd < 0) {
        elf_end(file_manager->elf);
        close(file_manager->fd);
        errors::PExit("open (write)");
    }

    file_manager->new_elf = elf_begin(file_manager->new_fd, ELF_C_WRITE, nullptr);
    if (file_manager->new_elf == nullptr) {
        elf_end(file_manager->elf);
        close(file_manager->fd);
        close(file_manager->new_fd);
        errors::Exit("elf_begin(write)", elf_errmsg(-1));
    }
    elf_flagelf(file_manager->new_elf, ELF_C_SET, ELF_F_LAYOUT);
}

void FileManagerClose(FileManager* file_manager) {
    if (file_manager->elf != nullptr) {
        elf_end(file_manager->elf);
    }
    if (file_manager->new_elf != nullptr) {
        elf_end(file_manager->new_elf);
    }
    if (file_manager->fd >= 0) {
        close(file_manager->fd);
    }
    if (file_manager->new_fd >= 0) {
        close(file_manager->new_fd);
    }
    file_manager->elf = nullptr;
    file_manager->new_elf = nullptr;
    file_manager->fd = -1;
    file_manager->new_fd = -1;
    for (void* buf : file_manager->allocated_buffers) {
        free(buf);
    }
}

bool SkipSections(const char* name, CommandInfo* cmd) {
    if (name == nullptr) {
        return false;
    }
    if (std::strncmp(name, ".debug_", 7) == 0) {
        return true;
    }
    if (!cmd->flag_d && std::strcmp(name, ".symtab") == 0) {
        return true;
    }
    return false;
}

void GetElfSectionInfo(Elf* elf, size_t* shnum, size_t* shstrndx) {
    if (elf_getshdrnum(elf, shnum) != 0 || elf_getshdrstrndx(elf, shstrndx) != 0) {
        errors::Exit("elf_getshdrnum/elf_getshdrstrndx", elf_errmsg(-1));
    }
}

void InitEhdr(FileManager* file_manager, GElf_Ehdr* ehdr) {
    if (gelf_getehdr(file_manager->elf, ehdr) == nullptr) {
        errors::Exit("gelf_getehdr", elf_errmsg(-1));
    }
    if (gelf_newehdr(file_manager->new_elf, gelf_getclass(file_manager->elf)) == nullptr) {
        errors::Exit("gelf_newehdr", elf_errmsg(-1));
    }
}

void ClonePhdrs(FileManager* file_manager) {
    size_t phnum = 0;
    if (elf_getphdrnum(file_manager->elf, &phnum) != 0) {
        errors::Exit("elf_getphdrnum", elf_errmsg(-1));
    }
    if (phnum == 0) {
        return;
    }
    if (gelf_newphdr(file_manager->new_elf, phnum) == nullptr) {
        errors::Exit("gelf_newphdr", elf_errmsg(-1));
    }

    for (size_t i = 0; i < phnum; ++i) {
        GElf_Phdr phdr{};
        if (gelf_getphdr(file_manager->elf, i, &phdr) == nullptr) {
            errors::Exit("gelf_getphdr", elf_errmsg(-1));
        }
        if (gelf_update_phdr(file_manager->new_elf, i, &phdr) == 0) {
            errors::Exit("gelf_update_phdr", elf_errmsg(-1));
        }
    }
}

void CloneSections(FileManager* file_manager, CommandInfo* cmd, size_t shstrndx, size_t* old_to_new,
                   size_t* new_shstrndx) {
    Elf_Scn* scn = nullptr;
    while ((scn = elf_nextscn(file_manager->elf, scn)) != nullptr) {
        size_t old_idx = elf_ndxscn(scn);
        GElf_Shdr shdr{};
        if (gelf_getshdr(scn, &shdr) == nullptr) {
            continue;
        }

        const char* name = elf_strptr(file_manager->elf, shstrndx, shdr.sh_name);
        if (SkipSections(name, cmd)) {
            continue;
        }

        Elf_Scn* new_scn = elf_newscn(file_manager->new_elf);
        if (new_scn == nullptr) {
            errors::Exit("elf_newscn", elf_errmsg(-1));
        }

        size_t new_idx = elf_ndxscn(new_scn);
        old_to_new[old_idx] = new_idx;
        if (std::strcmp(name, ".shstrtab") == 0) {
            *new_shstrndx = new_idx;
        }

        Elf_Data* data = elf_getdata(scn, nullptr);
        if (data != nullptr && data->d_size > 0 && data->d_buf != nullptr) {
            Elf_Data* new_data = elf_newdata(new_scn);
            if (new_data == nullptr) {
                errors::Exit("elf_newdata", elf_errmsg(-1));
            }

            void* buf = malloc(data->d_size);
            if (buf == nullptr) {
                errors::PExit("malloc");
            }
            memcpy(buf, data->d_buf, data->d_size);

            new_data->d_align = data->d_align;
            new_data->d_off = 0;
            new_data->d_type = data->d_type;
            new_data->d_version = data->d_version;
            new_data->d_size = data->d_size;
            new_data->d_buf = buf;
            file_manager->allocated_buffers.push_back(buf);
        }

        GElf_Shdr new_shdr{};
        if (gelf_getshdr(new_scn, &new_shdr) == nullptr) {
            errors::Exit("gelf_getshdr", elf_errmsg(-1));
        }
        new_shdr = shdr;
        gelf_update_shdr(new_scn, &new_shdr);
    }
}

void UpdateSectionLinks(FileManager* file_manager, size_t shnum, size_t* old_to_new) {
    Elf_Scn* scn = nullptr;
    while ((scn = elf_nextscn(file_manager->new_elf, scn)) != nullptr) {
        GElf_Shdr shdr{};
        if (gelf_getshdr(scn, &shdr) == nullptr) {
            continue;
        }

        if (shdr.sh_link != 0 && shdr.sh_link < shnum) {
            size_t new_link = old_to_new[shdr.sh_link];
            shdr.sh_link = (new_link != SHN_UNDEF) ? new_link : 0;
        }

        if (shdr.sh_info != 0 && shdr.sh_info < shnum) {
            size_t new_info = old_to_new[shdr.sh_info];
            shdr.sh_info = (new_info != SHN_UNDEF) ? new_info : 0;
        }
        gelf_update_shdr(scn, &shdr);
    }
}

void EndElf(FileManager* file_manager, GElf_Ehdr* ehdr, size_t new_shstrndx, size_t* old_to_new) {
    if (new_shstrndx != 0) {
        ehdr->e_shstrndx = new_shstrndx;
    }
    if (gelf_update_ehdr(file_manager->new_elf, ehdr) == 0) {
        errors::Exit("gelf_update_ehdr", elf_errmsg(-1));
    }
    if (elf_update(file_manager->new_elf, ELF_C_WRITE) < 0) {
        errors::Exit("elf_update", elf_errmsg(-1));
    }
    free(old_to_new);
}

void CloneElf(FileManager* file_manager, CommandInfo* cmd) {
    size_t shnum = 0;
    size_t shstrndx = 0;
    GetElfSectionInfo(file_manager->elf, &shnum, &shstrndx);

    GElf_Ehdr ehdr{};
    InitEhdr(file_manager, &ehdr);
    ClonePhdrs(file_manager);

    size_t* old_to_new = static_cast<size_t*>(calloc(shnum, sizeof(size_t)));
    if (old_to_new == nullptr) {
        errors::PExit("calloc");
    }

    for (size_t i = 0; i < shnum; ++i) {
        old_to_new[i] = SHN_UNDEF;
    }

    size_t new_shstrndx = 0;
    CloneSections(file_manager, cmd, shstrndx, old_to_new, &new_shstrndx);
    UpdateSectionLinks(file_manager, shnum, old_to_new);
    EndElf(file_manager, &ehdr, new_shstrndx, old_to_new);
}

int main(int argc, char** argv) {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        errors::Exit("elf_version", "ELF library initialization failed");
    }

    CommandInfo cmd = ReadArgc(argc, argv);

    FileManager file_manager;
    FileManagerOpen(&file_manager, cmd.filename);
    CloneElf(&file_manager, &cmd);
    FileManagerClose(&file_manager);

    if (rename("new_elf.tmp", cmd.filename) != 0) {
        errors::PExit("rename");
    }

    std::printf("Successful\n");
    return 0;
}