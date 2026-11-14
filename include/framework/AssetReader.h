#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

// A read-only view of one logical asset, not the surrounding package.
// Owns its own file cursor. Separate streams can be used independently.
// Do not call the same stream concurrently or modify its source while open.
class AssetStream
{
public:
    ~AssetStream();

    AssetStream(const AssetStream&) = delete;
    AssetStream& operator=(const AssetStream&) = delete;

    std::uint64_t GetSize() const;
    std::uint64_t Tell() const;

    // Absolute offset within this asset. Seeking to EOF is allowed.
    // Failure leaves the position unchanged.
    bool Seek(std::uint64_t _offset);

    // Reads up to buffer.size() bytes, stopping at this asset's EOF.
    // Success with zero bytes means EOF (or an empty buffer).
    // Failure sets _bytesRead to zero and leaves Tell() unchanged;
    // buffer contents may have been partially overwritten.
    bool Read(std::span<std::byte> _buffer, std::size_t& _bytesRead);

private:
    friend class AssetReader;
    struct Impl;
    explicit AssetStream(std::unique_ptr<Impl> _impl);
    std::unique_ptr<Impl> m_impl;
};

// Game-owned, explicitly configured asset source. No global state or fallback.
// Roots are filesystem paths; asset names are case-sensitive UTF-8 paths
// relative to that root, e.g. "images/player.png" (not "assets/images/...").
class AssetReader
{
public:
    enum class Mode
    {
        None,
        Directory,
        Package
    };

    AssetReader();
    ~AssetReader();

    AssetReader(const AssetReader&) = delete;
    AssetReader& operator=(const AssetReader&) = delete;

    // Resolve root relative to the current working directory at configuration
    // time. Failed configuration preserves the previous source.
    bool OpenDirectory(const std::filesystem::path& _root);
    // _root contains assets.pak and assets.1, assets.2, ...
    // Validates index and every part header/size; does not scan payload CRCs.
    bool OpenPackage(const std::filesystem::path& _root);
    void Close();

    Mode GetMode() const;
    bool IsOpen() const;

    // On success, a stream owns everything it needs to read the selected
    // asset even after this reader is closed, reconfigured or destroyed.
    // Keep source files unchanged until the last stream is released.
    std::unique_ptr<AssetStream> Open(std::string_view _path) const;

    // Whole-asset convenience API for image/font/config data. Caller owns
    // the buffer. Failure leaves _bytes unchanged; an empty asset succeeds.
    bool ReadAll(std::string_view _path, std::vector<std::byte>& _bytes) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
