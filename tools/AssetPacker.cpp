#include "AssetPacker.h"

#include "AssetPackageIndex.h"
#include "AssetPath.h"
#include "SplitAssetData.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    namespace fs = std::filesystem;
    using namespace lwgf::detail;

    std::string Utf8(const fs::path& _path)
    {
        const auto value = _path.generic_u8string();
        return std::string(value.begin(), value.end());
    }

    [[noreturn]] void Fail(const std::string& _reason, const fs::path& _path)
    {
        throw std::runtime_error(_reason + ": " + Utf8(_path));
    }

    fs::file_status Status(const fs::path& _path)
    {
        std::error_code error;
        const auto status = fs::symlink_status(_path, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw fs::filesystem_error("Cannot inspect path", _path, error);
        return status;
    }

    bool IsLink(const fs::path& _path, fs::file_status _status)
    {
        if (fs::is_symlink(_status))
            return true;
#ifdef _WIN32
        if (fs::exists(_status))
        {
            const auto attributes = GetFileAttributesW(_path.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
                Fail("Cannot inspect Windows file attributes", _path);
            return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        }
#else
        (void)_path;
#endif
        return false;
    }

    fs::path AbsolutePath(const fs::path& _path)
    {
        if (_path.empty())
            throw std::runtime_error("Empty filesystem path");
        auto path = fs::absolute(_path).lexically_normal();
        if (path != path.root_path() && path.filename().empty())
            path = path.parent_path();
        return path;
    }

    bool EqualComponent(const fs::path& _left, const fs::path& _right)
    {
#ifdef _WIN32
        return CompareStringOrdinal(_left.c_str(), -1, _right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
        return _left == _right;
#endif
    }

    bool IsWithin(const fs::path& _path, const fs::path& _parent)
    {
        auto it = _path.begin();
        for (const auto& component : _parent)
        {
            if (it == _path.end() || !EqualComponent(*it, component))
                return false;
            ++it;
        }
        return true;
    }

    PackageId NewPackageId()
    {
        std::random_device random;
        std::uniform_int_distribution<unsigned int> distribution(0, 255);
        PackageId id{};
        do
        {
            for (auto& value : id)
                value = static_cast<std::byte>(distribution(random));
        } while (!IsValidPackageId(id));
        return id;
    }

    std::string HexId(const PackageId& _id)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string value;
        value.reserve(_id.size() * 2);
        for (const auto byte : _id)
        {
            const auto n = std::to_integer<unsigned int>(byte);
            value.push_back(digits[n >> 4]);
            value.push_back(digits[n & 15]);
        }
        return value;
    }

    struct Workspace
    {
        fs::path path;
        bool preserve = false;
        ~Workspace()
        {
            if (!path.empty() && !preserve)
            {
                std::error_code error;
                fs::remove_all(path, error);
                if (error)
                    std::cerr << "[AssetPacker] Could not clean staging directory: "
                              << Utf8(path) << ": " << error.message() << '\n';
            }
        }
    };

    struct SourceFile
    {
        fs::path physical;
        AssetIndexEntry entry;
        fs::file_time_type modified;
    };

    std::vector<SourceFile> CollectFiles(const fs::path& _root, std::uint64_t _partSize)
    {
        std::vector<SourceFile> files;
        AssetPathRegistry registry;
        std::uint64_t total = 0;
        std::size_t indexBytes = 64; // fixed index header and CRC
        for (const auto& item : fs::recursive_directory_iterator(_root))
        {
            const auto status = item.symlink_status();
            if (IsLink(item.path(), status))
                Fail("Symlinks and reparse points are not supported", item.path());

            const auto logical = Utf8(item.path().lexically_relative(_root));
            if (fs::is_directory(status))
            {
                if (!registry.AddDirectory(logical))
                    Fail("Invalid or conflicting asset directory", item.path());
                continue;
            }
            if (!fs::is_regular_file(status))
                Fail("Only regular files are supported", item.path());
            if (!registry.AddFile(logical))
                Fail("Invalid or conflicting asset path", item.path());

            const auto size = fs::file_size(item.path());
            if (size > std::numeric_limits<std::uint64_t>::max() - total)
                Fail("Package size overflow", item.path());
            total += size;
            if (20 + logical.size() > MaxAssetIndexSize - indexBytes)
                Fail("Asset index exceeds V1 size limit", item.path());
            indexBytes += 20 + logical.size();
            if (PartCount(total, _partSize) > (MaxAssetIndexSize - indexBytes) / 4)
                Fail("Too many data parts for V1 index", item.path());
            files.push_back({item.path(), {logical, 0, static_cast<std::uint64_t>(size)},
                             fs::last_write_time(item.path())});
        }

        // Stable order independent of filesystem enumeration.
        std::sort(files.begin(), files.end(), [](const SourceFile& _a, const SourceFile& _b) {
            return _a.entry.path < _b.entry.path;
        });
        return files;
    }

    AssetPackageIndex LoadIndex(const fs::path& _directory)
    {
        const auto path = _directory / "assets.pak";
        const auto status = Status(path);
        AssetPackageIndex index;
        if (IsLink(path, status) || !fs::is_regular_file(status) || !ReadAssetIndex(path, index))
            Fail("Cannot read or validate package index", path);
        return index;
    }

    void VerifyPackage(const fs::path& _directory)
    {
        const auto index = LoadIndex(_directory);
        SplitAssetReader reader(_directory / "assets", index.partSize, index.totalSize, index.id);
        if (!reader.VerifyPayload(index.partCrcs))
            Fail("Package part header, size or payload CRC verification failed", _directory);
    }

    // Refuse to replace directories containing anything except the exact
    // package files described by a valid index. Payload damage is repairable.
    void CheckReplaceable(const fs::path& _output)
    {
        const auto index = LoadIndex(_output);
        SplitAssetReader reader(_output / "assets", index.partSize, index.totalSize, index.id);
        if (!reader.Validate())
            Fail("Existing package is incomplete; use a new output directory", _output);

        std::size_t count = 0;
        for (const auto& item : fs::directory_iterator(_output))
        {
            const auto name = Utf8(item.path().filename());
            const auto status = item.symlink_status();
            if (IsLink(item.path(), status) || !fs::is_regular_file(status))
                Fail("Output is not a dedicated package directory", item.path());
            if (name != "assets.pak")
            {
                bool valid = name.starts_with("assets.") && name.size() > 7 && name[7] != '0';
                std::uint64_t number = 0;
                for (std::size_t i = 7; valid && i < name.size(); ++i)
                {
                    const char ch = name[i];
                    if (ch < '0' || ch > '9' ||
                        number > (std::numeric_limits<std::uint64_t>::max() - (ch - '0')) / 10)
                        valid = false;
                    else
                        number = number * 10 + static_cast<unsigned int>(ch - '0');
                }
                if (!valid || number == 0 || number > index.partCrcs.size())
                    Fail("Unrelated or unexpected file in package output", item.path());
            }
            ++count;
        }
        if (count != index.partCrcs.size() + 1)
            Fail("Unexpected output directory contents", _output);
    }

    void WritePackage(const fs::path& _stage, std::vector<SourceFile>& _files,
                      std::uint64_t _partSize, const PackageId& _id)
    {
        AssetPackageIndex index;
        index.id = _id;
        index.partSize = _partSize;
        SplitAssetWriter writer(_stage / "assets", _partSize, _id);
        std::array<std::byte, 64 * 1024> buffer{};

        for (auto& source : _files)
        {
            const auto status = Status(source.physical);
            if (IsLink(source.physical, status) || !fs::is_regular_file(status) ||
                fs::file_size(source.physical) != source.entry.size ||
                fs::last_write_time(source.physical) != source.modified)
                Fail("Source asset changed during packaging", source.physical);
            std::ifstream file(source.physical, std::ios::binary);
            if (!file)
                Fail("Cannot open source asset", source.physical);

            source.entry.offset = writer.GetSize();
            auto remaining = source.entry.size;
            while (remaining != 0)
            {
                const auto count = static_cast<std::size_t>(
                    std::min<std::uint64_t>(remaining, buffer.size()));
                if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(count)))
                    Fail("Cannot read source asset", source.physical);
                if (!writer.Write(std::span(buffer).first(count)))
                    Fail("Cannot write package data for source asset", source.physical);
                remaining -= count;
            }
            if (file.peek() != std::char_traits<char>::eof() || file.bad() ||
                fs::file_size(source.physical) != source.entry.size ||
                fs::last_write_time(source.physical) != source.modified)
                Fail("Source asset changed during packaging", source.physical);
            index.entries.push_back(std::move(source.entry));
        }

        if (!writer.Finish())
            Fail("Cannot finish package data", _stage);
        index.totalSize = writer.GetSize();
        index.partCrcs = writer.GetPartCrcs();
        if (!WriteAssetIndex(_stage / "assets.pak", index))
            Fail("Cannot write package index", _stage / "assets.pak");
    }
}

