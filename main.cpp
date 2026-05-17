#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <optional>

namespace fs = std::filesystem;
std::fstream wowExe;
constexpr std::streamsize kExpectedSize = 0x757C00;

using patch_t = std::tuple<std::streampos, std::vector<std::uint8_t>, std::vector<std::uint8_t> >;

bool handleStreamError(const std::ostream &stream, const std::string &message) {
    if (!stream) {
        std::cerr << message << std::endl;
        return true;
    }
    return false;
}

// Utility function to write data at a specific position (for ranges)
template<typename T>
/**
 * Writes a sequence of bytes to a specific position in a binary stream.
 *
 * This function attempts to write a sequence of byte values starting at a specified
 * position in an external file stream referred to by the global variable `wowExe`.
 * If the stream is not open, an error message is displayed and the function returns.
 * The function checks for errors at each step: repositioning the write pointer
 * and writing each value.
 *
 * @tparam T The type of elements in the vector to be written to the stream.
 * @param pos The position in the stream where the writing should start.
 * @param values A vector containing the values to be written to the stream.
 */
void writeBytesAt(const std::streampos pos, const std::vector<T> &values) {
    if (!wowExe) {
        std::cerr << "Stream is not open\n";
        return;
    }
    wowExe.clear();
    wowExe.seekp(pos);
    if (handleStreamError(wowExe, "Failed to set position in the stream"))
        return;

    for (const auto &value: values) {
        wowExe.write(reinterpret_cast<const char *>(&value), sizeof(value));
        if (handleStreamError(wowExe, "Failed to write to the stream"))
            return;
    }
}

/**
 * @brief Writes a byte to a specific position in a file.
 *
 * This function writes a single byte to a given position in the already opened file stream `wowExe`.
 * If the file is not open, it prints an error message and returns immediately.
 * It clears the stream's error flags, seeks to the specified position, and performs the write operation.
 * After seeking and writing, it checks for stream errors using `handleStreamError`.
 *
 * @param pos The position in the file where the byte should be written.
 * @param value The byte value to write to the file.
 */
void writeByteAt(const std::streampos &pos, const uint8_t value) {
    if (!wowExe.is_open()) {
        std::cerr << "File is not open.\n";
        return;
    }
    wowExe.clear();
    wowExe.seekp(pos);
    if (handleStreamError(wowExe, "Unable to seek position"))
        return;

    wowExe.write(reinterpret_cast<const char *>(&value), sizeof(value));
    handleStreamError(wowExe, "Unable to write to file");
}

/**
 * Writes a specified number of repeated bytes to a file stream at a given position.
 *
 * This function writes `n` bytes with the value `value` to the `wowExe` file stream starting at the specified position `pos`.
 * It first checks if the file stream `wowExe` is open. Then, it attempts to seek to the given position. If seeking to the
 * position fails, or if the write operation itself fails, appropriate error messages are displayed.
 *
 * @param pos  The position in the file to start writing bytes.
 * @param value The byte value to be written repeatedly.
 * @param n The number of bytes to write.
 */
void writeRepeatedBytesAt(const std::streampos &pos, const uint8_t value, const size_t n) {
    if (!wowExe) {
        std::cerr << "File stream is not open.\n";
        return;
    }
    wowExe.clear();
    wowExe.seekp(pos);
    if (handleStreamError(wowExe, "Failed to seek to position"))
        return;

    const std::vector buffer(n, value);
    const auto dataSize = static_cast<std::streamsize>(buffer.size());
    wowExe.write(reinterpret_cast<const char*>(buffer.data()), dataSize);
    handleStreamError(wowExe, "Write operation failed");
}

/**
 * @brief Creates a backup of the specified file.
 *
 * This function attempts to create a backup copy of the file specified by `filepath`.
 * The backup file will have a ".backup" extension appended to the original file name.
 * If the backup is successfully created, the path to the backup file is returned.
 * If the backup creation fails, an empty optional is returned.
 *
 * @param filepath The path to the file that needs to be backed up.
 * @return A std::optional containing the backup file path if the backup is successful;
 *         otherwise, an empty std::optional.
 */
[[nodiscard]] std::optional<std::string> createBackup(const std::string &filepath) {
    std::string backupPath = filepath + ".backup";
    try {
        fs::copy(filepath, backupPath, fs::copy_options::overwrite_existing);
        std::cout << "Backup created at: " << backupPath << "\n";
        return backupPath;
    } catch (const std::exception &e) {
        std::cerr << "Failed to create backup: " << e.what() << "\n";
        return std::nullopt;
    }
}

