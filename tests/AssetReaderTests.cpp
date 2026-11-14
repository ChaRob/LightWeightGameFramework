#include <framework/AssetReader.h>

#include "AssetPackageIndex.h"
#include "SplitAssetData.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#define CHECK(_condition) do { if (!(_condition)) { \
    std::cerr << "Check failed at line " << __LINE__ << ": " << #_condition << '\n'; \
    return __LINE__; } } while (false)

namespace fs = std::filesystem;
using namespace lwgf::detail;

namespace
{
    struct TempDirectory
    {
        fs::path path;
        TempDirectory()
        {
            std::random_device random;
            for (int i = 0; i < 16; ++i)
            {
                const auto candidate = fs::temp_directory_path() /
                    ("lwgf-reader-test-" + std::to_string(random()) + "-" + std::to_string(random()));
                if (fs::create_directory(candidate))
                {
                    path = candidate;
                    break;
                }
            }
        }
        ~TempDirectory()
        {
            if (!path.empty())
            {
                std::error_code error;
                fs::remove_all(path, error);
            }
        }
    };

    bool WriteFile(const fs::path& _path, std::span<const std::byte> _data)
    {
        std::ofstream file(_path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        if (!_data.empty())
            file.write(reinterpret_cast<const char*>(_data.data()),
                       static_cast<std::streamsize>(_data.size()));
        file.close();
        return !file.fail();
    }

    constexpr std::array<std::byte, 11> AssetA{
        std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}, std::byte{9}, std::byte{255}
    };
    constexpr std::array<std::byte, 4> AssetB{
        std::byte{41}, std::byte{42}, std::byte{43}, std::byte{44}
    };

    bool MakeFixture(const fs::path& _directory, const fs::path& _package)
    {
        fs::create_directories(_directory / "data");
        fs::create_directories(_package);
        if (!WriteFile(_directory / "data" / "A.bin", AssetA) ||
            !WriteFile(_directory / "data" / "B.bin", AssetB) ||
            !WriteFile(_directory / "empty.bin", {}) ||
            !WriteFile(_directory / fs::path(u8"\uD55C\uAE00.bin"), AssetB))
            return false;

        AssetPackageIndex index;
        index.id = PackageId{std::byte{0x42}};
        index.partSize = AssetPartHeaderSize + 4;
        SplitAssetWriter writer(_package / "assets", index.partSize, index.id);
        index.entries = {{"data/A.bin", 0, AssetA.size()},
                         {"data/B.bin", AssetA.size(), AssetB.size()},
                         {"empty.bin", AssetA.size() + AssetB.size(), 0},
                         {"\xed\x95\x9c\xea\xb8\x80.bin", AssetA.size() + AssetB.size(), AssetB.size()}};
        if (!writer.Write(AssetA) || !writer.Write(AssetB) ||
            !writer.Write(AssetB) || !writer.Finish())
            return false;
        index.totalSize = writer.GetSize();
        index.partCrcs = writer.GetPartCrcs();
        return WriteAssetIndex(_package / "assets.pak", index);
    }

