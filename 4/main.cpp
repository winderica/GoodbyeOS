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

auto getPermissions(perms p) {
    stringstream ss;
    ss << YELLOW << ((p & perms::owner_read) != perms::none ? "r" : "-")    // owner readable
       << RED << ((p & perms::owner_write) != perms::none ? "w" : "-")      // owner writable
       << GREEN << ((p & perms::owner_exec) != perms::none ? "x" : "-")     // owner executable
       << YELLOW << ((p & perms::group_read) != perms::none ? "r" : "-")    // group readable
       << RED << ((p & perms::group_write) != perms::none ? "w" : "-")      // group writable
       << GREEN << ((p & perms::group_exec) != perms::none ? "x" : "-")     // group executable
       << YELLOW << ((p & perms::others_read) != perms::none ? "r" : "-")   // others readable
       << RED << ((p & perms::others_write) != perms::none ? "w" : "-")     // others writable
       << GREEN << ((p & perms::others_exec) != perms::none ? "x" : "-");   // others executable
    return ss.str();
}

auto getSize(optional<double> originalSize) {
    stringstream ss;
    ss << CYAN << setw(4);
    if (originalSize == nullopt) {
        ss << " " << "-"; // trick for alignment
    } else {
        double size = originalSize.value();
        // convert size into human-friendly format
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
    auto pwUID = getpwuid(info.st_uid);
    auto grGID = getgrgid(info.st_gid);
    ss << YELLOW << setw(10) << (pwUID == nullptr ? to_string(info.st_uid) : pwUID->pw_name) << " " // username
       << BRIGHT_YELLOW << setw(10) << (grGID == nullptr ? to_string(info.st_gid) : grGID->gr_name); // group name
    return ss.str();
}

auto getTime(file_time_type time) {
    using namespace chrono;
    stringstream ss;
    auto t = system_clock::to_time_t(time - file_time_type::clock::now() + system_clock::now()); // convert time
    ss << BLUE << put_time(localtime(&t), "%Y %b %d %H:%M"); // format time
    return ss.str();
}

auto getFilename(const directory_iterator &entry) {
    stringstream ss;
    auto path = entry->path();
    ss << (entry->is_directory() ? MAGENTA : RESET_COLOR) << path.filename().string();
    if (entry->is_symlink()) {
        // show original path of symbolic link
        ss << BRIGHT_CYAN << " -> " << read_symlink(path).string();
    }
    ss << RESET_COLOR;
    return ss.str();
}

// implemented by C++17 `filesystem` STL
void ls(const string &path, const string &prefix = "") {
    auto entry = directory_iterator(path);
    auto emptyEntry = end(entry);
    while (entry != emptyEntry) {
        stringstream ss;
        auto nextPath = entry->path().string();
        auto isDirectory = entry->is_directory();
        auto isSymLink = entry->is_symlink();
        // `stat` is used because `filesystem` has no APIs related to user and group
        struct stat info{};
        stat(nextPath.c_str(), &info);
        // ascii colors are used to print colors
        ss << BRIGHT_BLUE << (isSymLink ? "l" : isDirectory ? "d" : ".")
           << getPermissions(entry->status().permissions()) << " "
           << BRIGHT_MAGENTA << setw(3) << entry->hard_link_count() << " "
           << getSize((isDirectory || isSymLink) ? nullopt : optional<double>(entry->file_size())) << " "
           << getUserAndGroup(info) << " "
           << getTime(entry->last_write_time()) << " "
           << getFilename(entry) << endl;
        entry++;
        // box drawing characters are used to print result like tree
        cout << BRIGHT_BLACK << prefix << (entry == emptyEntry ? "└─" : "├─") << ss.str();
        if (recursive && isDirectory) {
            ls(nextPath, prefix + (entry == emptyEntry ? " " : "│") + " ");
        }
    }
}

// implemented by native C APIs
void lss(const string &path, const string &prefix = "") {
    string realPath = realpath(path.c_str(), nullptr);
    realPath += "/";
    chdir(realPath.c_str());
    auto dir = opendir(realPath.c_str());
    if (dir == nullptr) {
        throw exception();
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
        auto name = entry->d_name;
        struct stat info{};
        stat(name, &info);
        auto isDirectory = entry->d_type == DT_DIR;
        auto isSymLink = entry->d_type == DT_LNK;
        auto fullPath = realPath + name;
        cout << prefix
             << (entry == entries.back() ? "└─" : "├─")
             << BRIGHT_BLUE << (isSymLink ? "l" : isDirectory ? "d" : ".")
             << getPermissions(static_cast<perms>(info.st_mode)) << " "
             << BRIGHT_MAGENTA << setw(3) << info.st_nlink << " "
             << getSize((isSymLink || isDirectory) ? nullopt : optional<double>(info.st_size)) << " "
             << getUserAndGroup(info) << " "
             << BLUE << put_time(localtime(&(info.st_mtime)), "%Y %b %d %H:%M ")
             << (isDirectory ? MAGENTA : RESET_COLOR) << name
             << BRIGHT_CYAN << (isSymLink ? " -> " + read_symlink(fullPath).string() : "")
             << RESET_COLOR << endl;
        if (recursive && isDirectory) {
            lss(fullPath, prefix + (entry == entries.back() ? " " : "│") + " ");
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
            for (const auto &path: paths) {
                ls(path);
            }
        }
    } catch (...) {
        cout << "Failed to get directory info!" << endl;
    }
}