#include <framework/AssetReader.h>

#include "AssetPackageIndex.h"
#include "AssetPath.h"
#include "SplitAssetData.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace lwgf::detail;

    std::string Utf8(const fs::path& _path)
    {
        const auto value = _path.generic_u8string();
        return std::string(value.begin(), value.end());
    }

    void LogError(std::string_view _operation, std::string_view _path,
                  std::string_view _reason)
    {
        std::cerr << "[AssetReader] " << _operation << " '" << _path
                  << "' failed: " << _reason << '\n';
    }

    fs::path ResolveRoot(const fs::path& _root)
    {
        if (_root.empty())
            return {};
        auto root = fs::absolute(_root).lexically_normal();
        if (root != root.root_path() && root.filename().empty())
            root = root.parent_path();
        if (!IsPlainAssetDirectory(root))
            return {};
        return fs::canonical(root);
    }

    // Enumerate only the requested path's components. Exact spelling even on
    // Windows prevents Directory mode from hiding package case mismatches.
    // No directory snapshot/cache: newly added or replaced files are visible
    // on the next Open. Source mutations during a call are unsupported.
    fs::path ResolveAssetFile(const fs::path& _root, std::string_view _path)
    {
        if (!IsPlainAssetDirectory(_root))
            return {};
        auto current = _root;
        while (true)
        {
            const auto slash = _path.find('/');
            const auto component = _path.substr(0, slash);
            fs::path match;
            for (const auto& item : fs::directory_iterator(current))
            {
                if (Utf8(item.path().filename()) == component)
                {
                    match = item.path();
                    break;
                }
            }

            if (match.empty())
                return {};
            if (slash == std::string_view::npos)
                return IsPlainAssetFile(match) ? match : fs::path{};
            if (!IsPlainAssetDirectory(match))
                return {};
            current = std::move(match);
            _path.remove_prefix(slash + 1);
        }
    }
}

struct AssetStream::Impl
{
    std::string path;
    std::uint64_t size = 0;
    std::uint64_t position = 0;
    std::uint64_t packageOffset = 0;
    std::ifstream file;
    std::unique_ptr<lwgf::detail::SplitAssetReader> package;
};

struct AssetReader::Impl
{
    Mode mode = Mode::None;
    std::filesystem::path root;
    lwgf::detail::AssetPackageIndex index;
    // Views reference immutable strings owned by index.entries.
    // Impl is heap-owned and never moved; lookup is destroyed before index.
    std::unordered_map<std::string_view, std::size_t> lookup;
};

AssetStream::AssetStream(std::unique_ptr<Impl> _impl)
    : m_impl(std::move(_impl))
{
}

AssetStream::~AssetStream() = default;

std::uint64_t AssetStream::GetSize() const
{
    return m_impl->size;
}

std::uint64_t AssetStream::Tell() const
{
    return m_impl->position;
}

bool AssetStream::Seek(std::uint64_t _offset)
{
    if (_offset > m_impl->size)
    {
        LogError("Seek", m_impl->path, "offset exceeds asset size");
        return false;
    }
    m_impl->position = _offset;
    return true;
}

bool AssetStream::Read(std::span<std::byte> _buffer, std::size_t& _bytesRead)
{
    _bytesRead = 0;
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(
        _buffer.size(), m_impl->size - m_impl->position));
    if (count == 0)
        return true;

    auto bytes = _buffer.first(count);
    try
    {
        if (m_impl->package)
        {
            if (!m_impl->package->ReadAt(m_impl->packageOffset + m_impl->position, bytes))
            {
                LogError("Read", m_impl->path, "package data read failed");
                return false;
            }
        }
        else
        {
            auto& file = m_impl->file;
            file.clear();
            file.seekg(static_cast<std::streamoff>(m_impl->position), std::ios::beg);
            constexpr auto maxChunk = static_cast<std::uint64_t>(
                std::numeric_limits<std::streamsize>::max());
            while (!bytes.empty())
            {
                const auto chunk = static_cast<std::size_t>(
                    std::min<std::uint64_t>(bytes.size(), maxChunk));
                if (!file || !file.read(reinterpret_cast<char*>(bytes.data()),
                                        static_cast<std::streamsize>(chunk)))
                {
                    LogError("Read", m_impl->path, "source file read failed");
                    return false;
                }
                bytes = bytes.subspan(chunk);
            }
        }
        m_impl->position += count;
        _bytesRead = count;
        return true;
    }
    catch (const std::exception& _exception)
    {
        LogError("Read", m_impl->path, _exception.what());
        return false;
    }
}

AssetReader::AssetReader() = default;
AssetReader::~AssetReader() = default;