    int CheckSource(AssetReader& _reader)
    {
        std::vector<std::byte> bytes{std::byte{0x77}};
        CHECK(_reader.ReadAll("data/A.bin", bytes));
        CHECK(std::equal(bytes.begin(), bytes.end(), AssetA.begin(), AssetA.end()));
        CHECK(_reader.ReadAll("data/B.bin", bytes));
        CHECK(std::equal(bytes.begin(), bytes.end(), AssetB.begin(), AssetB.end()));
        CHECK(_reader.ReadAll("\xed\x95\x9c\xea\xb8\x80.bin", bytes));
        CHECK(std::equal(bytes.begin(), bytes.end(), AssetB.begin(), AssetB.end()));
        CHECK(_reader.ReadAll("empty.bin", bytes));
        CHECK(bytes.empty());

        bytes = {std::byte{0x77}};
        CHECK(!_reader.ReadAll("missing.bin", bytes));
        CHECK(bytes.size() == 1 && bytes[0] == std::byte{0x77});
        CHECK(!_reader.Open("data/a.bin"));
        CHECK(!_reader.Open("Data/A.bin"));
        CHECK(!_reader.Open("assets/data/A.bin"));
        CHECK(!_reader.Open("../data/A.bin"));
        CHECK(!_reader.Open("/data/A.bin"));
        CHECK(!_reader.Open("data\\A.bin"));
        CHECK(!_reader.Open("data"));
        CHECK(!_reader.Open(""));

        auto first = _reader.Open("data/A.bin");
        auto second = _reader.Open("data/A.bin");
        auto adjacent = _reader.Open("data/B.bin");
        CHECK(first && second && adjacent);
        CHECK(first->GetSize() == AssetA.size() && first->Tell() == 0);
        CHECK(second->GetSize() == AssetA.size() && second->Tell() == 0);
        CHECK(adjacent->GetSize() == AssetB.size());

        std::array<std::byte, 16> buffer{};
        std::size_t read = 99;
        CHECK(first->Seek(3));
        CHECK(first->Read(std::span(buffer).first(7), read));
        CHECK(read == 7 && first->Tell() == 10);
        CHECK(std::equal(buffer.begin(), buffer.begin() + 7, AssetA.begin() + 3));

        CHECK(second->Tell() == 0 && adjacent->Tell() == 0);
        CHECK(second->Read(std::span(buffer).first(2), read));
        CHECK(read == 2 && buffer[0] == AssetA[0] && buffer[1] == AssetA[1]);
        CHECK(first->Tell() == 10);
        CHECK(adjacent->Read(buffer, read));
        CHECK(read == AssetB.size() && std::equal(buffer.begin(), buffer.begin() + 4, AssetB.begin()));

        // Read near EOF: must not leak adjacent asset bytes from the package.
        buffer.fill(std::byte{0x66});
        CHECK(first->Read(buffer, read));
        CHECK(read == 1 && buffer[0] == AssetA.back() && first->Tell() == AssetA.size());
        CHECK(buffer[1] == std::byte{0x66});
        CHECK(first->Read(buffer, read) && read == 0);
        CHECK(first->Seek(AssetA.size()));
        CHECK(!first->Seek(AssetA.size() + 1));
        CHECK(!first->Seek(std::numeric_limits<std::uint64_t>::max()));
        CHECK(first->Tell() == AssetA.size());
        CHECK(first->Seek(1));
        CHECK(first->Read({}, read) && read == 0 && first->Tell() == 1);
        CHECK(first->Read(std::span(buffer).first(2), read));
        CHECK(read == 2 && buffer[0] == AssetA[1] && buffer[1] == AssetA[2]);

        auto empty = _reader.Open("empty.bin");
        CHECK(empty && empty->GetSize() == 0 && empty->Tell() == 0);
        CHECK(empty->Read(buffer, read) && read == 0);
        CHECK(!empty->Seek(1));
        return 0;
    }

