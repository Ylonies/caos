#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <string>
#include <unistd.h>

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

void Info(const char* context, const char* message) {
    std::fprintf(stdout, "INFO %s: %s\n", context, (message != nullptr) ? message : "");
}
}  // namespace errors

enum class CommandType { kSetRpath, kSetInterpreter, kPrintRpath, kPrintInterpreter, kUnknown };

struct CommandInfo {
    CommandType type = CommandType::kUnknown;
    const char* path = nullptr;
    const char* filename = nullptr;
};

struct FileManager {
    int fd;
    Elf* elf;
};

CommandInfo ReadArgc(int argc, char** argv) {
    CommandInfo cmd;

    if (argc < 3) {
        errors::Exit("ReadArgc", "Not enough arguments");
    }

    const char* command_type = argv[1];

    if (std::strcmp(command_type, "--print-rpath") == 0) {
        cmd.type = CommandType::kPrintRpath;
        cmd.filename = argv[2];
    } else if (std::strcmp(command_type, "--print-interpreter") == 0) {
        cmd.type = CommandType::kPrintInterpreter;
        cmd.filename = argv[2];
    } else if (std::strcmp(command_type, "--set-rpath") == 0) {
        if (argc != 4) {
            errors::Exit("ReadArgc", "Usage: --set-rpath <new_rpath> <file>");
        }
        cmd.type = CommandType::kSetRpath;
        cmd.path = argv[2];
        cmd.filename = argv[3];
    } else if (std::strcmp(command_type, "--set-interpreter") == 0) {
        if (argc != 4) {
            errors::Exit("ReadArgc", "Usage: --set-interpreter <new_interp> <file>");
        }
        cmd.type = CommandType::kSetInterpreter;
        cmd.path = argv[2];
        cmd.filename = argv[3];
    } else {
        errors::Exit("ReadArgc", "Unknown command");
    }

    return cmd;
}

void FileManagerOpen(FileManager& file_manager, const char* filename) {
    struct stat st{};
    if (stat(filename, &st) != 0) {
        errors::PExit("stat");
    }

    file_manager.fd = open(filename, O_RDWR);
    if (file_manager.fd < 0) {
        errors::PExit("open");
    }

    file_manager.elf = elf_begin(file_manager.fd, ELF_C_RDWR, nullptr);
    if (file_manager.elf == nullptr || elf_kind(file_manager.elf) != ELF_K_ELF) {
        close(file_manager.fd);
        errors::Exit("elf_begin", "Not a valid ELF file");
    }
}

void FileManagerClose(FileManager* file_manager) {
    if (file_manager->elf != nullptr) {
        elf_end(file_manager->elf);
    }
    if (file_manager->fd >= 0) {
        close(file_manager->fd);
    }
    file_manager->elf = nullptr;
    file_manager->fd = -1;
}

struct DynamicInfo {
    Elf_Scn* dyn_scn = nullptr;
    Elf_Data* dyn_data = nullptr;
    Elf_Data* dynstr_data = nullptr;
    GElf_Shdr shdr{};
    bool found = false;
};

DynamicInfo FindDynamicStr(FileManager& file_manager) {
    DynamicInfo info;
    Elf_Scn* scn = nullptr;

    while ((scn = elf_nextscn(file_manager.elf, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) == nullptr) {
            errors::Exit("FindDynamicStr", elf_errmsg(-1));
        }

        if (shdr.sh_type == SHT_DYNAMIC) {
            Elf_Data* dyn_data = elf_getdata(scn, nullptr);
            if (dyn_data == nullptr) {
                errors::Exit("FindDynamicStr", "Failed to get .dynamic data");
            }

            Elf_Scn* str_scn = elf_getscn(file_manager.elf, shdr.sh_link);
            if (str_scn == nullptr) {
                errors::Exit("FindDynamicStr", "No linked .dynstr section");
            }

            Elf_Data* str_data = elf_getdata(str_scn, nullptr);
            if (str_data == nullptr) {
                errors::Exit("FindDynamicStr", "Failed to get .dynstr data");
            }

            info.dyn_scn = scn;
            info.dyn_data = dyn_data;
            info.dynstr_data = str_data;
            info.shdr = shdr;
            info.found = true;
            return info;
        }
    }

    return info;
}

