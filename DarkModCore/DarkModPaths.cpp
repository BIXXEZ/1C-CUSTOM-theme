// ============================================================
// DARKMOD CORE - PATHS AND FILE IO
// ============================================================

#include "DarkModPaths.h"

#include <fstream>
#include <sstream>

namespace dm
{

    static std::wstring g_dataDirectory;


    // ============================================================
    // DIRECTORIES
    // ============================================================

    std::wstring ModuleDirectory(
        HMODULE module)
    {
        wchar_t path[MAX_PATH]{};

        const DWORD n =
            GetModuleFileNameW(
                module,
                path,
                MAX_PATH);

        if (!n)
            return L".";

        std::wstring result(
            path,
            n);

        const size_t pos =
            result.find_last_of(
                L"\\/");

        if (pos == std::wstring::npos)
            return L".";

        return
            result.substr(
                0,
                pos);
    }


    std::wstring LocalAppDataDirectory()
    {
        wchar_t local[MAX_PATH]{};

        const DWORD n =
            GetEnvironmentVariableW(
                L"LOCALAPPDATA",
                local,
                MAX_PATH);

        //
        // No usable variable: fall back to the exe directory
        // rather than refusing to start.
        //

        if (!n || n >= MAX_PATH)
            return ModuleDirectory();

        std::wstring dir(
            local,
            n);

        while (
            !dir.empty() &&
            (dir.back() == L'\\' ||
                dir.back() == L'/'))
        {
            dir.pop_back();
        }

        dir += L"\\DarkMod";

        if (
            !CreateDirectoryW(
                dir.c_str(),
                nullptr) &&
            GetLastError() !=
            ERROR_ALREADY_EXISTS)
        {
            return ModuleDirectory();
        }

        return dir;
    }


    void SetDataDirectory(
        const std::wstring& dir)
    {
        g_dataDirectory = dir;
    }


    const std::wstring& DataDirectory()
    {
        if (g_dataDirectory.empty())
        {
            g_dataDirectory =
                LocalAppDataDirectory();
        }

        return g_dataDirectory;
    }


    std::wstring FilePath(
        const wchar_t* name)
    {
        return
            DataDirectory() +
            L"\\" +
            name;
    }


    bool FileExists(
        const std::wstring& path)
    {
        const DWORD attributes =
            GetFileAttributesW(
                path.c_str());

        return
            attributes !=
            INVALID_FILE_ATTRIBUTES &&
            !(attributes &
                FILE_ATTRIBUTE_DIRECTORY);
    }


    // ============================================================
    // TEXT FILES
    // ============================================================

    std::string ReadTextFile(
        const std::wstring& path)
    {
        std::ifstream file(
            path,
            std::ios::binary);

        if (!file)
            return {};

        std::ostringstream ss;

        ss << file.rdbuf();

        return ss.str();
    }


    bool WriteTextFile(
        const std::wstring& path,
        const std::string& text)
    {
        std::ofstream file(
            path,
            std::ios::binary |
            std::ios::trunc);

        if (!file)
            return false;

        file.write(
            text.data(),
            static_cast<std::streamsize>(
                text.size()));

        return
            file.good();
    }


    bool WriteTextFileAtomic(
        const std::wstring& path,
        const std::string& text)
    {
        const std::wstring tmpPath =
            path + L".tmp";

        if (
            !WriteTextFile(
                tmpPath,
                text))
        {
            DeleteFileW(
                tmpPath.c_str());

            return false;
        }

        if (
            !MoveFileExW(
                tmpPath.c_str(),
                path.c_str(),
                MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(
                tmpPath.c_str());

            return false;
        }

        return true;
    }


    bool BackupFile(
        const std::wstring& path)    {
        if (!FileExists(path))
            return false;

        //
        // An empty current file is almost certainly the result of
        // a failed write, and overwriting the backup with it would
        // throw away the only good copy left.
        //

        WIN32_FILE_ATTRIBUTE_DATA info{};

        if (
            GetFileAttributesExW(
                path.c_str(),
                GetFileExInfoStandard,
                &info) &&
            info.nFileSizeHigh == 0 &&
            info.nFileSizeLow == 0)
        {
            return false;
        }

        const std::wstring backup =
            path + L".bak";

        return
            CopyFileW(
                path.c_str(),
                backup.c_str(),
                FALSE) != 0;
    }

}
