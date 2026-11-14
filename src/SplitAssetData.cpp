#include "SplitAssetData.h"
#include "AssetPath.h"

#include <algorithm>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace
{
    std::filesystem::path PartPath(const std::filesystem::path& _base, std::uint64_t _number)
    {
        auto path = _base;
        path += "." + std::to_string(_number);
        return path;
    }

    std::size_t ChunkSize(std::size_t _remaining, std::uint64_t _available)
    {
        const auto limit = static_cast<std::uint64_t>(
            std::numeric_limits<std::streamsize>::max());
        return static_cast<std::size_t>(std::min<std::uint64_t>(
            _remaining, std::min(_available, limit)));
    }
}

namespace lwgf::detail
{
    SplitAssetWriter::SplitAssetWriter(std::filesystem::path _base, std::uint64_t _partSize,
                                       PackageId _id)
        : m_base(std::move(_base)), m_partSize(_partSize), m_id(_id),
          m_failed(!IsValidPartLimit(_partSize) || m_base.empty() || !IsValidPackageId(_id))
    {
    }

    bool SplitAssetWriter::ClosePart()
    {
        if (!m_file.is_open())
            return true;
        m_file.close();
        if (m_file.fail())
            return false;
        if (!m_failed)
            m_partCrcs.push_back(m_crc.Get());
        return true;
    }

    bool SplitAssetWriter::OpenPart(std::uint64_t _number)
    {
        if (!ClosePart())
            return false;
        const auto path = PartPath(m_base, _number);
        // Never truncate an existing part. Caller supplies a private staging area.
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory)
            return false;
        if (status.type() != std::filesystem::file_type::not_found)
            return false;
        m_file.open(path, std::ios::binary | std::ios::out);
        if (!m_file)
            return false;

        const auto header = MakePartHeader(m_id, _number);
        m_file.write(reinterpret_cast<const char*>(header.data()),
                     static_cast<std::streamsize>(header.size()));
        m_crc = Crc32{};
        return !m_file.fail();
    }

    bool SplitAssetWriter::Write(std::span<const std::byte> _bytes)
    {
        if (m_failed || m_finished)
            return false;
        if (_bytes.size() > std::numeric_limits<std::uint64_t>::max() - m_size)
        {
            m_failed = true;
            return false;
        }

        const auto capacity = m_partSize - AssetPartHeaderSize;
        while (!_bytes.empty())
        {
            const auto local = m_size % capacity;
            if (local == 0 && !OpenPart(m_size / capacity + 1))
            {
                m_failed = true;
                return false;
            }

            const auto count = ChunkSize(_bytes.size(), capacity - local);
            m_file.write(reinterpret_cast<const char*>(_bytes.data()),
                         static_cast<std::streamsize>(count));
            if (!m_file)
            {
                m_failed = true;
                return false;
            }
            m_crc.Update(_bytes.first(count));
            m_size += count;
            _bytes = _bytes.subspan(count);
        }
        return true;
    }

    bool SplitAssetWriter::Finish()
    {
        if (!m_finished)
        {
            if (!ClosePart())
                m_failed = true;
            m_finished = true;
        }
        return !m_failed;
    }

    SplitAssetReader::SplitAssetReader(std::filesystem::path _base, std::uint64_t _partSize,
                                       std::uint64_t _totalSize, PackageId _id)
        : m_base(std::move(_base)), m_partSize(_partSize), m_totalSize(_totalSize), m_id(_id)
    {
    }

    bool SplitAssetReader::OpenVerifiedPart(std::uint64_t _number,
                                           std::uint64_t _expectedPayload,
                                           std::ifstream& _file) const
    {
        if (_file.is_open())
            _file.close();
        _file.clear();

        const auto path = PartPath(m_base, _number);
        std::error_code error;
        if (!IsPlainAssetFile(path))
            return false;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size != _expectedPayload + AssetPartHeaderSize)
            return false;

        _file.open(path, std::ios::binary);
        if (!_file)
            return false;

        std::array<std::byte, AssetPartHeaderSize> header{};
        if (!_file.read(reinterpret_cast<char*>(header.data()),
                        static_cast<std::streamsize>(header.size())) ||
            header != MakePartHeader(m_id, _number))
        {
            _file.close();
            return false;
        }
        return true;
    }

    bool SplitAssetReader::PrepareFromValidatedIndex()
    {
        m_valid = false;
        m_openPart = 0;
        if (m_file.is_open())
            m_file.close();
        m_file.clear();
        m_valid = IsValidPartLimit(m_partSize) && !m_base.empty() && IsValidPackageId(m_id);
        return m_valid;
    }

    bool SplitAssetReader::Validate()
    {
        if (!PrepareFromValidatedIndex())
            return false;
        m_valid = false;

        const auto capacity = m_partSize - AssetPartHeaderSize;
        std::uint64_t remaining = m_totalSize;
        std::uint64_t number = 1;
        std::ifstream file;
        while (remaining != 0)
        {
            const auto expected = std::min(remaining, capacity);
            if (!OpenVerifiedPart(number, expected, file))
                return false;
            remaining -= expected;
            if (remaining != 0)
                ++number;
        }
        m_valid = true;
        return true;
    }

    bool SplitAssetReader::VerifyPayload(std::span<const std::uint32_t> _partCrcs)
    {
        if (!IsValidPartLimit(m_partSize) ||
            _partCrcs.size() != PartCount(m_totalSize, m_partSize) || !Validate())
            return false;

        const auto capacity = m_partSize - AssetPartHeaderSize;
        std::array<std::byte, 64 * 1024> buffer{};
        std::uint64_t offset = 0;
        for (const auto expectedCrc : _partCrcs)
        {
            Crc32 crc;
            auto remaining = std::min(capacity, m_totalSize - offset);
            while (remaining != 0)
            {
                const auto count = ChunkSize(buffer.size(), remaining);
                auto chunk = std::span(buffer).first(count);
                if (!ReadAt(offset, chunk))
                    return false;
                crc.Update(chunk);
                offset += count;
                remaining -= count;
            }
            if (crc.Get() != expectedCrc)
                return false;
        }
        return true;
    }

    bool SplitAssetReader::ReadAt(std::uint64_t _offset, std::span<std::byte> _bytes)
    {
        if (!m_valid || _offset > m_totalSize ||
            _bytes.size() > m_totalSize - _offset)
            return false;

        const auto capacity = m_partSize - AssetPartHeaderSize;
        while (!_bytes.empty())
        {
            const auto number = _offset / capacity + 1;
            const auto local = _offset % capacity;
            if (number != m_openPart || !m_file.is_open())
            {
                m_openPart = 0;
                const auto partStart = _offset - local;
                if (!OpenVerifiedPart(number, std::min(capacity, m_totalSize - partStart), m_file))
                    return false;
                m_openPart = number;
            }
            m_file.clear();
            m_file.seekg(static_cast<std::streamoff>(local + AssetPartHeaderSize), std::ios::beg);
            const auto count = ChunkSize(_bytes.size(), capacity - local);
            if (!m_file || !m_file.read(reinterpret_cast<char*>(_bytes.data()),
                                         static_cast<std::streamsize>(count)))
            {
                m_file.close();
                m_openPart = 0;
                return false;
            }
            _offset += count;
            _bytes = _bytes.subspan(count);
        }
        return true;
    }
}