bool AssetReader::OpenDirectory(const std::filesystem::path& _root)
{
    try
    {
        auto root = ResolveRoot(_root);
        if (root.empty())
        {
            LogError("OpenDirectory", Utf8(_root), "root must be a real directory");
            return false;
        }

        auto next = std::make_unique<Impl>();
        next->mode = Mode::Directory;
        next->root = std::move(root);
        m_impl = std::move(next);
        return true;
    }
    catch (const std::exception& _exception)
    {
        LogError("OpenDirectory", "<root>", _exception.what());
        return false;
    }
}

bool AssetReader::OpenPackage(const std::filesystem::path& _root)
{
    try
    {
        auto root = ResolveRoot(_root);
        if (root.empty())
        {
            LogError("OpenPackage", Utf8(_root), "root must be a real directory");
            return false;
        }

        auto next = std::make_unique<Impl>();
        next->root = std::move(root);
        const auto indexPath = next->root / "assets.pak";
        if (!IsPlainAssetFile(indexPath) ||
            !ReadAssetIndex(indexPath, next->index))
        {
            LogError("OpenPackage", Utf8(indexPath),
                     "missing, unsupported or invalid package index");
            return false;
        }

        SplitAssetReader reader(next->root / "assets", next->index.partSize,
                                next->index.totalSize, next->index.id);
        if (!reader.Validate())
        {
            LogError("OpenPackage", Utf8(next->root), "missing or invalid package data part");
            return false;
        }

        next->lookup.reserve(next->index.entries.size());
        for (std::size_t i = 0; i < next->index.entries.size(); ++i)
            next->lookup.emplace(next->index.entries[i].path, i);
        next->mode = Mode::Package;
        m_impl = std::move(next);
        return true;
    }
    catch (const std::exception& _exception)
    {
        LogError("OpenPackage", "<root>", _exception.what());
        return false;
    }
}

void AssetReader::Close()
{
    m_impl.reset();
}

AssetReader::Mode AssetReader::GetMode() const
{
    return m_impl ? m_impl->mode : Mode::None;
}

bool AssetReader::IsOpen() const
{
    return m_impl != nullptr;
}

std::unique_ptr<AssetStream> AssetReader::Open(std::string_view _path) const
{
    if (!m_impl)
    {
        LogError("Open", _path, "asset source is not configured");
        return nullptr;
    }

    try
    {
        if (!IsValidAssetPath(_path))
        {
            LogError("Open", _path, "invalid logical asset path");
            return nullptr;
        }

        auto stream = std::make_unique<AssetStream::Impl>();
        stream->path = _path;
        if (m_impl->mode == Mode::Directory)
        {
            const auto path = ResolveAssetFile(m_impl->root, _path);
            if (path.empty())
            {
                LogError("Open", _path, "source file not found or not a regular file");
                return nullptr;
            }

            stream->file.open(path, std::ios::binary | std::ios::ate);
            const auto size = stream->file.tellg();
            if (!stream->file || size < 0)
            {
                LogError("Open", _path, "cannot open source file or determine its size");
                return nullptr;
            }
            stream->size = static_cast<std::uint64_t>(size);
        }
        else
        {
            const auto it = m_impl->lookup.find(_path);
            if (it == m_impl->lookup.end())
            {
                LogError("Open", _path, "asset not found in package");
                return nullptr;
            }

            const auto& entry = m_impl->index.entries[it->second];
            stream->size = entry.size;
            stream->packageOffset = entry.offset;
            stream->package = std::make_unique<SplitAssetReader>(
                m_impl->root / "assets", m_impl->index.partSize,
                m_impl->index.totalSize, m_impl->index.id);
            // OpenPackage already scanned part headers. Each stream validates
            // any part it actually opens; no repeated whole-package scan.
            if (!stream->package->PrepareFromValidatedIndex())
            {
                LogError("Open", _path, "invalid package metadata");
                return nullptr;
            }
        }
        return std::unique_ptr<AssetStream>(new AssetStream(std::move(stream)));
    }
    catch (const std::exception& _exception)
    {
        LogError("Open", _path, _exception.what());
        return nullptr;
    }
}

bool AssetReader::ReadAll(std::string_view _path, std::vector<std::byte>& _bytes) const
{
    auto stream = Open(_path);
    if (!stream)
        return false;

    try
    {
        std::vector<std::byte> bytes;
        if (stream->GetSize() > bytes.max_size())
        {
            LogError("ReadAll", _path, "asset is too large for a memory buffer");
            return false;
        }
        bytes.resize(static_cast<std::size_t>(stream->GetSize()));
        std::size_t read = 0;
        if (!stream->Read(bytes, read) || read != bytes.size())
            return false;

        _bytes = std::move(bytes);
        return true;
    }
    catch (const std::exception& _exception)
    {
        LogError("ReadAll", _path, _exception.what());
        return false;
    }
}
