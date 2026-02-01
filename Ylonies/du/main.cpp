#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <set>
#include <stdlib.h>

#include <sys/stat.h>

const uint64_t kMagicNumber = 4096;

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
    bool flag_a = false;
    bool flag_s = false;
    bool flag_L = false;
    const char* dir_name = nullptr;
};

CommandInfo ReadArgc(int argc, char** argv) {
    CommandInfo cmd;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        const char* arg = argv[i];

        for (int j = 1; arg[j] != '\0'; ++j) {
            switch (arg[j]) {
                case 'a':
                    cmd.flag_a = true;
                    break;
                case 's':
                    cmd.flag_s = true;
                    break;
                case 'L':
                    cmd.flag_L = true;
                    break;
                default:
                    errors::Exit("ReadArgc", "Unknown flag");
            }
        }
        ++i;
    }
    if (cmd.flag_a && cmd.flag_s) {
        errors::Exit("ReadArgc", "Flags -a and -s cannot be combined");
    }
    if (i == argc - 1) {
        cmd.dir_name = argv[argc - 1];
    } else if (i == argc) {
        cmd.dir_name = ".";
    } else {
        errors::Exit("ReadArgc", "Too many path args");
    }
    return cmd;
}

bool AlreadyCounted(const struct stat& stat_info, std::set<std::pair<dev_t, ino_t>>& visited) {
    auto pair = std::pair<dev_t, ino_t>(stat_info.st_dev, stat_info.st_ino);
    return visited.find(pair) != visited.end();
}

void PrintSize(uint64_t size_bytes, const char* path) {
    printf("%lu %s\n", size_bytes, path);
}

uint64_t GetDirSize(const char* path, const CommandInfo& cmd,
                    std::set<std::pair<dev_t, ino_t>>& visited, bool is_root = true) {
    struct stat stat_info;

    if (cmd.flag_L) {
        if (stat(path, &stat_info) != 0) {
            errors::Report("GetDirSize", "cannot access stat");
            return 0;
        }
    } else {
        if (lstat(path, &stat_info) != 0) {
            errors::Report("GetDirSize", "cannot access lstat");
            return 0;
        }
    }

    if (AlreadyCounted(stat_info, visited)) {
        return 0;
    }
    visited.insert(std::make_pair(stat_info.st_dev, stat_info.st_ino));

    if (S_ISLNK(stat_info.st_mode) && !cmd.flag_L) {
        return stat_info.st_size;
    }

    if (S_ISREG(stat_info.st_mode)) {
        if (cmd.flag_a || is_root) {
            PrintSize(stat_info.st_size, path);
        }
        return stat_info.st_size;
    }
    if (S_ISDIR(stat_info.st_mode)) {
        uint64_t summary_size = kMagicNumber;
        DIR* dir = opendir(path);
        if (dir == nullptr) {
            errors::Report("GetDirSize", "Cannot open directory");
            return 0;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            const char* name = entry->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }
            char child_path[PATH_MAX];
            snprintf(child_path, PATH_MAX, "%s/%s", path, entry->d_name);
            summary_size += GetDirSize(child_path, cmd, visited, false);
        }
        closedir(dir);
        if (!cmd.flag_s || is_root) {
            PrintSize(summary_size, path);
        }
        return summary_size;
    }

    return 0;
}

int main(int argc, char** argv) {
    CommandInfo cmd = ReadArgc(argc, argv);
    std::set<std::pair<dev_t, ino_t>> visited;
    GetDirSize(cmd.dir_name, cmd, visited);
}