#include "AssetPacker.h"
#include "AssetPackageFormat.h"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    template<typename Char>
    int Run(int _argc, Char** _argv)
    {
        namespace fs = std::filesystem;
        try
        {
            std::string error;
            if (_argc >= 2 && fs::path(_argv[1]) == "pack" && (_argc == 4 || _argc == 5))
            {
                auto partSize = lwgf::detail::DefaultAssetPartSize;
                if (_argc == 5)
                {
                    const auto value = fs::path(_argv[4]).string();
                    const auto result = std::from_chars(value.data(), value.data() + value.size(), partSize);
                    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                        !lwgf::detail::IsValidPartLimit(partSize))
                    {
                        std::cerr << "[AssetPacker] Invalid part size in bytes.\n";
                        return 2;
                    }
                }
                if (!lwgf::tools::PackAssets(fs::path(_argv[2]), fs::path(_argv[3]), partSize, error))
                {
                    std::cerr << "[AssetPacker] " << error << '\n';
                    return 1;
                }
                std::cout << "[AssetPacker] Package created and verified.\n";
                return 0;
            }
            if (_argc == 3 && fs::path(_argv[1]) == "verify")
            {
                if (!lwgf::tools::VerifyAssets(fs::path(_argv[2]), error))
                {
                    std::cerr << "[AssetPacker] " << error << '\n';
                    return 1;
                }
                std::cout << "[AssetPacker] Package verified.\n";
                return 0;
            }
            std::cerr << "Usage:\n"
                      << "  LWGFAssetPacker pack <source-dir> <output-dir> [part-size-bytes]\n"
                      << "  LWGFAssetPacker verify <package-dir>\n";
            return 2;
        }
        catch (const std::exception& _exception)
        {
            std::cerr << "[AssetPacker] " << _exception.what() << '\n';
            return 1;
        }
    }
}

#ifdef _WIN32
int wmain(int _argc, wchar_t** _argv)
#else
int main(int _argc, char** _argv)
#endif
{
    return Run(_argc, _argv);
}