void PrintRPath(FileManager& file_manager) {
    DynamicInfo info = FindDynamicStr(file_manager);
    if (!info.found) {
        errors::Info("PrintRPath", "No .dynamic section found");
        return;
    }
    if (info.shdr.sh_entsize == 0) {
        errors::Report("PrintRpath", "section size is zero");
        return;
    }
    for (size_t i = 0; i < info.shdr.sh_size / info.shdr.sh_entsize; ++i) {
        GElf_Dyn dyn;
        if (gelf_getdyn(info.dyn_data, i, &dyn) == nullptr) {
            errors::Exit("PrintRPath", elf_errmsg(-1));
        }

        if (dyn.d_tag == DT_RPATH || dyn.d_tag == DT_RUNPATH) {
            if (dyn.d_un.d_val >= info.dynstr_data->d_size) {
                errors::Report("PrintRpath", "invalid dynstr offset");
                continue;
            }
            const char* rpath = reinterpret_cast<const char*>(
                static_cast<char*>(info.dynstr_data->d_buf) + dyn.d_un.d_val);
            errors::Info("PrintRPath", rpath);
            return;
        }
    }

    errors::Info("PrintRPath", "No RPATH found");
}

void SetRPath(FileManager& file_manager, const char* new_rpath) {
    DynamicInfo info = FindDynamicStr(file_manager);
    if (!info.found) {
        errors::Info("SetRPath", "No .dynamic section found");
        return;
    }
    if (info.shdr.sh_entsize == 0) {
        errors::Report("SetRpath", "section size is zero");
        return;
    }
    for (size_t i = 0; i < info.shdr.sh_size / info.shdr.sh_entsize; ++i) {
        GElf_Dyn dyn;
        if (gelf_getdyn(info.dyn_data, i, &dyn) == nullptr) {
            errors::Exit("SetRPath", elf_errmsg(-1));
        }
        if (dyn.d_tag == DT_RPATH || dyn.d_tag == DT_RUNPATH) {
            if (dyn.d_un.d_val >= info.dynstr_data->d_size) {
                errors::Report("SetRpath", "invalid dynstr offset");
                continue;
            }
            const char* old_str = reinterpret_cast<const char*>(
                static_cast<char*>(info.dynstr_data->d_buf) + dyn.d_un.d_val);
            size_t old_len = std::strlen(old_str);
            size_t new_len = std::strlen(new_rpath);

            if (new_len > old_len) {
                errors::Exit("SetRPath", "New RPATH longer than old one. ELF rebuild needed");
            }

            off_t file_offset = info.shdr.sh_offset + dyn.d_un.d_val;

            if (pwrite(file_manager.fd, new_rpath, new_len, file_offset) !=
                static_cast<ssize_t>(new_len)) {
                errors::PExit("SetRPath: pwrite new_rpath");
            }

            if (new_len < old_len) {
                std::string padding(old_len - new_len, '\0');
                if (pwrite(file_manager.fd, padding.data(), padding.size(),
                           file_offset + new_len) != static_cast<ssize_t>(padding.size())) {
                    errors::PExit("SetRPath: pwrite padding");
                }
            }

            errors::Info("SetRPath", new_rpath);
            return;
        }
    }
    errors::Info("SetRPath", "No RPATH or RUNPATH entry found");
}

