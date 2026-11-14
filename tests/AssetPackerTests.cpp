#include "AssetPacker.h"
#include "AssetPackageIndex.h"
#include "AssetPath.h"
#include "SplitAssetData.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#define CHECK(_condition) do { if (!(_condition)) { \
    std::cerr << "Check failed at line " << __LINE__ << ": " << #_condition << '\n'; \
    return __LINE__; } } while (false)

namespace fs = std::filesystem;
using namespace lwgf::detail;
using namespace lwgf::tools;

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
                    ("lwgf-packer-test-" + std::to_string(random()) + "-" + std::to_string(random()));
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

    bool WriteFile(const fs::path& _path, std::string_view _data)
    {
        std::ofstream file(_path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(_data.data(), static_cast<std::streamsize>(_data.size()));
        file.close();
        return !file.fail();
    }

    bool CheckOriginals(const fs::path& _source, const fs::path& _package,
                        const AssetPackageIndex& _index)
    {
        SplitAssetReader reader(_package / "assets", _index.partSize, _index.totalSize, _index.id);
        if (!reader.Validate())
            return false;
        for (const auto& entry : _index.entries)
        {
            const auto path = _source / fs::path(std::u8string(entry.path.begin(), entry.path.end()));
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file || file.tellg() != static_cast<std::streamoff>(entry.size))
                return false;
            std::vector<std::byte> expected(static_cast<std::size_t>(entry.size));
            std::vector<std::byte> actual(expected.size());
            file.seekg(0, std::ios::beg);
            if (!expected.empty() && !file.read(reinterpret_cast<char*>(expected.data()),
                                                static_cast<std::streamsize>(expected.size())))
                return false;
            if (!reader.ReadAt(entry.offset, actual) || actual != expected)
                return false;
        }
        return true;
    }

    std::size_t FileCount(const fs::path& _directory)
    {
        std::size_t count = 0;
        for (const auto& item : fs::directory_iterator(_directory))
        {
            (void)item;
            ++count;
        }
        return count;
    }
}

