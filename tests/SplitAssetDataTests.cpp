#include "SplitAssetData.h"
#include "AssetPackageIndex.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <span>
#include <system_error>

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
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            std::random_device random;
            const auto root = fs::temp_directory_path();
            for (int i = 0; i < 16; ++i)
            {
                const auto candidate = root / ("lwgf-split-" + std::to_string(stamp) +
                                                "-" + std::to_string(random()));
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
}

int main()
{
    TempDirectory temp;
    CHECK(!temp.path.empty());
    CHECK(DefaultAssetPartSize == 536870912ull);
    const auto base = temp.path / "assets";
    const PackageId id{std::byte{0x12}};
    const PackageId otherId{std::byte{0x34}};
    constexpr auto limit = AssetPartHeaderSize + 4;
    std::vector<std::uint32_t> crcs;
    const std::array<std::byte, 11> data{
        std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}, std::byte{9},
        std::byte{255}
    };

    // Four-byte parts deliberately force boundary crossing without huge fixtures.
    {
        SplitAssetWriter writer(base, limit, id);
        CHECK(writer.Write(std::span(data).first(3)));
        CHECK(writer.Write(std::span(data).subspan(3)));
        CHECK(writer.GetSize() == data.size());
        CHECK(writer.Finish());
        CHECK(writer.Finish());
        crcs = writer.GetPartCrcs();
        CHECK(crcs.size() == 3);
        CHECK(!writer.Write(data));
    }
    CHECK(fs::file_size(temp.path / "assets.1") == limit);
    CHECK(fs::file_size(temp.path / "assets.2") == limit);
    CHECK(fs::file_size(temp.path / "assets.3") == AssetPartHeaderSize + 3);
    CHECK(!fs::exists(temp.path / "assets.4"));

    {
        SplitAssetReader reader(base, limit, data.size(), id);
        std::array<std::byte, 11> all{};
        CHECK(!reader.ReadAt(0, all));
        CHECK(reader.Validate());
        CHECK(reader.VerifyPayload(crcs));
        CHECK(reader.ReadAt(0, all));
        CHECK(all == data);
        std::array<std::byte, 7> cross{};
        CHECK(reader.ReadAt(3, cross)); // Three physical files.
        for (std::size_t i = 0; i < cross.size(); ++i)
            CHECK(cross[i] == data[i + 3]);
        std::array<std::byte, 2> back{};
        CHECK(reader.ReadAt(1, back)); // Backward seek after a cross-part read.
        CHECK(back[0] == data[1] && back[1] == data[2]);
        CHECK(reader.ReadAt(data.size(), {}));
        CHECK(!reader.ReadAt(data.size(), back));
        CHECK(!reader.ReadAt(std::numeric_limits<std::uint64_t>::max(), {}));
        CHECK(!reader.ReadAt(10, back));
    }
    {
        SplitAssetWriter collision(base, limit, id);
        CHECK(!collision.Write(data));
        CHECK(!collision.Finish());
        CHECK(fs::file_size(temp.path / "assets.1") == limit);
    }
    {
        const auto exact = temp.path / "exact";
        SplitAssetWriter writer(exact, limit, id);
        CHECK(writer.Write(std::span(data).first(8)));
        CHECK(writer.Finish());
        CHECK(!fs::exists(temp.path / "exact.3"));
        SplitAssetReader reader(exact, limit, 8, id);
        CHECK(reader.Validate());
        std::array<std::byte, 1> last{};
        CHECK(reader.ReadAt(7, last));
        CHECK(last[0] == data[7]);
    }
    {
        SplitAssetWriter invalid(temp.path / "invalid", AssetPartHeaderSize, id);
        CHECK(!invalid.Write(data));
        CHECK(!invalid.Finish());
        SplitAssetReader reader(base, AssetPartHeaderSize, data.size(), id);
        CHECK(!reader.Validate());
    }
    {
        const auto empty = temp.path / "empty";
        SplitAssetWriter writer(empty, limit, id);
        CHECK(writer.Write({}));
        CHECK(writer.Finish());
        CHECK(!fs::exists(temp.path / "empty.1"));
        SplitAssetReader reader(empty, limit, 0, id);
        CHECK(reader.Validate());
        CHECK(reader.ReadAt(0, {}));
    }

    // Independent CRC known-answer test (IEEE CRC-32).
    {
        const std::string_view input = "123456789";
        Crc32 crc;
        auto bytes = std::as_bytes(std::span(input.data(), input.size()));
        crc.Update(bytes.first(4));
        crc.Update(bytes.subspan(4));
        CHECK(crc.Get() == 0xcbf43926u);
    }

    // Index round trip with cross-part entries, including an empty asset.
    AssetPackageIndex index;
    index.id = id;
    index.partSize = limit;
    index.totalSize = data.size();
    index.partCrcs = crcs;
    index.entries = {{"images/a.png", 0, 3}, {"audio/b.wav", 3, 7}, {"empty", 10, 0}, {"data/c", 10, 1}};
    const auto indexPath = temp.path / "assets.pak";
    CHECK(WriteAssetIndex(indexPath, index));
    CHECK(!WriteAssetIndex(indexPath, index));
    AssetPackageIndex loaded;
    CHECK(ReadAssetIndex(indexPath, loaded));
    CHECK(loaded.id == id && loaded.totalSize == data.size() && loaded.partSize == limit);
    CHECK(loaded.partCrcs == crcs && loaded.entries.size() == 4);
    CHECK(loaded.entries[1].path == "audio/b.wav");
    CHECK(loaded.entries[1].offset == 3 && loaded.entries[1].size == 7);
    {
        SplitAssetReader reader(base, loaded.partSize, loaded.totalSize, loaded.id);
        CHECK(reader.Validate());
        CHECK(reader.VerifyPayload(loaded.partCrcs));
        std::array<std::byte, 7> asset{};
        CHECK(reader.ReadAt(loaded.entries[1].offset, asset));
        CHECK(std::equal(asset.begin(), asset.end(), data.begin() + 3));
    }

    std::vector<std::byte> encoded;
    CHECK(EncodeAssetIndex(index, encoded));
    auto refreshIndexCrc = [](std::vector<std::byte>& _bytes)
    {
        Crc32 crc;
        crc.Update(std::span(_bytes).first(_bytes.size() - 4));
        for (std::size_t i = 0; i < 4; ++i)
            _bytes[_bytes.size() - 4 + i] = static_cast<std::byte>((crc.Get() >> (i * 8)) & 0xffu);
    };
    {
        auto bad = encoded;
        bad[30] ^= std::byte{1}; // Invalid CRC.
        CHECK(!DecodeAssetIndex(bad, loaded));
        CHECK(loaded.id == id && loaded.entries.size() == 4);
        CHECK(!DecodeAssetIndex(std::span(encoded).first(encoded.size() - 1), loaded));
        bad = encoded;
        bad[4] = std::byte{2}; // Unsupported version even with a valid CRC.
        refreshIndexCrc(bad);
        CHECK(!DecodeAssetIndex(bad, loaded));
        bad = encoded;
        bad[8] = std::byte{1}; // Unsupported flags.
        refreshIndexCrc(bad);
        CHECK(!DecodeAssetIndex(bad, loaded));
        bad = encoded;
        bad[59] = std::byte{0xff}; // Huge entry count.
        refreshIndexCrc(bad);
        CHECK(!DecodeAssetIndex(bad, loaded));
        bad = encoded;
        bad[76] = std::byte{1}; // First entry offset (60-byte header + 12-byte CRC table + 4-byte length).
        refreshIndexCrc(bad);
        CHECK(!DecodeAssetIndex(bad, loaded));
        bad = encoded;
        bad[72] = std::byte{0xff}; // Invalid path length.
        refreshIndexCrc(bad);
        CHECK(!DecodeAssetIndex(bad, loaded));
    }
    {
        auto invalid = index;
        invalid.entries[1].path = invalid.entries[0].path;
        CHECK(!EncodeAssetIndex(invalid, encoded));
        invalid = index;
        invalid.entries[1].offset = std::numeric_limits<std::uint64_t>::max();
        CHECK(!EncodeAssetIndex(invalid, encoded));
        invalid = index;
        invalid.partCrcs.pop_back();
        CHECK(!EncodeAssetIndex(invalid, encoded));
    }

    // A different generation with identical size must not pass fast validation.
    {
        const auto other = temp.path / "other";
        SplitAssetWriter writer(other, limit, otherId);
        CHECK(writer.Write(data));
        CHECK(writer.Finish());
        fs::copy_file(temp.path / "assets.2", temp.path / "saved.2");
        fs::copy_file(temp.path / "other.2", temp.path / "assets.2", fs::copy_options::overwrite_existing);
        {
            SplitAssetReader reader(base, limit, data.size(), id);
            CHECK(!reader.Validate());
        }
        fs::copy_file(temp.path / "assets.1", temp.path / "assets.2", fs::copy_options::overwrite_existing);
        {
            SplitAssetReader reader(base, limit, data.size(), id);
            CHECK(!reader.Validate()); // Same generation, wrong part number.
        }
        fs::copy_file(temp.path / "saved.2", temp.path / "assets.2", fs::copy_options::overwrite_existing);
    }

    // Same-size payload corruption is deliberately only caught by full verify.
    {
        std::fstream file(temp.path / "assets.2", std::ios::binary | std::ios::in | std::ios::out);
        CHECK(file.is_open());
        file.seekp(static_cast<std::streamoff>(AssetPartHeaderSize));
        file.put(static_cast<char>(0xff));
        file.close();
        CHECK(!file.fail());
    }
    {
        SplitAssetReader reader(base, limit, data.size(), id);
        CHECK(reader.Validate());
        CHECK(!reader.VerifyPayload(crcs));
        CHECK(!reader.VerifyPayload(std::span(crcs).first(2)));
    }

    fs::resize_file(temp.path / "assets.2", limit - 1);
    {
        SplitAssetReader reader(base, limit, data.size(), id);
        CHECK(!reader.Validate());
    }
    fs::resize_file(temp.path / "assets.2", limit);
    fs::remove(temp.path / "assets.3");
    {
        SplitAssetReader reader(base, limit, data.size(), id);
        CHECK(!reader.Validate());
    }
    return 0;
}
