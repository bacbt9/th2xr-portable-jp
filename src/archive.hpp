#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <span>
#include <vector>

namespace th2 {

enum class ArchiveKind {
    kcap,
    lac,
};

struct ArchiveEntry {
    std::string name;
    std::uint32_t offset;
    std::uint32_t stored_size;
    bool compressed;
};

class Archive {
public:
    // `patch` names an update archive (patch.pak) that, like the original
    // PAC_LoadFile, is searched before this archive for every file. It is
    // ignored when it does not exist.
    explicit Archive(
        const std::filesystem::path& path,
        const std::filesystem::path& patch = {});

    ArchiveKind kind() const { return kind_; }
    const std::filesystem::path& path() const { return path_; }
    const std::vector<ArchiveEntry>& entries() const { return entries_; }
    const ArchiveEntry* find(std::string_view name) const;
    std::vector<std::uint8_t> read(const ArchiveEntry& entry) const;

private:
    std::filesystem::path path_;
    ArchiveKind kind_;
    std::vector<ArchiveEntry> entries_;
    std::shared_ptr<const Archive> patch_;

    bool owns(const ArchiveEntry& entry) const;
};

}  // namespace th2
