#include "archive.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void write_u32(std::ofstream& output, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        output.put(static_cast<char>((value >> shift) & 0xff));
    }
}

// Uncompressed KCAP archive with the given (name, contents) entries.
void write_kcap(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& files)
{
    std::ofstream output(path, std::ios::binary);
    output.write("KCAP", 4);
    write_u32(output, static_cast<std::uint32_t>(files.size()));
    std::uint32_t offset =
        8 + static_cast<std::uint32_t>(files.size()) * (4 + 24 + 4 + 4);
    for (const auto& [name, contents] : files) {
        write_u32(output, 0);
        std::string padded = name;
        padded.resize(24, '\0');
        output.write(padded.data(), 24);
        write_u32(output, offset);
        write_u32(output, static_cast<std::uint32_t>(contents.size()));
        offset += static_cast<std::uint32_t>(contents.size());
    }
    for (const auto& file : files) {
        output.write(file.second.data(),
                     static_cast<std::streamsize>(file.second.size()));
    }
}

std::string read_text(const th2::Archive& archive, std::string_view name)
{
    const auto* entry = archive.find(name);
    if (!entry) {
        return "<missing>";
    }
    const auto bytes = archive.read(*entry);
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace

int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() / "th2-archive-test";
    std::filesystem::create_directories(directory);
    const auto base = directory / "SDT.PAK";
    const auto patch = directory / "patch.pak";
    write_kcap(base, {{"a.sdt", "original a"}, {"b.sdt", "original b"}});
    write_kcap(patch, {{"A.SDT", "patched a"}, {"new.tga", "new file"}});

    // patch.pak is searched first, case-insensitively, like PAC_LoadFile.
    const th2::Archive patched(base, patch);
    if (read_text(patched, "a.sdt") != "patched a") return 1;
    if (read_text(patched, "b.sdt") != "original b") return 2;
    if (read_text(patched, "new.tga") != "new file") return 3;
    if (patched.entries().size() != 2) return 4;

    // Without an update installed, the archive reads as before.
    const th2::Archive plain(base, directory / "missing.pak");
    if (read_text(plain, "a.sdt") != "original a") return 5;
    if (read_text(plain, "new.tga") != "<missing>") return 6;

    std::filesystem::remove_all(directory);
    return 0;
}
