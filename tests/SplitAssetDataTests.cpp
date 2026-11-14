#include "SplitAssetData.h"

#include <array>
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
    const std::array<std::byte, 11> data{
        std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}, std::byte{9},
        std::byte{255}
    };

    // Four-byte parts deliberately force boundary crossing without huge fixtures.
    {
        SplitAssetWriter writer(base, 4);
        CHECK(writer.Write(std::span(data).first(3)));
        CHECK(writer.Write(std::span(data).subspan(3)));
        CHECK(writer.GetSize() == data.size());
        CHECK(writer.Finish());
        CHECK(writer.Finish());
        CHECK(!writer.Write(data));
    }
    CHECK(fs::file_size(temp.path / "assets.1") == 4);
    CHECK(fs::file_size(temp.path / "assets.2") == 4);
    CHECK(fs::file_size(temp.path / "assets.3") == 3);
    CHECK(!fs::exists(temp.path / "assets.4"));

    {
        SplitAssetReader reader(base, 4, data.size());
        std::array<std::byte, 11> all{};
        CHECK(!reader.ReadAt(0, all));
        CHECK(reader.Validate());
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
        SplitAssetWriter collision(base, 4);
        CHECK(!collision.Write(data));
        CHECK(!collision.Finish());
        CHECK(fs::file_size(temp.path / "assets.1") == 4);
    }
    {
        const auto exact = temp.path / "exact";
        SplitAssetWriter writer(exact, 4);
        CHECK(writer.Write(std::span(data).first(8)));
        CHECK(writer.Finish());
        CHECK(!fs::exists(temp.path / "exact.3"));
        SplitAssetReader reader(exact, 4, 8);
        CHECK(reader.Validate());
        std::array<std::byte, 1> last{};
        CHECK(reader.ReadAt(7, last));
        CHECK(last[0] == data[7]);
    }
    {
        SplitAssetWriter invalid(temp.path / "invalid", 0);
        CHECK(!invalid.Write(data));
        CHECK(!invalid.Finish());
        SplitAssetReader reader(base, 0, data.size());
        CHECK(!reader.Validate());
    }
    {
        const auto empty = temp.path / "empty";
        SplitAssetWriter writer(empty, 4);
        CHECK(writer.Write({}));
        CHECK(writer.Finish());
        CHECK(!fs::exists(temp.path / "empty.1"));
        SplitAssetReader reader(empty, 4, 0);
        CHECK(reader.Validate());
        CHECK(reader.ReadAt(0, {}));
    }

    fs::resize_file(temp.path / "assets.2", 3);
    {
        SplitAssetReader reader(base, 4, data.size());
        CHECK(!reader.Validate());
    }
    fs::resize_file(temp.path / "assets.2", 4);
    fs::remove(temp.path / "assets.3");
    {
        SplitAssetReader reader(base, 4, data.size());
        CHECK(!reader.Validate());
    }
    return 0;
}
