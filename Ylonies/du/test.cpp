#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

#ifndef DU_PATH
#define DU_PATH "./du"
#endif

namespace fs = std::filesystem;

struct TempTree {
    [[nodiscard]] TempTree(const std::vector<std::pair<std::string, std::string>>& desc) {
        fs::path tmp_dir = fs::temp_directory_path() / "duXXXXXX";
        std::string tmp_str = tmp_dir.string();

        if (mkdtemp(tmp_str.data()) == nullptr) {
            throw std::runtime_error("Failed to create temp dir");
        }

        tmp_dir = tmp_str.c_str();

        for (const auto& [path_str, data] : desc) {
            fs::path full_path = tmp_dir / path_str;

            if (path_str.back() == '/') {
                fs::create_directories(full_path);
                continue;
            }
            fs::create_directories(full_path.parent_path());

            if (path_str.ends_with("_hardlink")) {
                fs::path target = tmp_dir / data;
                if (link(target.c_str(), full_path.c_str()) != 0) {
                    perror(("Failed to create hard link: " + path_str).c_str());
                    throw std::runtime_error("link() failed");
                }
                continue;
            }

            if (path_str.ends_with("_symlink")) {
                if (symlink(data.c_str(), full_path.c_str()) != 0) {
                    perror(("Failed to create symlink: " + path_str).c_str());
                    throw std::runtime_error("symlink() failed");
                }
                continue;
            }

            std::ofstream ofs(full_path);

            if (!ofs) {
                throw std::runtime_error("Failed to open " + full_path.string());
            }

            ofs << data;
        }

        root = tmp_dir;
    }

    ~TempTree() {
        fs::remove_all(root);
    }

    fs::path root;
};

int MakeDiffFile(const std::string& root, const std::string& du_path, const std::string& du_args) {
    std::string du_cmd = "du -b" + du_args + root + " > expected.out 2>/dev/null";
    std::string program_cmd = du_path + du_args + root + " > program.out";

    system(du_cmd.c_str());
    system(program_cmd.c_str());
    int diff_exit = system("diff -ub expected.out program.out > diff.out");
    return diff_exit;
}