namespace lwgf::tools
{
    bool PackAssets(const std::filesystem::path& _source,
                    const std::filesystem::path& _output,
                    std::uint64_t _partSize, std::string& _error)
    {
        _error.clear();
        try
        {
            if (!IsValidPartLimit(_partSize))
                throw std::runtime_error("Invalid physical part size; expected at least 33 bytes");

            const auto sourcePath = AbsolutePath(_source);
            const auto outputPath = AbsolutePath(_output);
            const auto sourceStatus = Status(sourcePath);
            const auto outputStatus = Status(outputPath);
            if (!fs::is_directory(sourceStatus) || IsLink(sourcePath, sourceStatus))
                Fail("Source must be a real directory", sourcePath);
            if (outputPath == outputPath.root_path() || IsLink(outputPath, outputStatus) ||
                (fs::exists(outputStatus) && !fs::is_directory(outputStatus)))
                Fail("Output must be a dedicated real directory", outputPath);
            if (!fs::is_directory(outputPath.parent_path()))
                Fail("Output parent directory must exist", outputPath.parent_path());

            const auto root = fs::canonical(sourcePath);
            const auto output = fs::weakly_canonical(outputPath);
            if (IsWithin(output, root) || IsWithin(root, output))
                Fail("Source and output directories must not overlap", output);

            const bool replacing = fs::exists(outputStatus);
            // An existing empty directory is safe to replace, too.
            if (replacing && !fs::is_empty(output))
                CheckReplaceable(output);

            auto files = CollectFiles(root, _partSize);
            const auto id = NewPackageId();
            Workspace workspace;
            for (int attempt = 0; attempt < 16; ++attempt)
            {
                auto candidate = output.parent_path() / (".lwgf-packer-" + HexId(NewPackageId()));
                if (fs::create_directory(candidate))
                {
                    workspace.path = std::move(candidate);
                    break;
                }
            }
            if (workspace.path.empty())
                Fail("Cannot create private staging directory", output.parent_path());

            const auto stage = workspace.path / "new";
            const auto backup = workspace.path / "previous";
            fs::create_directory(stage);
            WritePackage(stage, files, _partSize, id);
            VerifyPackage(stage); // Never publish an unverified package.

            // Same-volume directory renames. No live game or competing packer
            // may use the output during publication.
            if (replacing)
            {
                // Recheck before renaming so a newly added unrelated file is not removed.
                if (!fs::is_empty(output))
                    CheckReplaceable(output);
                fs::rename(output, backup);
            }

            std::error_code publishError;
            fs::rename(stage, output, publishError);
            if (publishError)
            {
                if (replacing)
                {
                    std::error_code rollbackError;
                    fs::rename(backup, output, rollbackError);
                    if (rollbackError)
                    {
                        workspace.preserve = true;
                        throw std::runtime_error("Publication and rollback failed; previous package preserved at " +
                                                 Utf8(backup) + ": " + rollbackError.message());
                    }
                }
                throw fs::filesystem_error("Cannot publish package", stage, output, publishError);
            }
            return true;
        }
        catch (const std::exception& _exception)
        {
            _error = _exception.what();
            return false;
        }
    }

    bool VerifyAssets(const std::filesystem::path& _directory, std::string& _error)
    {
        _error.clear();
        try
        {
            VerifyPackage(AbsolutePath(_directory));
            return true;
        }
        catch (const std::exception& _exception)
        {
            _error = _exception.what();
            return false;
        }
    }
}
