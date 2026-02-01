
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace errors {
void Report(const std::string& context, const std::string& message = "") {
    std::fprintf(stderr, "ERROR %s: %s\n", context.c_str(),
                 message.empty() ? "unknown error" : message.c_str());
}

void Exit(const std::string& context, const std::string& message = "") {
    Report(context, message);
    std::exit(EXIT_FAILURE);
}
}  // namespace errors

namespace consts {
const double kHundredPercent = 100.0;
const int64_t kTicksPerSec = sysconf(_SC_CLK_TCK);
}  // namespace consts

std::vector<int> all_pids;

class CpuLimit {
public:
    CpuLimit(int argc, char** argv) {
        std::atexit(CleanUp);
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        CommandInfo cmd;
        if (argc != 5) {
            ExitWithUsage();
        }
        if (std::string(argv[3]) != "-l") {
            ExitWithUsage();
        }
        if (std::atoi(argv[4]) <= 0) {
            ExitWithUsage();
        }

        cmd.limit_fraction = std::atoi(argv[4]) / consts::kHundredPercent;
        if (std::string(argv[1]) == "-p") {
            cmd.pid = std::atoi(argv[2]);
            RunCpuLimitByPid(cmd);

        } else if (std::string(argv[1]) == "-e") {
            cmd.exec_filename = argv[2];
            RunCpuLimitByExec(cmd);
        }
        ExitWithUsage();
    }

private:
    static void CleanUp() {
        for (int pid : all_pids) {
            kill(pid, SIGCONT);
        }
        std::printf("\nCpulimit Exit\n");
    }

    static void SignalHandler(int signal_num) {
        std::exit(signal_num);
    }

    void ExitWithUsage() {
        std::fprintf(stderr,
                     "Usage:\n"
                     "  cpulimit -p <pid> -l <limit_percentage>\n"
                     "  cpulimit -e <executable_name> -l <limit_percentage>\n");
        _exit(EXIT_FAILURE);
    }

    struct CommandInfo {
        int pid;
        std::string exec_filename;
        double limit_fraction;
    };

    int64_t GetProcessCpuUsage(int pid) {
        std::string file_path = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream file(file_path);
        if (!file.is_open()) {
            errors::Exit("GetProcessStats", "open " + file_path);
        }
        std::string skip;
        std::getline(file, skip, ')');

        for (int i = 0; i < 11; ++i) {
            file >> skip;
        }

        int64_t utime, stime;
        if (file >> utime >> stime) {
            return utime + stime;
        }
        errors::Exit("GetProcessStats", "read from " + file_path);
        return -1;
    }

    std::vector<int> GetPidsByExec(const std::string& executable_name) {
        std::vector<int> pids;

        DIR* dir = opendir("/proc");
        if (dir == nullptr) {
            errors::Exit("GetAllPids", "opendir /proc");
            return pids;
        }
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            int pid = 0;
            if (sscanf(entry->d_name, "%d", &pid) == 1) {
                std::ifstream file("/proc/" + std::to_string(pid) + "/comm");
                if (!file.is_open()) {
                    continue;
                }
                std::string process_name;
                if (file >> process_name) {
                    if (process_name == executable_name) {
                        pids.push_back(pid);
                    }
                }
            }
        }
        closedir(dir);
        return pids;
    }

    int64_t GetTotalCpuUsage(const std::vector<int>& pids) {
        int64_t total_usage = 0;
        for (int pid : pids) {
            total_usage += GetProcessCpuUsage(pid);
        }
        return total_usage;
    }

    double CountElapsedTime(const std::chrono::steady_clock::time_point& start,
                            const std::chrono::steady_clock::time_point& end) {
        return std::chrono::duration<double>(end - start).count();
    }

    double CountCpuDiff(int64_t last_ticks, int64_t current_ticks) {
        return static_cast<double>(current_ticks - last_ticks) / consts::kTicksPerSec;
    }

    double CountSleepTime(double cpu_diff, double elapsed_diff, double limit_fraction) {
        return (cpu_diff / limit_fraction) - elapsed_diff;
    }

    void RunCpuLimitByPid(const CommandInfo& cmd) {
        int64_t last_ticks = GetProcessCpuUsage(cmd.pid);
        auto last_time = std::chrono::steady_clock::now();
        all_pids.push_back(cmd.pid);
        while (true) {
            kill(cmd.pid, SIGCONT);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            int64_t current_ticks = GetProcessCpuUsage(cmd.pid);
            auto current_time = std::chrono::steady_clock::now();
            double elapsed_diff = CountElapsedTime(last_time, current_time);
            double cpu_diff = CountCpuDiff(last_ticks, current_ticks);

            if (cpu_diff >= elapsed_diff * cmd.limit_fraction) {
                kill(cmd.pid, SIGSTOP);

                double sleep_time = CountSleepTime(cpu_diff, elapsed_diff, cmd.limit_fraction);

                if (sleep_time > 0) {
                    std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time));
                }
            }

            last_ticks = current_ticks;
            last_time = std::chrono::steady_clock::now();
        }
    }

    void RunCpuLimitByExec(const CommandInfo& cmd) {
        all_pids = GetPidsByExec(cmd.exec_filename);
        if (all_pids.empty()) {
            errors::Exit("RunCpuLimitByExec", "no processes found with the given executable name");
        }
        int64_t last_ticks = GetTotalCpuUsage(all_pids);
        auto last_time = std::chrono::steady_clock::now();

        while (true) {
            for (int pid : all_pids) {
                kill(pid, SIGCONT);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            int64_t current_ticks = GetTotalCpuUsage(all_pids);
            auto current_time = std::chrono::steady_clock::now();
            double elapsed_diff = CountElapsedTime(last_time, current_time);
            double cpu_diff = CountCpuDiff(last_ticks, current_ticks);

            if (cpu_diff >= elapsed_diff * cmd.limit_fraction) {
                for (int pid : all_pids) {
                    kill(pid, SIGSTOP);
                }

                double sleep_time = CountSleepTime(cpu_diff, elapsed_diff, cmd.limit_fraction);

                if (sleep_time > 0) {
                    std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time));
                }
            }

            all_pids = GetPidsByExec(cmd.exec_filename);

            last_ticks = current_ticks;
            last_time = std::chrono::steady_clock::now();
        }
    }
};

int main(int argc, char** argv) {
    CpuLimit cpulimit(argc, argv);
    return 0;
}