    int RunTests()
    {
        TempDirectory temp;
        CHECK(!temp.path.empty());
        const auto directory = temp.path / "original";
        const auto package = temp.path / "package";
        CHECK(MakeFixture(directory, package));

        AssetReader reader;
        CHECK(!reader.IsOpen() && reader.GetMode() == AssetReader::Mode::None);
        CHECK(!reader.Open("data/A.bin"));
        CHECK(!reader.OpenDirectory(temp.path / "not-found"));
        CHECK(!reader.OpenPackage(directory));
        CHECK(!reader.IsOpen());

        CHECK(reader.OpenDirectory(directory));
        CHECK(reader.IsOpen() && reader.GetMode() == AssetReader::Mode::Directory);
        CHECK(CheckSource(reader) == 0);

        // Root is captured at configuration time, not on each asset lookup.
        const auto oldCwd = fs::current_path();
        fs::current_path(temp.path);
        const bool opened = reader.OpenDirectory("original");
        fs::current_path(oldCwd);
        CHECK(opened);
        CHECK(CheckSource(reader) == 0);

        // Directory mode observes edits and new files on later opens.
        const auto fresh = directory / "fresh.bin";
        CHECK(WriteFile(fresh, AssetB));
        std::vector<std::byte> bytes;
        CHECK(reader.ReadAll("fresh.bin", bytes) && bytes.size() == AssetB.size());
        CHECK(WriteFile(fresh, AssetA));
        CHECK(reader.ReadAll("fresh.bin", bytes) && bytes.size() == AssetA.size());
        fs::remove(fresh);
        CHECK(!reader.Open("fresh.bin"));

        // Failed reconfiguration preserves an existing source; no implicit fallback.
        CHECK(!reader.OpenPackage(directory));
        CHECK(reader.GetMode() == AssetReader::Mode::Directory);
        auto directoryStream = reader.Open("data/A.bin");
        CHECK(directoryStream);
        CHECK(reader.OpenPackage(package));
        CHECK(reader.GetMode() == AssetReader::Mode::Package);
        CHECK(CheckSource(reader) == 0);
        CHECK(!reader.OpenDirectory(temp.path / "missing"));
        CHECK(reader.GetMode() == AssetReader::Mode::Package);

        // A physical file in the package directory is not a package entry.
        CHECK(WriteFile(package / "loose.bin", AssetB));
        CHECK(!reader.Open("loose.bin"));
        fs::remove(package / "loose.bin");

        // Streams own their lifetime independently of the source configuration.
        auto packageStream = reader.Open("data/A.bin");
        CHECK(packageStream);
        reader.Close();
        CHECK(!reader.IsOpen() && reader.GetMode() == AssetReader::Mode::None);
        CHECK(!reader.Open("data/A.bin"));
        std::array<std::byte, 11> buffer{};
        std::size_t read = 0;
        CHECK(packageStream->Read(buffer, read) && read == AssetA.size() && buffer == AssetA);
        CHECK(directoryStream->Read(buffer, read) && read == AssetA.size() && buffer == AssetA);
        packageStream.reset();
        directoryStream.reset();
        std::unique_ptr<AssetStream> detached;
        {
            AssetReader temporary;
            CHECK(temporary.OpenPackage(package));
            detached = temporary.Open("data/B.bin");
        }
        CHECK(detached && detached->Read(buffer, read) && read == AssetB.size());
        CHECK(std::equal(buffer.begin(), buffer.begin() + read, AssetB.begin()));
        detached.reset();

        // Directory links are rejected even if the OS allows creating them.
        std::error_code linkError;
        fs::create_symlink(directory / "data" / "A.bin", directory / "link.bin", linkError);
        if (!linkError)
        {
            CHECK(reader.OpenDirectory(directory));
            CHECK(!reader.Open("link.bin"));
            reader.Close();
            fs::remove(directory / "link.bin");
        }

        // Package initialization rejects missing data parts.
        fs::rename(package / "assets.2", package / "saved.2");
        CHECK(!reader.OpenPackage(package));
        CHECK(!reader.IsOpen());
        fs::rename(package / "saved.2", package / "assets.2");
        CHECK(reader.OpenPackage(package));
        auto missingPart = reader.Open("data/A.bin");
        CHECK(missingPart);
        // No cached file handle for part 2 yet. Failure must preserve the cursor.
        fs::rename(package / "assets.2", package / "saved.2");
        CHECK(missingPart->Seek(3));
        CHECK(!missingPart->Read(buffer, read));
        CHECK(read == 0 && missingPart->Tell() == 3);
        fs::rename(package / "saved.2", package / "assets.2");
        CHECK(missingPart->Read(buffer, read) && read == AssetA.size() - 3);
        missingPart.reset();
        reader.Close();

        // Index CRC failure rejects package before loading assets.
        {
            std::fstream file(package / "assets.pak", std::ios::binary | std::ios::in | std::ios::out);
            CHECK(file.is_open());
            file.put('X');
            file.close();
            CHECK(!file.fail());
        }
        CHECK(!reader.OpenPackage(package));

        // Index-only package is valid.
        const auto emptyPackage = temp.path / "empty-package";
        fs::create_directory(emptyPackage);
        AssetPackageIndex emptyIndex;
        emptyIndex.id = PackageId{std::byte{0x01}};
        CHECK(WriteAssetIndex(emptyPackage / "assets.pak", emptyIndex));
        CHECK(reader.OpenPackage(emptyPackage));
        CHECK(reader.GetMode() == AssetReader::Mode::Package);
        CHECK(!reader.Open("data/A.bin"));
        return 0;
    }

    // Called by the real Sample CLI test: compare both public source modes.
    int CompareSample(const fs::path& _directory, const fs::path& _package)
    {
        AssetReader original;
        AssetReader packed;
        CHECK(original.OpenDirectory(_directory));
        CHECK(packed.OpenPackage(_package));
        std::vector<std::byte> expected;
        std::vector<std::byte> actual;
        for (const auto& item : fs::recursive_directory_iterator(_directory))
        {
            if (!item.is_regular_file())
                continue;
            const auto utf8 = item.path().lexically_relative(_directory).generic_u8string();
            const std::string logical(utf8.begin(), utf8.end());
            CHECK(original.ReadAll(logical, expected));
            CHECK(packed.ReadAll(logical, actual));
            CHECK(actual == expected);

            auto stream = packed.Open(logical);
            CHECK(stream && stream->GetSize() == expected.size());
            // Exercise several non-aligned chunks, including across part boundaries.
            std::array<std::byte, 4093> buffer{};
            std::size_t offset = 0;
            for (;;)
            {
                std::size_t read = 0;
                CHECK(stream->Read(buffer, read));
                if (read == 0)
                    break;
                CHECK(read <= expected.size() - offset);
                CHECK(std::equal(buffer.begin(), buffer.begin() + read, expected.begin() + offset));
                offset += read;
            }
            CHECK(offset == expected.size());
        }
        return 0;
    }
}

#ifdef _WIN32
int wmain(int _argc, wchar_t** _argv)
#else
int main(int _argc, char** _argv)
#endif
{
    if (_argc == 1)
        return RunTests();
    if (_argc == 4 && fs::path(_argv[1]) == "--compare")
        return CompareSample(fs::path(_argv[2]), fs::path(_argv[3]));
    return 1;
}
