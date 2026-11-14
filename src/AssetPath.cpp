#include "AssetPath.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    bool IsPlainPath(const std::filesystem::path& _path, std::filesystem::file_type _type)
    {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(_path, error);
        if (error || status.type() != _type)
            return false;
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(_path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            return false;
#endif
        return true;
    }

    char FoldAscii(char _value)
    {
        return _value >= 'A' && _value <= 'Z' ? static_cast<char>(_value + ('a' - 'A')) : _value;
    }

    bool ValidUtf8(std::string_view _path)
    {
        std::size_t i = 0;
        while (i < _path.size())
        {
            const auto first = static_cast<unsigned char>(_path[i++]);
            if (first < 0x80)
                continue;
            unsigned int count = 0;
            std::uint32_t code = 0;
            std::uint32_t minimum = 0;
            if (first >= 0xc2 && first <= 0xdf)
            {
                count = 1; code = first & 0x1fu; minimum = 0x80;
            }
            else if (first >= 0xe0 && first <= 0xef)
            {
                count = 2; code = first & 0x0fu; minimum = 0x800;
            }
            else if (first >= 0xf0 && first <= 0xf4)
            {
                count = 3; code = first & 0x07u; minimum = 0x10000;
            }
            else
                return false;
            if (count > _path.size() - i)
                return false;
            for (unsigned int j = 0; j < count; ++j)
            {
                const auto next = static_cast<unsigned char>(_path[i++]);
                if ((next & 0xc0u) != 0x80u)
                    return false;
                code = (code << 6) | (next & 0x3fu);
            }
            if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
                return false;
        }
        return true;
    }

    bool ValidComponent(std::string_view _component)
    {
        if (_component.empty() || _component == "." || _component == ".." ||
            _component.back() == '.' || _component.back() == ' ')
            return false;
        for (const auto ch : _component)
        {
            if (static_cast<unsigned char>(ch) < 32 ||
                std::string_view("<>:\"\\|?*").find(ch) != std::string_view::npos)
                return false;
        }

        // Windows device names are reserved even with an extension.
        auto stem = _component.substr(0, _component.find('.'));
        while (!stem.empty() && stem.back() == ' ')
            stem.remove_suffix(1);
        std::string name(stem);
        std::transform(name.begin(), name.end(), name.begin(), FoldAscii);
        if (name == "con" || name == "prn" || name == "aux" || name == "nul" ||
            name == "conin$" || name == "conout$")
            return false;
        if (name.starts_with("com") || name.starts_with("lpt"))
        {
            const auto suffix = std::string_view(name).substr(3);
            if ((suffix.size() == 1 && suffix[0] >= '1' && suffix[0] <= '9') ||
                suffix == "\xc2\xb9" || suffix == "\xc2\xb2" || suffix == "\xc2\xb3")
                return false;
        }
        return true;
    }
}

namespace lwgf::detail
{
    bool IsPlainAssetFile(const std::filesystem::path& _path)
    {
        return IsPlainPath(_path, std::filesystem::file_type::regular);
    }

    bool IsPlainAssetDirectory(const std::filesystem::path& _path)
    {
        return IsPlainPath(_path, std::filesystem::file_type::directory);
    }

    bool IsValidAssetPath(std::string_view _path)
    {
        // Conservative V1 metadata bound; not an OS long-path guarantee.
        if (_path.empty() || _path.size() > 4096 || !ValidUtf8(_path))
            return false;
        while (true)
        {
            const auto slash = _path.find('/');
            if (!ValidComponent(_path.substr(0, slash)))
                return false;
            if (slash == std::string_view::npos)
                return true;
            _path.remove_prefix(slash + 1);
        }
    }

    bool AssetPathRegistry::CaseLess::operator()(const std::string& _left,
                                                  const std::string& _right) const
    {
#ifdef _WIN32
        // Input is valid UTF-8 and bounded by IsValidAssetPath().
        const auto left = std::filesystem::path(std::u8string(_left.begin(), _left.end())).wstring();
        const auto right = std::filesystem::path(std::u8string(_right.begin(), _right.end())).wstring();
        const int result = CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                                                 right.data(), static_cast<int>(right.size()), TRUE);
        if (result != 0)
            return result == CSTR_LESS_THAN;
        return _left < _right;
#else
        return std::lexicographical_compare(_left.begin(), _left.end(), _right.begin(), _right.end(),
            [](char _a, char _b) {
                return static_cast<unsigned char>(FoldAscii(_a)) <
                       static_cast<unsigned char>(FoldAscii(_b));
            });
#endif
    }

    bool AssetPathRegistry::Add(std::string_view _path, bool _directory)
    {
        if (!IsValidAssetPath(_path))
            return false;
        // Register all ancestors, so A/x and a/y cannot silently alias a folder.
        std::size_t end = 0;
        while (true)
        {
            end = _path.find('/', end);
            const bool last = end == std::string_view::npos;
            const std::string key(_path.substr(0, end));
            const bool directory = !last || _directory;
            const auto [it, inserted] = m_paths.try_emplace(key, Entry{key, directory});
            if (!inserted && (it->second.spelling != key ||
                it->second.directory != directory || !directory))
                return false;
            if (last)
                return true;
            ++end;
        }
    }

    bool AssetPathRegistry::AddFile(std::string_view _path)
    {
        return Add(_path, false);
    }

    bool AssetPathRegistry::AddDirectory(std::string_view _path)
    {
        return Add(_path, true);
    }
}
