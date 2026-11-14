#include "AudioAssetVfs.h"

#include <framework/AssetReader.h>

#include <filesystem>
#include <limits>
#include <new>
#include <span>
#include <type_traits>

namespace
{
    using lwgf::detail::AudioAssetVfs;

    static_assert(std::is_standard_layout_v<AudioAssetVfs>);
    static_assert(offsetof(AudioAssetVfs, callbacks) == 0);

    AssetStream* Stream(ma_vfs_file _file)
    {
        return static_cast<AssetStream*>(_file);
    }

    ma_result Open(ma_vfs* _vfs, const char* _path, ma_uint32 _mode, ma_vfs_file* _file)
    {
        if (_file == nullptr)
            return MA_INVALID_ARGS;
        *_file = nullptr;
        if (_vfs == nullptr || _path == nullptr || _path[0] == '\0')
            return MA_INVALID_ARGS;
        if (_mode != MA_OPEN_MODE_READ)
            return MA_ACCESS_DENIED;

        try
        {
            const auto* vfs = static_cast<const AudioAssetVfs*>(_vfs);
            if (!vfs->assets || !vfs->assets->IsOpen())
                return MA_INVALID_OPERATION;
            auto stream = vfs->assets->Open(_path);
            if (!stream)
                return MA_DOES_NOT_EXIST;
            // miniaudio's seek and tell API uses signed 64-bit offsets.
            if (stream->GetSize() > static_cast<std::uint64_t>(
                    std::numeric_limits<ma_int64>::max()))
                return MA_INVALID_ARGS;

            *_file = stream.release();
            return MA_SUCCESS;
        }
        catch (const std::bad_alloc&)
        {
            return MA_OUT_OF_MEMORY;
        }
        catch (...)
        {
            return MA_IO_ERROR;
        }
    }

    ma_result OpenWide(ma_vfs* _vfs, const wchar_t* _path, ma_uint32 _mode, ma_vfs_file* _file)
    {
        if (_file == nullptr)
            return MA_INVALID_ARGS;
        *_file = nullptr;
        if (_path == nullptr)
            return MA_INVALID_ARGS;
        try
        {
            const auto path = std::filesystem::path(_path).u8string();
            const std::string utf8(path.begin(), path.end());
            return Open(_vfs, utf8.c_str(), _mode, _file);
        }
        catch (const std::bad_alloc&)
        {
            return MA_OUT_OF_MEMORY;
        }
        catch (...)
        {
            return MA_INVALID_ARGS;
        }
    }

    ma_result Close(ma_vfs*, ma_vfs_file _file)
    {
        if (_file == nullptr)
            return MA_INVALID_ARGS;
        delete Stream(_file);
        return MA_SUCCESS;
    }

    ma_result Read(ma_vfs*, ma_vfs_file _file, void* _destination,
                   size_t _size, size_t* _bytesRead)
    {
        if (_bytesRead)
            *_bytesRead = 0;
        if (_file == nullptr || (_destination == nullptr && _size != 0))
            return MA_INVALID_ARGS;
        if (_size == 0)
            return MA_SUCCESS;

        try
        {
            std::size_t read = 0;
            if (!Stream(_file)->Read(
                    std::span(static_cast<std::byte*>(_destination), _size), read))
                return MA_IO_ERROR;
            if (_bytesRead)
                *_bytesRead = read;
            return read == 0 ? MA_AT_END : MA_SUCCESS;
        }
        catch (...)
        {
            return MA_IO_ERROR;
        }
    }

    ma_result Write(ma_vfs*, ma_vfs_file, const void*, size_t, size_t* _bytesWritten)
    {
        if (_bytesWritten)
            *_bytesWritten = 0;
        return MA_ACCESS_DENIED;
    }

    ma_result Seek(ma_vfs*, ma_vfs_file _file, ma_int64 _offset, ma_seek_origin _origin)
    {
        if (_file == nullptr)
            return MA_INVALID_ARGS;

        try
        {
            auto* stream = Stream(_file);
            std::uint64_t base = 0;
            switch (_origin)
            {
            case ma_seek_origin_start: break;
            case ma_seek_origin_current: base = stream->Tell(); break;
            case ma_seek_origin_end: base = stream->GetSize(); break;
            default: return MA_INVALID_ARGS;
            }

            // Avoid overflow, including negating INT64_MIN.
            std::uint64_t target = 0;
            if (_offset < 0)
            {
                const auto magnitude = static_cast<std::uint64_t>(-(_offset + 1)) + 1;
                if (magnitude > base)
                    return MA_INVALID_ARGS;
                target = base - magnitude;
            }
            else
            {
                const auto delta = static_cast<std::uint64_t>(_offset);
                if (delta > stream->GetSize() - base)
                    return MA_INVALID_ARGS;
                target = base + delta;
            }

            return stream->Seek(target) ? MA_SUCCESS : MA_INVALID_ARGS;
        }
        catch (...)
        {
            return MA_IO_ERROR;
        }
    }

    ma_result Tell(ma_vfs*, ma_vfs_file _file, ma_int64* _cursor)
    {
        if (_file == nullptr || _cursor == nullptr)
            return MA_INVALID_ARGS;
        *_cursor = static_cast<ma_int64>(Stream(_file)->Tell());
        return MA_SUCCESS;
    }

    ma_result Info(ma_vfs*, ma_vfs_file _file, ma_file_info* _info)
    {
        if (_file == nullptr || _info == nullptr)
            return MA_INVALID_ARGS;
        *_info = {};
        _info->sizeInBytes = Stream(_file)->GetSize();
        return MA_SUCCESS;
    }
}

namespace lwgf::detail
{
    AudioAssetVfs::AudioAssetVfs(const AssetReader* _assets)
        : assets(_assets)
    {
        callbacks.onOpen = Open;
        callbacks.onOpenW = OpenWide;
        callbacks.onClose = Close;
        callbacks.onRead = Read;
        callbacks.onWrite = Write;
        callbacks.onSeek = Seek;
        callbacks.onTell = Tell;
        callbacks.onInfo = Info;
    }
}
