#pragma once

#include <map>
#include <string>
#include <string_view>

namespace lwgf::detail
{
    // Canonical logical path: UTF-8, relative, forward slashes, no OS aliases.
    bool IsValidAssetPath(std::string_view _path);

    // Detect file/directory aliases and case conflicts. Exact spelling remains
    // the lookup key. Windows uses ordinal case-insensitive comparison;
    // non-Windows hosts currently fold ASCII only.
    class AssetPathRegistry
    {
    public:
        bool AddFile(std::string_view _path);
        bool AddDirectory(std::string_view _path);

    private:
        struct CaseLess
        {
            bool operator()(const std::string& _left, const std::string& _right) const;
        };
        struct Entry
        {
            std::string spelling;
            bool directory;
        };
        bool Add(std::string_view _path, bool _directory);
        std::map<std::string, Entry, CaseLess> m_paths;
    };
}
