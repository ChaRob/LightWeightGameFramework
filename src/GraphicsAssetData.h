#pragma once

#include <framework/AssetReader.h>

#include <exception>
#include <iostream>
#include <limits>

namespace lwgf::detail
{
    // stb decoders use int-sized inputs/offsets. Check before allocating.
    inline bool ReadGraphicsAsset(const AssetReader& _assets, std::string_view _path,
                                  std::vector<std::byte>& _data)
    {
        auto stream = _assets.Open(_path);
        if (!stream)
            return false;
        if (stream->GetSize() == 0 ||
            stream->GetSize() > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
            stream->GetSize() > _data.max_size())
        {
            std::cerr << "[Graphics] Invalid asset size: " << _path << '\n';
            return false;
        }
        try
        {
            _data.resize(static_cast<std::size_t>(stream->GetSize()));
            std::size_t read = 0;
            return stream->Read(_data, read) && read == _data.size();
        }
        catch (const std::exception& _exception)
        {
            std::cerr << "[Graphics] Cannot read asset '" << _path << "': "
                      << _exception.what() << '\n';
            return false;
        }
    }
}
