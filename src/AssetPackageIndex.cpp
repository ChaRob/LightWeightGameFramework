#include "AssetPackageIndex.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace
{
    using namespace lwgf::detail;

    constexpr std::array<std::byte, 4> Magic{
        std::byte{0x4c}, std::byte{0x57}, std::byte{0x50}, std::byte{0x4b} // LWPK
    };
    constexpr std::size_t HeaderSize = 60;
    constexpr std::size_t FooterSize = 4;
    constexpr std::size_t EntryFixedSize = 20;

    template<typename T>
    void AppendLE(std::vector<std::byte>& _bytes, T _value)
    {
        for (std::size_t i = 0; i < sizeof(T); ++i)
            _bytes.push_back(static_cast<std::byte>((_value >> (i * 8)) & 0xffu));
    }

    template<typename T>
    bool ReadLE(std::span<const std::byte>& _bytes, T& _value)
    {
        if (_bytes.size() < sizeof(T))
            return false;
        _value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
            _value |= static_cast<T>(std::to_integer<unsigned int>(_bytes[i])) << (i * 8);
        _bytes = _bytes.subspan(sizeof(T));
        return true;
    }

    // Minimum metadata invariants only; filesystem normalization and Windows
    // case-collision policy belong to the Packer/asset path layer.
    bool ValidEntry(const AssetIndexEntry& _entry)
    {
        return !_entry.path.empty() &&
            _entry.path.size() <= std::numeric_limits<std::uint32_t>::max() &&
            _entry.path.find('\0') == std::string::npos;
    }

    bool ValidateIndex(const AssetPackageIndex& _index, std::size_t& _encodedSize)
    {
        if (!IsValidPackageId(_index.id) || !IsValidPartLimit(_index.partSize) ||
            _index.partCrcs.size() != PartCount(_index.totalSize, _index.partSize))
            return false;

        constexpr auto overhead = HeaderSize + FooterSize;
        if (_index.partCrcs.size() > (MaxAssetIndexSize - overhead) / 4)
            return false;
        std::size_t size = overhead + _index.partCrcs.size() * 4;
        if (_index.entries.size() > (MaxAssetIndexSize - size) / (EntryFixedSize + 1))
            return false;

        std::uint64_t offset = 0;
        std::unordered_set<std::string_view> paths;
        for (const auto& entry : _index.entries)
        {
            if (!ValidEntry(entry) || entry.offset != offset ||
                entry.size > _index.totalSize - offset ||
                !paths.insert(entry.path).second ||
                MaxAssetIndexSize - size < EntryFixedSize ||
                entry.path.size() > MaxAssetIndexSize - size - EntryFixedSize)
                return false;
            offset += entry.size;
            size += EntryFixedSize + entry.path.size();
        }
        if (offset != _index.totalSize)
            return false;
        _encodedSize = size;
        return true;
    }
}

namespace lwgf::detail
{
    bool EncodeAssetIndex(const AssetPackageIndex& _index, std::vector<std::byte>& _bytes)
    {
        std::size_t size = 0;
        if (!ValidateIndex(_index, size))
            return false;

        std::vector<std::byte> bytes;
        bytes.reserve(size);
        bytes.insert(bytes.end(), Magic.begin(), Magic.end());
        AppendLE(bytes, AssetPackageVersion);
        AppendLE(bytes, std::uint32_t{0}); // flags: V1 uncompressed
        bytes.insert(bytes.end(), _index.id.begin(), _index.id.end());
        AppendLE(bytes, _index.partSize);
        AppendLE(bytes, _index.totalSize);
        AppendLE(bytes, static_cast<std::uint64_t>(_index.partCrcs.size()));
        AppendLE(bytes, static_cast<std::uint64_t>(_index.entries.size()));

        for (const auto crc : _index.partCrcs)
            AppendLE(bytes, crc);
        for (const auto& entry : _index.entries)
        {
            AppendLE(bytes, static_cast<std::uint32_t>(entry.path.size()));
            AppendLE(bytes, entry.offset);
            AppendLE(bytes, entry.size);
            const auto path = std::as_bytes(std::span(entry.path.data(), entry.path.size()));
            bytes.insert(bytes.end(), path.begin(), path.end());
        }

        Crc32 crc;
        crc.Update(bytes);
        AppendLE(bytes, crc.Get());
        _bytes = std::move(bytes);
        return true;
    }