bool FindInterpHeader(FileManager& file_manager, GElf_Phdr* out_phdr) {
    size_t phnum = 0;
    if (elf_getphdrnum(file_manager.elf, &phnum) != 0) {
        errors::Exit("FindInterpHeader", elf_errmsg(-1));
    }

    for (size_t i = 0; i < phnum; ++i) {
        GElf_Phdr phdr{};
        if (gelf_getphdr(file_manager.elf, static_cast<int>(i), &phdr) == nullptr) {
            errors::Exit("FindInterpHeader", elf_errmsg(-1));
        }
        if (phdr.p_type == PT_INTERP) {
            if (out_phdr != nullptr) {
                *out_phdr = phdr;
            }
            return true;
        }
    }

    return false;
}

void PrintInterpreter(FileManager& file_manager) {
    GElf_Phdr phdr{};
    if (!FindInterpHeader(file_manager, &phdr)) {
        errors::Info("PrintInterpreter", "No interpreter (PT_INTERP) found in ELF");
        return;
    }

    if (phdr.p_filesz == 0) {
        errors::Info("PrintInterpreter", "Interpreter segment size is 0");
        return;
    }

    size_t buf_size = static_cast<size_t>(phdr.p_filesz);
    char* buf = static_cast<char*>(std::malloc(buf_size + 1));
    if (buf == nullptr) {
        errors::PExit("PrintInterpreter: malloc");
    }

    ssize_t rd = pread(file_manager.fd, buf, buf_size, static_cast<off_t>(phdr.p_offset));
    if (rd < 0) {
        std::free(buf);
        errors::PExit("PrintInterpreter: pread");
    }

    buf[std::min(static_cast<size_t>(rd), buf_size)] = '\0';
    errors::Info("PrintInterpreter", buf);
    std::free(buf);
}

void SetInterpreter(FileManager& file_manager, const char* new_interp) {
    GElf_Phdr phdr{};
    if (!FindInterpHeader(file_manager, &phdr)) {
        errors::Exit("SetInterpreter", "No PT_INTERP segment found");
    }

    if (phdr.p_filesz == 0) {
        errors::Exit("SetInterpreter", "PT_INTERP segment size is 0");
    }

    size_t old_size = static_cast<size_t>(phdr.p_filesz);
    size_t new_len = std::strlen(new_interp) + 1;

    if (new_len > old_size) {
        errors::Exit("SetInterpreter", "new interpreter path longer than old one");
    }

    ssize_t written =
        pwrite(file_manager.fd, new_interp, new_len, static_cast<off_t>(phdr.p_offset));
    if (written < 0) {
        errors::PExit("SetInterpreter: pwrite");
    }

    if (new_len < old_size) {
        std::string padding(old_size - new_len, '\0');
        int pwrite_res = pwrite(file_manager.fd, padding.data(), padding.size(),
                                static_cast<off_t>(phdr.p_offset + new_len));
        if (pwrite_res == -1) {
            errors::PExit("pwrite");
        }
        if (pwrite_res == 0) {
            errors::Exit("pwrite", "nothing was written");
        }
        if (static_cast<size_t>(pwrite_res) != padding.size()) {
            errors::Exit("pwrite", "partial write occurred");
        }
    }

    errors::Info("SetInterpreter", new_interp);
}

int main(int argc, char** argv) {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        errors::Exit("elf_version", "ELF library initialization failed");
    }

    CommandInfo cmd = ReadArgc(argc, argv);
    FileManager file_manager;
    FileManagerOpen(file_manager, cmd.filename);

    switch (cmd.type) {
        case CommandType::kPrintRpath:
            PrintRPath(file_manager);
            break;
        case CommandType::kPrintInterpreter:
            PrintInterpreter(file_manager);
            break;
        case CommandType::kSetRpath:
            SetRPath(file_manager, cmd.path);
            break;
        case CommandType::kSetInterpreter:
            SetInterpreter(file_manager, cmd.path);
            break;
        default:
            errors::Info("Main", "Unsupported command");
            break;
    }
    FileManagerClose(&file_manager);
}