TEST(DuTests, BasicTree) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"dir1/inner/inner_x2/file1", "who cares"},
        {"dir1/inner/inner_x2/file2", "nobody"},
        {"dir1/inner/file3", "somefile"},
        {"dir1/inner_other/file4", "sometext"},
        {"dir1/inner_other/empty/", ""},
        {"dir2/file5", "this file is high"},
        {"dir2/file6", "kek"},
        {"random_file", "what is is doing here"},
    };

    TempTree tree(desc);
    int diff_exit = MakeDiffFile(tree.root.string(), std::string(DU_PATH), " ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, SimpleFile) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"just_file", "who cares"},
    };

    TempTree tree(desc);
    int diff_exit = MakeDiffFile((tree.root / "just_file").string(), std::string(DU_PATH), " ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, FileSymlink) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"base/file_symlink", "../just_file"},
        {"just_file", "who even cares 'bout this"},
    };

    TempTree tree(desc);
    int diff_exit = MakeDiffFile((tree.root / "just_file").string(), std::string(DU_PATH), " -L ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, DoubleFileSymlink) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"base/file_symlink", "../other_symlink"},
        {"other_symlink", "./just_file"},
        {"just_file", "who even cares 'bout this"},
    };

    TempTree tree(desc);
    int diff_exit =
        MakeDiffFile((tree.root / "base/file_symlink").string(), std::string(DU_PATH), " -L ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, DirectorySymlink) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"dir/inner/inner_x2/file1", "who cares"},
        {"dir/inner/inner_x2/file2", "nobody"},
        {"dir/inner/file3", "somefile"},
        {"dir/inner_other/file4", "sometext"},
        {"dir/inner_other/empty/", ""},
        {"base/dir_symlink", "../dir"},
        {"just_file", "this text doesnt matter"},
    };

    TempTree tree(desc);
    int diff_exit =
        MakeDiffFile((tree.root / "base/dir_symlink").string(), std::string(DU_PATH), " -L ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, HardLinks) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"dir1/inner/inner_x2/file1", "who cares"},
        {"dir1/inner/inner_x2/file2", "nobody"},
        {"dir1/inner/file3", "somefile"},
        {"dir1/inner_other/file4", "sometext"},
        {"dir1/file1_hardlink", "dir1/inner/inner_x2/file1"},
        {"dir1/inner_other/empty/", ""},
        {"dir2/file5", "this file is high"},
        {"dir2/file6", "kek"},
        {"dir2/file5_hardlink", "dir2/file5"},
        {"random_file", "what is is doing here"},
    };

    TempTree tree(desc);
    int diff_exit = MakeDiffFile(tree.root.string(), std::string(DU_PATH), " ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, SymLinks) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"base/dir1/inner/inner_x2/file1", "who cares"},
        {"base/dir1/inner/inner_x2/file2", "nobody"},
        {"base/dir1/inner/file3", "somefile"},
        {"base/dir1/inner_other/file4", "sometext"},
        {"base/dir1/random_file_symlink", "../random_file"},
        {"base/dir1/inner_other/empty/", ""},
        {"base/dir2/file5", "this file is high"},
        {"base/dir2/file6", "kek"},
        {"base/dir2/hidden_symlink", "../../hidden"},
        {"base/dir2/broken_symlink", "dir2/this_thing_doesnt_exist"},
        {"base/random_file",
         "what is it doing here, also this file is pretty big. Some nonsense "
         "text: wowowowowowowowowowowowowowowowowowowowowowowo"},
        {"hidden/secret1", "This is hidden content"},
        {"hidden/secret2", "Dont you dare"},
    };

    TempTree tree(desc);
    int diff_exit = MakeDiffFile((tree.root / "base").string(), std::string(DU_PATH), " ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, Cycle) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"base/dir1/inner/inner_x2/file1", "who cares"},
        {"base/dir1/inner/inner_x2/file2", "nobody"},
        {"base/dir1/inner/file3", "somefile"},
        {"base/dir1/inner_other/file4", "sometext"},
        {"base/dir1/self_symlink", "./"},
        {"base/dir1/random_file_symlink", "../random_file"},
        {"base/dir1/inner_other/empty/", ""},
        {"base/dir2/file5", "this file is high"},
        {"base/dir2/file6", "kek"},
        {"base/dir2/hidden_symlink", "../../hidden"},
        {"base/dir2/broken_symlink", "dir2/this_thing_doesnt_exist"},
        {"base/random_file",
         "what is it doing here, also this file is pretty big. Some nonsense "
         "text: wowowowowowowowowowowowowowowowowowowowowowowo"},
        {"hidden/secret1", "This is hidden content"},
        {"hidden/secret2", "Dont you dare"},
        {"hidden/base_symlink", "../base"}};

    TempTree tree(desc);
    int diff_exit = MakeDiffFile((tree.root / "base").string(), std::string(DU_PATH), " -L ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, FlagsAll) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"base/dir1/inner/inner_x2/file1", "who cares"},
        {"base/dir1/inner/inner_x2/file2", "nobody"},
        {"base/dir1/inner/file3", "somefile"},
        {"base/dir1/inner_other/file4", "sometext"},
        {"base/dir1/self_symlink", "./"},
        {"base/dir1/file1_hardlink", "base/dir1/inner/inner_x2/file1"},
        {"base/dir1/random_file_symlink", "../random_file"},
        {"base/dir1/inner_other/empty/", ""},
        {"base/dir2/file5", "this file is high"},
        {"base/dir2/file6", "kek"},
        {"base/dir2/hidden_symlink", "../../hidden"},
        {"base/dir2/broken_symlink", "dir2/this_thing_doesnt_exist"},
        {"base/random_file",
         "what is it doing here, also this file is pretty big. Some nonsense "
         "text: wowowowowowowowowowowowowowowowowowowowowowowo"},
        {"hidden/secret1", "This is hidden content"},
        {"hidden/secret2", "Dont you dare"},
        {"hidden/base_symlink", "../base"}};

    TempTree tree(desc);
    int diff_exit = MakeDiffFile((tree.root / "base").string(), std::string(DU_PATH), " -aL ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}

TEST(DuTests, FlagsSummarize) {
    std::vector<std::pair<std::string, std::string>> desc = {
        {"base/dir1/inner/inner_x2/file1", "who cares"},
        {"base/dir1/inner/inner_x2/file2", "nobody"},
        {"base/dir1/inner/file3", "somefile"},
        {"base/dir1/inner_other/file4", "sometext"},
        {"base/dir1/self_symlink", "./"},
        {"base/dir1/file1_hardlink", "base/dir1/inner/inner_x2/file1"},
        {"base/dir1/random_file_symlink", "../random_file"},
        {"base/dir1/inner_other/empty/", ""},
        {"base/dir2/file5", "this file is high"},
        {"base/dir2/file6", "kek"},
        {"base/dir2/hidden_symlink", "../../hidden"},
        {"base/dir2/broken_symlink", "dir2/this_thing_doesnt_exist"},
        {"base/random_file",
         "what is it doing here, also this file is pretty big. Some nonsense "
         "text: wowowowowowowowowowowowowowowowowowowowowowowo"},
        {"hidden/secret1", "This is hidden content"},
        {"hidden/secret2", "Dont you dare"},
        {"hidden/base_symlink", "../base"}};

    TempTree tree(desc);
    int diff_exit = MakeDiffFile((tree.root / "base").string(), std::string(DU_PATH), " -sL ");

    if (diff_exit != 0) {
        std::ifstream diff("diff.out");
        std::stringstream diff_content;
        diff_content << diff.rdbuf();
        FAIL() << "-asL" << diff_content.str();
    }

    fs::remove("expected.out");
    fs::remove("program.out");
    fs::remove("diff.out");
}