/**
 * Validates whether the given file path points to a valid executable file.
 *
 * This function performs the following checks on the specified file:
 *  1. Verifies that the file exists.
 *  2. Opens the file in binary mode to ensure it is accessible.
 *  3. Checks that the file size matches the expected size.
 *
 * @param filepath The path to the executable file to be validated.
 * @return true if the executable is valid, false otherwise.
 */
[[nodiscard]] bool validateExecutable(const std::string &filepath) {
    if (!fs::exists(filepath)) {
        std::cerr << "Executable not found.\n";
        return false;
    }
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open executable for validation.\n";
        return false;
    }
    file.seekg(0, std::ios::end);
    if (file.tellg() != kExpectedSize) {
        std::cerr << "Validation failed: unexpected file size.\n";
        return false;
    }
    std::cout << "Executable validation passed.\n";
    return true;
}

/**
 * Restores a backup of a file.
 *
 * This function checks if a backup file with the same name as the input file
 * parameter exists but with a ".backup" extension. If it exists, the function
 * attempts to remove the original file and rename the backup file to the
 * original file name.
 *
 * @param wow The path to the original file for which the backup should be restored.
 * @return true if the backup was successfully restored, false otherwise.
 */
bool restoreBackup(const std::string &wow) {
    if (const std::string backupPath = wow + ".backup"; fs::exists(backupPath)) {
        try {
            fs::remove(wow);
            fs::rename(backupPath, wow);
            return true;
        } catch (const fs::filesystem_error &e) {
            std::cerr << "Failed to restore backup: " << e.what() << "\n";
            return false;
        }
    }
    std::cerr << "Backup not found.\n";
    return false;
}

/**
 * @brief Main function to patch the World of Warcraft executable.
 *
 * This function performs several operations to patch the World of Warcraft executable
 * located at the provided path. It verifies if the path is valid, creates a backup
 * of the executable, validates the executable before patching, and applies several
 * patches to it. The patches include fixes such as resolving a remote code execution
 * exploit, enabling full screen mode from windowed mode, making certain animations
 * and actions consistent, among others.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments, where `argv[1]` should be the path
 * to the World of Warcraft executable.
 *
 * @return An integer indicating the status of the execution. Returns `EXIT_SUCCESS`
 * if patching is completed successfully, or `EXIT_FAILURE` if an error occurs at any
 * stage of the process.
 */
int main(const int argc, char **argv) {
    constexpr int errorState = EXIT_SUCCESS;
    if (argc < 2) {
        std::cerr << "World of Warcraft exe path not provided!\n";
        return EXIT_FAILURE;
    }

    const std::string wowPath = argv[1];

    if (!fs::exists(wowPath)) {
        std::cerr << "Executable not found at: " << wowPath << "\n";
        return EXIT_FAILURE;
    }

    if (!createBackup(wowPath)) {
        std::cerr << "Backup creation failed. Aborting.\n";
        return EXIT_FAILURE;
    }

    if (!validateExecutable(wowPath)) {
        std::cerr << "Executable validation failed. Aborting.\n";
        return EXIT_FAILURE;
    }

    wowExe.open(wowPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!wowExe) {
        std::cerr << "Failed to open executable for patching.\n";
        return EXIT_FAILURE;
    }

    std::vector<patch_t> patches = {
        // Remote code execution exploit
{0x2a7, {0xe0}, {0xc0}},
        // Windowed mode to full screen
{0xe94, {0x74}, {0xeb}},
        // Melee swing on right-click
{0x2e1c67, {0x6a,0xff,0x6a,0x40,0x8b,0xce,0xe8,0xbe,0x83,0x5,0x0}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}},
        // NPC attack animation when turning
{0x33d7c9, {0x74}, {0xeb}},
        // "Ghost" attack when NPC evades combat
{0x355bf, {0xe8}, {0xeb}},
        // Missing pre-cast animation for spells
{0x33e0d6, {0x6a,0xff,0x6a,0x0,0x8b,0xce,0xe8,0x4f,0xbf,0xff,0xff,0x8d,0x8d,0x58,0xfd,0xff,0xff,0xe8,0x84,0xfe,0xea,0xff}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}},
        // Patch mail timeout
{0x16d899, {0x5,0x60,0xea,0x0,0x0}, {0x5,0x1,0x0,0x0,0x0}},
        // Area trigger timer precision
{0x2db241, {0x64}, {0x32}},
        // Blue Moon
{0x5cfbc0, {0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}, {0xc7,0x5,0x74,0x8e,0xd3,0x0,0xff,0xff,0xff,0xff,0xc3}},
        // Mouse flickering and camera snapping issue when mouse has high report rate
{0x469a2c, {0x8b,0x45,0xf0,0x8b,0x15,0xec,0x13,0xd4,0x0,0x8b,0x1d,0xf0}, {0xe9,0x71,0xf0,0xb,0x0,0xf8,0x13,0xd4,0x0,0x8b,0x1d,0xfc}},
{0x528aa2, {0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}, {0x8d,0x4d,0xf0,0x51,0x57,0xff,0x15,0xdc,0xf5,0x9d,0x0,0x8b,0x45,0xf0,0x8b,0x15,0xf8,0x13,0xd4,0x0,0xe9,0x7a,0xf,0xf4,0xff}},
{0x4691b1, {0x8b,0xec,0x83,0xec,0x10,0x8d,0x45,0xf0,0x50,0x6a,0x0,0xe8,0xdf,0x28,0x0,0x0,0x83,0xc4,0x4,0x50,0xff,0x15,0xc,0xf6,0x9d,0x0,0x8b,0x45,0xf8,0x99,0x2b,0xc2,0x8b,0xc8,0x8b,0x45,0xfc,0x99,0x2b,0xc2,0xd1,0xf8,0xd1,0xf9,0x50,0x51,0x89,0xd,0xec,0x13,0xd4,0x0,0xa3,0xf0,0x13,0xd4,0x0,0xe8,0x81,0xee,0xff,0xff,0x83,0xc4,0x8,0x8b,0xe5,0x5d,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}, {0x89,0xe5,0x8b,0x5,0xfc,0x13,0xd4,0x0,0x8b,0xd,0xf8,0x13,0xd4,0x0,0xeb,0xc2,0x7d,0x3,0x83,0xc1,0x1,0x83,0xc0,0x32,0x83,0xc1,0x32,0x3b,0xd,0xec,0xbc,0xca,0x0,0x7e,0x3,0x83,0xe9,0x1,0x3b,0x5,0xf0,0xbc,0xca,0x0,0x7e,0x3,0x83,0xe8,0x1,0x83,0xe9,0x32,0x83,0xe8,0x32,0x89,0xd,0xf8,0x13,0xd4,0x0,0x89,0x5,0xfc,0x13,0xd4,0x0,0x89,0xec,0x5d,0xe9,0xb4,0xf7,0xff,0xff,0xec,0x5d,0xc3,0xc3}},

