#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <pwd.h>
#include <sstream>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>

namespace errors {
void Report(const std::string& context, const std::string& message = "") {
    std::fprintf(stderr, "ERROR %s: %s\n", context.c_str(),
                 message.empty() ? "unknown error" : message.c_str());
}

void Exit(const std::string& context, const std::string& message = "") {
    Report(context, message);
    std::exit(EXIT_FAILURE);
}

};  // namespace errors

namespace consts {
constexpr double kHundredPercent = 100.0;
constexpr int kSecsPerMin = 60;
constexpr int kKilobyte = 1024;
const int64_t kTicksPerSec = sysconf(_SC_CLK_TCK);

constexpr int kWidthPid = 6;
constexpr int kWidthUser = 10;
constexpr int kWidthPri = 4;
constexpr int kWidthNi = 4;
constexpr int kWidthVirt = 12;
constexpr int kWidthRes = 12;
constexpr int kWidthStat = 2;
constexpr int kWidthCpu = 6;
constexpr int kWidthMem = 6;
constexpr int kWidthTime = 11;
constexpr int kWidthCommand = 15;

constexpr int kPrecisionCpu = 1;
constexpr int kPrecisionTime = 0;
}  // namespace consts

struct ProcessStat {
    int pid;
    std::string command;
    char state;
    int64_t utime;
    int64_t stime;
    int64_t priority;
    int64_t niceness;
    int64_t starttime;
    int64_t res;
    std::string username;
    int virt;
    double cpu;
    double mem;
};

std::vector<int> GetAllPids() {
    std::vector<int> pids;

    DIR* dir = opendir("/proc");
    if (dir == nullptr) {
        errors::Exit("GetAllPids", "opendir /proc");
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        int pid = 0;
        if (sscanf(entry->d_name, "%d", &pid) == 1) {
            std::ifstream test("/proc/" + std::to_string(pid) + "/stat");
            if (test.is_open()) {
                pids.push_back(pid);
            }
        }
    }
    closedir(dir);

    return pids;
}

void GetProcessStats(int pid, ProcessStat& process) {
    std::string file_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(file_path);
    if (!file.is_open()) {
        errors::Exit("GetProcessStats", "open " + file_path);
    }
    for (size_t i = 1; i <= 24; ++i) {
        switch (i) {
            case 1:
                file >> process.pid;
                break;
            case 2: {
                std::string comm;
                file >> std::ws;
                std::getline(file, comm, ')');
                comm += ')';
                process.command = comm.substr(1, comm.size() - 2);
                break;
            }
            case 3:
                file >> process.state;
                break;
            case 14:
                file >> process.utime;
                break;
            case 15:
                file >> process.stime;
                break;
            case 18:
                file >> process.priority;
                break;
            case 19:
                file >> process.niceness;
                break;
            case 22: {
                file >> process.starttime;
                break;
            }
            case 23: {
                int64_t vsize;
                file >> vsize;
                process.virt = vsize / consts::kKilobyte;
                break;
            }
            case 24: {
                int rss;
                file >> rss;
                process.res = rss * sysconf(_SC_PAGESIZE) / consts::kKilobyte;
                break;
            }
            default: {
                int64_t skip;
                file >> skip;
                break;
            }
        }
    }
}

void GetUsername(int pid, ProcessStat& process) {
    std::string file_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(file_path);

    if (!file.is_open()) {
        errors::Exit("GetUsername", "open " + file_path);
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("Uid:", 0) == 0) {
            std::istringstream iss(line.substr(4));
            int uid;
            iss >> uid;
            struct passwd* passwd = getpwuid(uid);
            if (passwd != nullptr) {
                process.username = passwd->pw_name;
            } else {
                errors::Report("getpwuid", "Failed to get username");
            }
        }
    }
}

void CalculateCPU(const ProcessStat& prev, ProcessStat& curr) {
    int64_t delta = (curr.utime + curr.stime) - (prev.utime + prev.stime);
    curr.cpu = static_cast<double>(delta) / (consts::kTicksPerSec)*consts::kHundredPercent;
}