    bool DecodeAssetIndex(std::span<const std::byte> _bytes, AssetPackageIndex& _index)
    {
        if (_bytes.size() < HeaderSize + FooterSize || _bytes.size() > MaxAssetIndexSize ||
            !std::equal(Magic.begin(), Magic.end(), _bytes.begin()))
            return false;

        auto footer = _bytes.last(FooterSize);
        std::uint32_t expectedCrc = 0;
        if (!ReadLE(footer, expectedCrc))
            return false;
        auto body = _bytes.first(_bytes.size() - FooterSize);
        Crc32 crc;
        crc.Update(body);
        if (crc.Get() != expectedCrc)
            return false;

        body = body.subspan(Magic.size());
        std::uint32_t version = 0;
        std::uint32_t flags = 0;
        if (!ReadLE(body, version) || version != AssetPackageVersion ||
            !ReadLE(body, flags) || flags != 0)
            return false;

        AssetPackageIndex index;
        std::copy_n(body.begin(), index.id.size(), index.id.begin());
        body = body.subspan(index.id.size());

        std::uint64_t partCount = 0;
        std::uint64_t entryCount = 0;
        if (!ReadLE(body, index.partSize) || !ReadLE(body, index.totalSize) ||
            !ReadLE(body, partCount) || !ReadLE(body, entryCount) ||
            !IsValidPackageId(index.id) || !IsValidPartLimit(index.partSize) ||
            partCount != PartCount(index.totalSize, index.partSize) ||
            partCount > body.size() / 4)
            return false;
        const auto crcBytes = static_cast<std::size_t>(partCount) * 4;
        if (entryCount > (body.size() - crcBytes) / (EntryFixedSize + 1))
            return false;

        index.partCrcs.reserve(static_cast<std::size_t>(partCount));
        for (std::uint64_t i = 0; i < partCount; ++i)
        {
            std::uint32_t partCrc = 0;
            if (!ReadLE(body, partCrc))
                return false;
            index.partCrcs.push_back(partCrc);
        }
        // Do not reserve from an untrusted count; parse one entry at a time.
        for (std::uint64_t i = 0; i < entryCount; ++i)
        {
            std::uint32_t length = 0;
            AssetIndexEntry entry;
            if (!ReadLE(body, length) || !ReadLE(body, entry.offset) ||
                !ReadLE(body, entry.size) || length == 0 || length > body.size())
                return false;
            entry.path.assign(reinterpret_cast<const char*>(body.data()), length);
            body = body.subspan(length);
            index.entries.push_back(std::move(entry));
        }
        std::size_t encodedSize = 0;
        if (!body.empty() || !ValidateIndex(index, encodedSize))
            return false;

        _index = std::move(index);
        return true;
    }

    bool WriteAssetIndex(const std::filesystem::path& _path, const AssetPackageIndex& _index)
    {
        std::vector<std::byte> bytes;
        if (!EncodeAssetIndex(_index, bytes))
            return false;
        std::error_code error;
        const auto status = std::filesystem::symlink_status(_path, error);
        if (error && error != std::errc::no_such_file_or_directory)
            return false;
        if (status.type() != std::filesystem::file_type::not_found)
            return false;

        std::ofstream file(_path, std::ios::binary | std::ios::out);
        if (!file)
            return false;
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        file.close();
        return !file.fail();
    }

    bool ReadAssetIndex(const std::filesystem::path& _path, AssetPackageIndex& _index)
    {
        std::ifstream file(_path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;
        const auto size = file.tellg();
        if (size < static_cast<std::streamoff>(HeaderSize + FooterSize) ||
            size > static_cast<std::streamoff>(MaxAssetIndexSize))
            return false;
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size())))
            return false;
        return DecodeAssetIndex(bytes, _index);
    }
}
