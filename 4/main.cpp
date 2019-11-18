#include <filesystem>
#include <optional>
#include <iostream>
#include <vector>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "consts.h"

using namespace std;
using namespace filesystem;

auto recursive = false;

auto getTime(file_time_type time) {
    using namespace chrono;
    stringstream ss;
    auto t = system_clock::to_time_t(time_point_cast<system_clock::duration>(time - file_time_type::clock::now() + system_clock::now()));
    ss << BLUE << put_time(localtime(&t), "%Y %b %d %H:%M");
    return ss.str();
}

auto getSize(optional<double> originalSize) {
    stringstream ss;
    ss << CYAN << setw(3);
    if (originalSize == nullopt) {
        ss << " " << "-";
    } else {
        double size = originalSize.value();
        ss << fixed;
        if (size < KB) {
            ss << setprecision(0) << size << "B";
        } else if (size < MB) {
            size /= KB;
            ss << setprecision(size < 10) << size << "K";
        } else if (size < GB) {
            size /= MB;
            ss << setprecision(size < 10) << size << "M";
        } else if (size < TB) {
            size /= GB;
            ss << setprecision(size < 10) << size << "G";
        } else {
            size /= TB;
            ss << setprecision(size < 10) << size << "T";
        }
    }
    return ss.str();
}

auto getUserAndGroup(const struct stat &info) {
    stringstream ss;
    ss << YELLOW << setw(10) << getpwuid(info.st_uid)->pw_name << " "
       << BRIGHT_YELLOW << setw(10) << getgrgid(info.st_gid)->gr_name;
    return ss.str();
}

auto getFilename(const directory_iterator &entry) {
    stringstream ss;
    auto path = entry->path();
    if (entry->is_directory()) {
        ss << MAGENTA << path.filename().string() << RESET_COLOR;
    } else {
        ss << RESET_COLOR << entry->path().filename().string();
    }
    if (entry->is_symlink()) {
        ss << BRIGHT_CYAN << " -> " << read_symlink(path).string() << RESET_COLOR;
    }
    return ss.str();
}

auto getPermissions(perms p) {
    stringstream ss;
    ss << YELLOW << ((p & perms::owner_read) != perms::none ? "r" : "-")
       << RED << ((p & perms::owner_write) != perms::none ? "w" : "-")
       << GREEN << ((p & perms::owner_exec) != perms::none ? "x" : "-")
       << YELLOW << ((p & perms::group_read) != perms::none ? "r" : "-")
       << RED << ((p & perms::group_write) != perms::none ? "w" : "-")
       << GREEN << ((p & perms::group_exec) != perms::none ? "x" : "-")
       << YELLOW << ((p & perms::others_read) != perms::none ? "r" : "-")
       << RED << ((p & perms::others_write) != perms::none ? "w" : "-")
       << GREEN << ((p & perms::others_exec) != perms::none ? "x" : "-")
       << RESET_COLOR;
    return ss.str();
}

void ls(const string &path, const string &prefix = "") {
    auto entry = directory_iterator(path);
    auto emptyEntry = directory_iterator();
    while (entry != emptyEntry) {
        stringstream ss;
        struct stat info{};
        auto nextPath = entry->path().string();
        stat(nextPath.c_str(), &info);
        auto isDirectory = entry->is_directory();
        auto isSymLink = entry->is_symlink();
        ss << BRIGHT_BLUE << (isSymLink ? "l" : isDirectory ? "d" : ".")
           << getPermissions(entry->status().permissions()) << " "
           << BRIGHT_MAGENTA << setw(3) << entry->hard_link_count() << " "
           << getSize((isDirectory || isSymLink) ? nullopt : optional<double>(entry->file_size())) << " "
           << getUserAndGroup(info) << " "
           << getTime(entry->last_write_time()) << " "
           << getFilename(entry) << endl;
        entry++;
        cout << prefix << (entry == emptyEntry ? "└─" : "├─") << ss.str();
        if (recursive && isDirectory) {
            ls(nextPath, prefix + (entry == emptyEntry ? " " : "│") + " ");
        }
    }
}

void lss(const string &path, const string &prefix = "") {
    string realPath = realpath(path.c_str(), nullptr);
    realPath += "/";
    chdir(realPath.c_str());
    auto dir = opendir(realPath.c_str());
    if (dir == nullptr) {
        cout << "Failed to get directory info!" << endl;
        return;
    }

    vector<dirent *> entries;
    dirent *temp;

    while ((temp = readdir(dir))) {
        string name = temp->d_name;
        if (name != "." && name != "..") {
            entries.push_back(temp);
        }
    }

    for (auto entry: entries) {
        string name = entry->d_name;
        struct stat info{};
        stringstream ss;
        stat(name.c_str(), &info);
        auto perm = info.st_mode;
        auto isDirectory = entry->d_type == DT_DIR;
        auto isSymLink = entry->d_type == DT_LNK;
        ss << BRIGHT_BLUE << (isSymLink ? "l" : isDirectory ? "d" : ".")
           << YELLOW << (perm & 0400 ? "r" : "-")
           << RED << (perm & 0200 ? "w" : "-")
           << GREEN << (perm & 0100 ? "x" : "-")
           << YELLOW << (perm & 040 ? "r" : "-")
           << RED << (perm & 020 ? "w" : "-")
           << GREEN << (perm & 010 ? "x" : "-")
           << YELLOW << (perm & 04 ? "r" : "-")
           << RED << (perm & 02 ? "w" : "-")
           << GREEN << (perm & 01 ? "x" : "-")
           << RESET_COLOR << " "
           << BRIGHT_MAGENTA << setw(3) << info.st_nlink << " "
           << getSize((isSymLink || isDirectory) ? nullopt : optional<double>(info.st_size)) << " "
           << getUserAndGroup(info) << " "
           << BLUE << put_time(localtime(&(info.st_mtime)), "%Y %b %d %H:%M ");
        if (isDirectory) {
            ss << MAGENTA << name << RESET_COLOR;
        } else {
            ss << RESET_COLOR << name;
        }
        if (isSymLink) {
            ss << BRIGHT_CYAN << " -> " << read_symlink(realPath + name).string() << RESET_COLOR;
        }
        ss << endl;
        cout << prefix << (entry == entries.back() ? "└─" : "├─") << ss.str();
        if (recursive && isDirectory) {
            lss(realPath + name, prefix + (entry == entries.back() ? " " : "│") + " ");
            chdir(realPath.c_str());
        }
    }
    closedir(dir);
}

int main(int argc, char **argv) {
    vector<string> paths;
    for (auto i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-R") {
            recursive = true;
        } else {
            paths.push_back(arg);
        }
    }
    try {
        if (paths.empty()) {
            ls(".");
        } else {
            for (const auto& path: paths) {
                ls(path);
            }
        }
    } catch (...) {
        cout << "Failed to get directory info!" << endl;
    }
}