double CalculateTotalMem() {
    int64_t total_memory_kb = 0;
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        errors::Exit("CalculateTotalMem", "open /proc/meminfo");
    }

    std::string key;
    int64_t value;
    while (meminfo >> key >> value) {
        if (key == "MemTotal:") {
            total_memory_kb = value;
            break;
        }
    }
    if (total_memory_kb == 0) {
        errors::Exit("CalculateTotalMem", "Failed to read MemTotal");
    }
    return total_memory_kb;
}

std::string CalculateTime(const ProcessStat& process) {
    std::ifstream file("/proc/uptime");
    if (!file.is_open()) {
        errors::Exit("CalculateTime", "open /proc/uptime");
    }

    double uptime_seconds;
    file >> uptime_seconds;

    int64_t elapsed_ticks =
        static_cast<int64_t>(uptime_seconds * consts::kTicksPerSec) - process.starttime;
    double elapsed_seconds = static_cast<double>(elapsed_ticks) / consts::kTicksPerSec;

    int minutes = static_cast<int>(elapsed_seconds) / consts::kSecsPerMin;
    double seconds = elapsed_seconds - minutes * consts::kSecsPerMin;

    std::ostringstream oss;
    oss << minutes << ":" << std::fixed << std::setfill('0') << std::setw(5) << std::setprecision(2)
        << seconds;
    return oss.str();
}

void PrintTable(const std::vector<ProcessStat>& processes) {
    std::cout << "\033[H\033[2J\033[3J";

    std::cout << std::left << std::setw(consts::kWidthPid) << "PID" << std::setw(consts::kWidthUser)
              << "USER" << std::setw(consts::kWidthPri) << "PR" << std::setw(consts::kWidthNi)
              << "NI" << std::setw(consts::kWidthVirt) << "VIRT" << std::setw(consts::kWidthRes)
              << "RES" << std::setw(consts::kWidthStat) << "S" << std::setw(consts::kWidthCpu)
              << "%CPU" << std::setw(consts::kWidthMem) << "%MEM" << std::setw(consts::kWidthTime)
              << "TIME+"
              << "COMMAND" << std::endl;

    int max_lines = 25;
    int count = 0;

    for (const auto& process : processes) {
        if (count++ >= max_lines) {
            break;
        }

        std::cout << std::left << std::setw(consts::kWidthPid) << process.pid
                  << std::setw(consts::kWidthUser)
                  << process.username.substr(0, consts::kWidthUser - 1)
                  << std::setw(consts::kWidthPri) << process.priority << std::setw(consts::kWidthNi)
                  << process.niceness << std::setw(consts::kWidthVirt) << process.virt
                  << std::setw(consts::kWidthRes) << process.res << std::setw(consts::kWidthStat)
                  << process.state << std::setw(consts::kWidthCpu) << std::fixed
                  << std::setprecision(consts::kPrecisionCpu) << process.cpu
                  << std::setw(consts::kWidthMem) << process.mem << std::setw(consts::kWidthTime)
                  << std::setprecision(consts::kPrecisionTime) << CalculateTime(process)
                  << process.command.substr(0, consts::kWidthCommand) << std::endl;
    }

    std::cout << std::flush;
}

int main() {
    std::map<int, ProcessStat> prev_stats;
    while (true) {
        std::vector<ProcessStat> processes;
        std::vector<int> pids = GetAllPids();
        int64_t total_memory_kb = CalculateTotalMem();
        for (int pid : pids) {
            ProcessStat process{};
            GetProcessStats(pid, process);
            GetUsername(pid, process);

            if (prev_stats.count(pid) != 0) {
                CalculateCPU(prev_stats[pid], process);
            } else {
                process.cpu = 0.0;
            }
            process.mem =
                (static_cast<double>(process.res) / total_memory_kb) * consts::kHundredPercent;
            processes.push_back(process);
            prev_stats[pid] = process;
        }

        std::sort(processes.begin(), processes.end(),
                  [](auto& first, auto& second) { return first.cpu > second.cpu; });

        PrintTable(processes);

        sleep(1);
    }
    return 0;
}