int main()
{
    CHECK(IsValidAssetPath("sprites/player.png"));
    CHECK(IsValidAssetPath("\xed\x95\x9c\xea\xb8\x80/font.ttf"));
    CHECK(!IsValidAssetPath(""));
    CHECK(!IsValidAssetPath("/absolute"));
    CHECK(!IsValidAssetPath("C:/file"));
    CHECK(!IsValidAssetPath("a/../b"));
    CHECK(!IsValidAssetPath("a//b"));
    CHECK(!IsValidAssetPath("a/./b"));
    CHECK(!IsValidAssetPath("a/"));
    CHECK(!IsValidAssetPath("a\\b"));
    CHECK(!IsValidAssetPath("NUL.txt"));
    CHECK(!IsValidAssetPath("a/b."));
    CHECK(!IsValidAssetPath("a/b "));
    CHECK(!IsValidAssetPath("a/\xc0\xaf"));
    CHECK(!IsValidAssetPath("a/\xed\xa0\x80"));
    CHECK(!IsValidAssetPath(std::string_view("a\0b", 3)));
    {
        AssetPathRegistry registry;
        CHECK(registry.AddDirectory("Sprites"));
        CHECK(registry.AddFile("Sprites/A.png"));
        CHECK(!registry.AddFile("Sprites/a.PNG"));
        CHECK(!registry.AddFile("sprites/b.png"));
        CHECK(!registry.AddFile("Sprites"));
        CHECK(!registry.AddFile("Sprites/A.png/child"));
    }

    TempDirectory temp;
    CHECK(!temp.path.empty());
    const auto source = temp.path / "source";
    const auto output = temp.path / "package";
    fs::create_directories(source / "images");
    fs::create_directories(source / "data");
    const auto unicodePath = source / fs::path(u8"\uD55C\uAE00.bin");
    CHECK(WriteFile(source / "images" / "a.bin", std::string_view("abc\0defghij", 11)));
    CHECK(WriteFile(source / "data" / "empty.json", ""));
    CHECK(WriteFile(unicodePath, "123456789"));

    std::string error;
    constexpr auto limit = AssetPartHeaderSize + 5;
    CHECK(PackAssets(source, output, limit, error));
    CHECK(error.empty());
    CHECK(VerifyAssets(output, error));
    AssetPackageIndex first;
    CHECK(ReadAssetIndex(output / "assets.pak", first));
    CHECK(first.entries.size() == 3);
    CHECK(first.entries[0].path == "data/empty.json");
    CHECK(first.entries[0].size == 0);
    CHECK(first.entries[1].path == "images/a.bin");
    CHECK(first.entries[2].path == "\xed\x95\x9c\xea\xb8\x80.bin");
    CHECK(first.totalSize == 20);
    CHECK(first.partCrcs.size() == 4);
    CHECK(FileCount(output) == 5);
    for (int i = 1; i <= 4; ++i)
        CHECK(fs::file_size(output / ("assets." + std::to_string(i))) == limit);
    CHECK(CheckOriginals(source, output, first));

    // Invalid requests must leave the existing package untouched.
    CHECK(!PackAssets(source, output, AssetPartHeaderSize, error));
    CHECK(!error.empty());
    CHECK(!PackAssets(source, source / "nested-output", limit, error));
    CHECK(!PackAssets(source, temp.path, limit, error));
    CHECK(WriteFile(output / "unrelated.txt", "keep me"));
    CHECK(!PackAssets(source, output, limit, error));
    CHECK(fs::exists(output / "unrelated.txt"));
    fs::remove(output / "unrelated.txt");
    AssetPackageIndex unchanged;
    CHECK(ReadAssetIndex(output / "assets.pak", unchanged));
    CHECK(unchanged.id == first.id);
    CHECK(VerifyAssets(output, error));

    // Repack after deletion/modification/addition: stale part files disappear.
    fs::remove(source / "images" / "a.bin");
    fs::remove(unicodePath);
    CHECK(WriteFile(source / "data" / "empty.json", "x"));
    CHECK(WriteFile(source / "data" / "new.bin", "y"));
    CHECK(PackAssets(source, output, limit, error));
    AssetPackageIndex second;
    CHECK(ReadAssetIndex(output / "assets.pak", second));
    CHECK(second.id != first.id);
    CHECK(second.entries.size() == 2 && second.totalSize == 2);
    CHECK(second.partCrcs.size() == 1 && FileCount(output) == 2);
    CHECK(!fs::exists(output / "assets.2"));
    CHECK(CheckOriginals(source, output, second));
    CHECK(VerifyAssets(output, error));

    // Same-size corruption is caught by CLI-style full verification.
    {
        std::fstream file(output / "assets.1", std::ios::binary | std::ios::in | std::ios::out);
        CHECK(file.is_open());
        file.seekp(static_cast<std::streamoff>(AssetPartHeaderSize));
        file.put('z');
        file.close();
        CHECK(!file.fail());
    }
    CHECK(!VerifyAssets(output, error));
    CHECK(!error.empty());
    CHECK(PackAssets(source, output, limit, error));
    CHECK(VerifyAssets(output, error));

    // Empty source directory is a valid index-only package.
    fs::remove_all(source / "data");
    fs::remove_all(source / "images");
    CHECK(PackAssets(source, output, limit, error));
    AssetPackageIndex empty;
    CHECK(ReadAssetIndex(output / "assets.pak", empty));
    CHECK(empty.entries.empty() && empty.partCrcs.empty() && empty.totalSize == 0);
    CHECK(FileCount(output) == 1 && !fs::exists(output / "assets.1"));
    CHECK(VerifyAssets(output, error));

    // Try link rejection only if the host permits creating symlinks.
    CHECK(WriteFile(temp.path / "outside.bin", "external"));
    std::error_code linkError;
    fs::create_symlink(temp.path / "outside.bin", source / "link.bin", linkError);
    if (!linkError)
    {
        CHECK(!PackAssets(source, output, limit, error));
        CHECK(VerifyAssets(output, error));
    }

    // Staging must be removed after successful work and ordinary failures.
    for (const auto& item : fs::directory_iterator(temp.path))
        CHECK(!item.path().filename().string().starts_with(".lwgf-packer-"));
    return 0;
}
