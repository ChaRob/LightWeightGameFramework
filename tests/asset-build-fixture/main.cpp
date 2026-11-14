#include <lwgf/AssetBuildConfig.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

#ifdef _WIN32
int wmain(int _argc, wchar_t** _argv)
#else
int main(int _argc, char** _argv)
#endif
{
    if (_argc != 3)
        return 1;
    const bool expectedPackage = fs::path(_argv[1]) == "PACKAGE";
    if (lwgf::build::UsePackage != expectedPackage)
        return 2;
    if (expectedPackage)
    {
        if (lwgf::build::DirectoryRoot[0] != u8'\0' ||
            !fs::is_regular_file(fs::path(_argv[2]) / lwgf::build::PackageDirectory / "assets.pak"))
            return 3;
    }
    else
    {
        const auto root = fs::path(lwgf::build::DirectoryRoot);
        if (root != fs::path(_argv[2]))
            return 4;
        std::ifstream file(root / "a.bin", std::ios::binary);
        std::string value;
        file >> value;
        if (value != "changed")
            return 5;
    }
    return 0;
}
