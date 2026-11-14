#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ios>
#include <span>

namespace lwgf::detail
{
    using PackageId = std::array<std::byte, 16>;
    inline constexpr std::uint32_t AssetPackageVersion = 1;
    inline constexpr std::uint64_t DefaultAssetPartSize = 512ull * 1024 * 1024;
    inline constexpr std::size_t AssetPartHeaderSize = 32;

    inline constexpr auto Crc32Table = []
    {
        std::array<std::uint32_t, 256> table{};
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            auto value = i;
            for (int bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
            table[i] = value;
        }
        return table;
    }();

    // IEEE CRC-32, incremental; no allocation and no external dependency.
    class Crc32
    {
    public:
        void Update(std::span<const std::byte> _bytes)
        {
            for (const auto byte : _bytes)
                m_state = (m_state >> 8) ^
                    Crc32Table[(m_state ^ std::to_integer<unsigned int>(byte)) & 0xffu];
        }

        std::uint32_t Get() const { return m_state ^ 0xffffffffu; }

    private:
        std::uint32_t m_state = 0xffffffffu;
    };

    inline bool IsValidPackageId(const PackageId& _id)
    {
        for (const auto value : _id)
            if (value != std::byte{0})
                return true;
        return false;
    }

    inline bool IsValidPartLimit(std::uint64_t _limit)
    {
        return _limit > AssetPartHeaderSize &&
            _limit <= static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    }

    // Precondition: valid part limit. Logical offsets exclude all part headers.
    inline std::uint64_t PartCount(std::uint64_t _totalSize, std::uint64_t _limit)
    {
        const auto capacity = _limit - AssetPartHeaderSize;
        return _totalSize / capacity + (_totalSize % capacity != 0 ? 1 : 0);
    }

    inline std::array<std::byte, AssetPartHeaderSize> MakePartHeader(
        const PackageId& _id, std::uint64_t _number)
    {
        std::array<std::byte, AssetPartHeaderSize> header{
            std::byte{0x4c}, std::byte{0x57}, std::byte{0x50}, std::byte{0x44} // LWPD
        };
        for (std::size_t i = 0; i < 4; ++i)
            header[4 + i] = static_cast<std::byte>((AssetPackageVersion >> (i * 8)) & 0xffu);
        for (std::size_t i = 0; i < _id.size(); ++i)
            header[8 + i] = _id[i];
        for (std::size_t i = 0; i < 8; ++i)
            header[24 + i] = static_cast<std::byte>((_number >> (i * 8)) & 0xffu);
        return header;
    }
}