{0x469183, {0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}, {0x83,0xf8,0x32,0x7d,0x3,0x83,0xc0,0x1,0x83,0xf9,0x32,0xeb,0x31}},
    // 4gb aware
        {0x0126, {0x03}, {0x23}},
    // awesomewotlk
{0xABD0, {0x55,0x8B,0xEC,0xE8,0x98,0x85,0xFF,0xFF}, {0xE9,0xDB,0xA4,0x0D,0x00,0x90,0x90,0x90}},
{0xDC0F0, {0x55,0x8B,0xEC,0x56,0x8B,0x75}, {0xB8,0x00,0x00,0x00,0x00,0xC3}},
{0xE50B0, {0x55,0x8B,0xEC,0x56,0x33,0xF6,0x39,0x35,0x6C,0xB4,0xB6,0x00,0x0F,0x85,0xDB,0x01}, {0xB8,0x01,0x00,0x00,0xA3,0x74,0xB4,0xB6,0x00,0x68,0xE0,0x5C,0x4E,0x00,0xE8}},
    //
{0x123676, {0x8B,0xCE,0xE8,0xA3,0x18,0x1F,0x00}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90}},
{0x1241C6, {0x50}, {0x90}},
{0x1241C9, {0xE8,0x82,0xC0,0x1F,0x00}, {0x90,0x90,0x90,0x90,0x90}},
{0x320273, {0x0F,0x85,0xFE,0x00,0x00,0x00}, {0x90,0x90,0x90,0x90,0x90,0x90}},
{0x320282, {0x0F,0x85,0xEF,0x00,0x00,0x00}, {0x90,0x90,0x90,0x90,0x90,0x90}},
    // disable cache
{0x61BE58, {0x43,0x61}, {0x7C,0x7C}},
    // allow interface edits
{0x1F41BF, {0x74}, {0xEB}},
{0x415A25, {0x75}, {0xEB}},
{0x415A3F, {0x01}, {0x03}},
{0x415A95, {0x01}, {0x03}},
{0x415B46, {0x7F}, {0xEB}},
{0x415B5F, {0x83,0xC0,0x03,0x5E,0x8B,0xE5,0x5D}, {0xB8,0x03,0x00,0x00,0x00,0xEB,0xED}},
    };

    for (const auto &[pos, expected, data]: patches) {
        writeBytesAt(pos, data);
    }

    wowExe.close();
    std::cout << "Patching completed successfully.\n";

    return errorState;
}
