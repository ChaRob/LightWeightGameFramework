#include "SplitAssetData.h"

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

    bool ValidPartSize(std::uint64_t _size)
    {
        return _size != 0 &&
            _size <= static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
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
    SplitAssetWriter::SplitAssetWriter(std::filesystem::path _base, std::uint64_t _partSize)
        : m_base(std::move(_base)), m_partSize(_partSize),
          m_failed(!ValidPartSize(_partSize) || m_base.empty())
    {
    }

    bool SplitAssetWriter::ClosePart()
    {
        if (!m_file.is_open())
            return true;
        m_file.close();
        return !m_file.fail();
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
        return m_file.is_open() && !m_file.fail();
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

        while (!_bytes.empty())
        {
            const auto local = m_size % m_partSize;
            if (local == 0 && !OpenPart(m_size / m_partSize + 1))
            {
                m_failed = true;
                return false;
            }

            const auto count = ChunkSize(_bytes.size(), m_partSize - local);
            m_file.write(reinterpret_cast<const char*>(_bytes.data()),
                         static_cast<std::streamsize>(count));
            if (!m_file)
            {
                m_failed = true;
                return false;
            }
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
                                       std::uint64_t _totalSize)
        : m_base(std::move(_base)), m_partSize(_partSize), m_totalSize(_totalSize)
    {
    }

    bool SplitAssetReader::Validate()
    {
        m_valid = false;
        m_openPart = 0;
        if (m_file.is_open())
            m_file.close();
        m_file.clear();
        if (!ValidPartSize(m_partSize) || m_base.empty())
            return false;

        // Iterate by remaining bytes to avoid overflow in ceil(total / part).
        std::uint64_t remaining = m_totalSize;
        std::uint64_t number = 1;
        while (remaining != 0)
        {
            const auto expected = std::min(remaining, m_partSize);
            const auto path = PartPath(m_base, number);
            std::error_code error;
            const auto status = std::filesystem::symlink_status(path, error);
            if (error || !std::filesystem::is_regular_file(status))
                return false;
            const auto size = std::filesystem::file_size(path, error);
            if (error || size != expected)
                return false;
            remaining -= expected;
            if (remaining != 0)
                ++number;
        }
        m_valid = true;
        return true;
    }

    bool SplitAssetReader::ReadAt(std::uint64_t _offset, std::span<std::byte> _bytes)
    {
        if (!m_valid || _offset > m_totalSize ||
            _bytes.size() > m_totalSize - _offset)
            return false;

        while (!_bytes.empty())
        {
            const auto number = _offset / m_partSize + 1;
            const auto local = _offset % m_partSize;
            if (number != m_openPart || !m_file.is_open())
            {
                m_openPart = 0;
                if (m_file.is_open())
                    m_file.close();
                m_file.clear();
                m_file.open(PartPath(m_base, number), std::ios::binary);
                if (!m_file)
                    return false;
                m_openPart = number;
            }
            m_file.clear();
            m_file.seekg(static_cast<std::streamoff>(local), std::ios::beg);
            const auto count = ChunkSize(_bytes.size(), m_partSize - local);
            if (!m_file || !m_file.read(reinterpret_cast<char*>(_bytes.data()),
                                         static_cast<std::streamsize>(count)))
            {
                // Reopen on a later retry rather than retaining a failed handle.
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
