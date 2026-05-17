#include "patches.h"

#include <iomanip>
#include <iostream>
#include <filesystem>
#include <string>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
static void init_console() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    if (GetConsoleMode(h, &m))
        SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
static void init_console() {}
#endif

namespace fs = std::filesystem;

namespace C {
    constexpr auto RST  = "\033[0m";
    constexpr auto BOLD = "\033[1m";
    constexpr auto DIM  = "\033[2m";
    constexpr auto RED  = "\033[31m";
    constexpr auto GRN  = "\033[32m";
    constexpr auto YEL  = "\033[33m";
    constexpr auto CYN  = "\033[36m";
}

static std::vector<Patch> g_patches = default_patches();

static std::string read_line(const char* prompt = "") {
    if (prompt && *prompt) std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line))
        std::cin.clear();
    auto s = line.find_first_not_of(" \t\r\n");
    if (s == std::string::npos) return "";
    auto e = line.find_last_not_of(" \t\r\n");
    return line.substr(s, e - s + 1);
}

static void wait_enter() { read_line("  Press Enter to continue..."); }
static void clear_screen() { std::cout << "\033[2J\033[H" << std::flush; }

static void do_verify(const std::string& path) {
    clear_screen();
    std::cout << C::BOLD << "Verify Patches\n" << C::RST;
    std::cout << std::string(52, '-') << "\n";
    for (const auto& p : g_patches) {
        std::cout << "  " << std::left << std::setw(38) << p.name;
        switch (check_patch(path, p)) {
            case PatchStatus::Applied:
                std::cout << C::GRN << "[APPLIED  ]" << C::RST; break;
            case PatchStatus::Unpatched:
                std::cout << C::YEL << "[UNPATCHED]" << C::RST; break;
            case PatchStatus::Mismatch:
                std::cout << C::RED << "[MISMATCH ]" << C::RST; break;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
    wait_enter();
}

static void do_apply(const std::string& path) {
    // Check current state before opening for write so disabled patches with
    // Mismatch/Unpatched status are left untouched.
    std::vector<PatchStatus> statuses(g_patches.size());
    for (size_t i = 0; i < g_patches.size(); i++)
        if (!g_patches[i].enabled)
            statuses[i] = check_patch(path, g_patches[i]);

    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        std::cout << C::RED << "  Failed to open file for writing.\n" << C::RST;
        wait_enter();
        return;
    }

    std::cout << C::BOLD << "\nWriting patches...\n" << C::RST;
    int ok = 0, fail = 0;
    for (size_t i = 0; i < g_patches.size(); i++) {
        const auto& p = g_patches[i];
        bool all_ok = true;
        for (const auto& bp : p.changes) {
            const std::vector<uint8_t>* bytes = nullptr;
            if (p.enabled)
                bytes = &bp.replacement;
            else if (statuses[i] == PatchStatus::Applied)
                bytes = &bp.original;
            if (!bytes) continue;
            f.clear();
            f.seekp(bp.offset);
            if (!f) { all_ok = false; break; }
            f.write(reinterpret_cast<const char*>(bytes->data()),
                    static_cast<std::streamsize>(bytes->size()));
            if (!f) { all_ok = false; break; }
        }
        if (all_ok) {
            const char* tag = p.enabled ? "[+]" : "[-]";
            std::cout << "  " << (p.enabled ? C::GRN : C::DIM) << tag << C::RST
                      << " " << p.name << "\n";
            ok++;
        } else {
            std::cout << "  " << C::RED << "[!!]" << C::RST << " " << p.name << "\n";
            fail++;
        }
    }
    f.close();

    std::cout << "\n";
    if (fail == 0)
        std::cout << C::GRN << C::BOLD << "  Done. " << ok << " patches written.\n" << C::RST;
    else
        std::cout << C::YEL << C::BOLD << "  " << ok << " written, " << fail << " failed.\n" << C::RST;
    wait_enter();
}

static void do_toggle() {
    while (true) {
        clear_screen();
        std::cout << C::BOLD << "Toggle Patches\n" << C::RST;
        std::cout << C::DIM << "  (number to toggle | a=all on | n=all off | b=back)\n\n" << C::RST;
        for (size_t i = 0; i < g_patches.size(); i++) {
            const auto& p = g_patches[i];
            std::cout << "  " << std::setw(2) << (i + 1) << ". ";
            std::cout << (p.enabled ? C::GRN : C::DIM) << (p.enabled ? "[x]" : "[ ]") << C::RST;
            std::cout << " " << p.name << "\n";
        }
        std::cout << "\n";
        std::string in = read_line("  > ");
        if (in == "b" || in == "B" || in.empty()) break;
        if (in == "a" || in == "A") { for (auto& p : g_patches) p.enabled = true; continue; }
        if (in == "n" || in == "N") { for (auto& p : g_patches) p.enabled = false; continue; }
        try {
            int idx = std::stoi(in);
            if (idx >= 1 && idx <= static_cast<int>(g_patches.size()))
                g_patches[static_cast<size_t>(idx) - 1].enabled ^= true;
        } catch (...) {}
    }
}

static void print_header(const std::string& path) {
    std::cout << C::BOLD << C::CYN << "  WoW 3.3.5a Patcher\n"
              << "  " << std::string(36, '-') << "\n" << C::RST;
    std::cout << C::BOLD << "  File:   " << C::RST << path << "\n";

    if (!fs::exists(path)) {
        std::cout << C::RED << "  Status: NOT FOUND\n" << C::RST;
    } else {
        std::error_code ec;
        auto sz = static_cast<std::streamsize>(fs::file_size(path, ec));
        if (ec)
            std::cout << C::RED << "  Status: UNREADABLE\n" << C::RST;
        else if (sz != kExpectedFileSize)
            std::cout << C::YEL << "  Status: SIZE MISMATCH (" << sz << " vs " << kExpectedFileSize << ")\n" << C::RST;
        else
            std::cout << C::GRN << "  Status: OK (" << sz << " bytes)\n" << C::RST;
    }

    int enabled = 0;
    for (const auto& p : g_patches) if (p.enabled) enabled++;
    std::cout << C::DIM << "  Patches selected: " << enabled << "/" << g_patches.size() << "\n\n" << C::RST;
}

int main(int argc, char** argv) {
    init_console();

    std::string path;
    if (argc >= 2) {
        path = argv[1];
    } else {
        clear_screen();
        std::cout << C::BOLD << C::CYN << "  WoW 3.3.5a Patcher\n\n" << C::RST;
        path = read_line("  Path to WoW.exe: ");
    }

    if (path.empty()) {
        std::cerr << "No path provided.\n";
        return EXIT_FAILURE;
    }

    while (true) {
        clear_screen();
        print_header(path);
        std::cout << C::BOLD << "  [1]" << C::RST << " Verify patches\n";
        std::cout << C::BOLD << "  [2]" << C::RST << " Apply selected patches\n";
        std::cout << C::BOLD << "  [3]" << C::RST << " Toggle patches\n";
        std::cout << C::BOLD << "  [0]" << C::RST << " Quit\n\n";

        std::string choice = read_line("  > ");
        if      (choice == "0")                     break;
        else if (choice == "1")                     do_verify(path);
        else if (choice == "2" && fs::exists(path)) do_apply(path);
        else if (choice == "2")                   { std::cout << C::RED << "  File not found.\n" << C::RST; wait_enter(); }
        else if (choice == "3")                     do_toggle();
    }
    return EXIT_SUCCESS;
}
