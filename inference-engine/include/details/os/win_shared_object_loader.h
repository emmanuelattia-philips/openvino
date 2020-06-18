// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * @brief WINAPI compatible loader for a shared object
 * 
 * @file win_shared_object_loader.h
 */
#pragma once

#include "ie_api.h"
#include "details/ie_exception.hpp"
#include "details/os/os_filesystem.hpp"

// Avoidance of Windows.h to include winsock library.
#ifndef _WINSOCKAPI_
# define _WINSOCKAPI_
#endif

// Avoidance of Windows.h to define min/max.
#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <direct.h>
#include <windows.h>

namespace InferenceEngine {
namespace details {

/**
 * @brief This class provides an OS shared module abstraction
 */
class SharedObjectLoader {
private:
    HMODULE shared_object;

    void ExcludeCurrentDirectory() {
        // Exclude current directory from DLL search path process wise.
        // If application specific path was configured before then
        // current directory is alread excluded.
        // GetDLLDirectory does not distinguish if aplication specific
        // path was set to "" or NULL so reset it to "" to keep
        // aplication safe.
        if (GetDllDirectory(0, NULL) <= 1) {
            SetDllDirectory(TEXT(""));
        }
    }

    static const char  kPathSeparator = '\\';

    static const char* FindLastPathSeparatorA(LPCSTR path) {
        const char* const last_sep = strchr(path, kPathSeparator);
        return last_sep;
    }
    std::basic_string<CHAR> GetDirnameA(LPCSTR path) {
        auto pos = FindLastPathSeparatorA(path);
        if (pos == nullptr) {
            return path;
        }
        std::basic_string<CHAR> original(path);
        return original.substr(0, std::distance(path, pos));
    }
    std::basic_string<CHAR> IncludePluginDirectoryA(LPCSTR path) {
        std::vector<CHAR> lpBuffer;
        DWORD nBufferLength;

        nBufferLength = GetDllDirectoryW(0, nullptr);
        lpBuffer.resize(nBufferLength);
        GetDllDirectoryA(nBufferLength, &lpBuffer.front());

        auto dirname = GetDirnameA(path);
        if (!dirname.empty()) {
            SetDllDirectoryA(dirname.c_str());
        }

        return &lpBuffer.front();
    }
#ifdef ENABLE_UNICODE_PATH_SUPPORT
    static const wchar_t* FindLastPathSeparatorW(LPCWSTR path) {
        const wchar_t* const last_sep = wcsrchr(path, kPathSeparator);
        return last_sep;
    }

    std::basic_string<WCHAR> GetDirnameW(LPCWSTR path) {
        auto pos = FindLastPathSeparatorW(path);
        if (pos == nullptr) {
            return path;
        }
        std::basic_string<WCHAR> original(path);
        return original.substr(0, std::distance(path, pos));
    }

    std::basic_string<WCHAR> IncludePluginDirectoryW(LPCWSTR path) {
        std::vector<WCHAR> lpBuffer;
        DWORD nBufferLength;

        nBufferLength = GetDllDirectoryW(0, nullptr);
        lpBuffer.resize(nBufferLength);
        GetDllDirectoryW(nBufferLength, &lpBuffer.front());

        auto dirname = GetDirnameW(path);
        if (!dirname.empty()) {
            SetDllDirectoryW(dirname.c_str());
        }

        return &lpBuffer.front();
    }
#endif


public:
    /**
     * @brief A shared pointer to SharedObjectLoader
     */
    using Ptr = std::shared_ptr<SharedObjectLoader>;

#ifdef ENABLE_UNICODE_PATH_SUPPORT
    /**
     * @brief Loads a library with the name specified. The library is loaded according to the
     *        WinAPI LoadLibrary rules
     * @param pluginName Full or relative path to the plugin library
     */
    explicit SharedObjectLoader(LPCWSTR pluginName) {
        ExcludeCurrentDirectory();
        auto oldDir = IncludePluginDirectoryW(pluginName);

        shared_object = LoadLibraryW(pluginName);

        SetDllDirectoryW(oldDir.c_str());
        if (!shared_object) {
            char cwd[1024];
            THROW_IE_EXCEPTION << "Cannot load library '" << details::wStringtoMBCSstringChar(std::wstring(pluginName)) << "': " << GetLastError()
                               << " from cwd: " << _getcwd(cwd, sizeof(cwd));
        }
    }
#endif

    explicit SharedObjectLoader(LPCSTR pluginName) {
        ExcludeCurrentDirectory();
        auto oldDir = IncludePluginDirectoryA(pluginName);

        shared_object = LoadLibraryA(pluginName);

        SetDllDirectoryA(oldDir.c_str());
        if (!shared_object) {
            char cwd[1024];
            THROW_IE_EXCEPTION << "Cannot load library '" << pluginName << "': " << GetLastError()
                << " from cwd: " << _getcwd(cwd, sizeof(cwd));
        }
    }

    ~SharedObjectLoader() {
        FreeLibrary(shared_object);
    }

    /**
     * @brief Searches for a function symbol in the loaded module
     * @param symbolName Name of function to find
     * @return A pointer to the function if found
     * @throws InferenceEngineException if the function is not found
     */
    void* get_symbol(const char* symbolName) const {
        if (!shared_object) {
            THROW_IE_EXCEPTION << "Cannot get '" << symbolName << "' content from unknown library!";
        }
        auto procAddr = reinterpret_cast<void*>(GetProcAddress(shared_object, symbolName));
        if (procAddr == nullptr)
            THROW_IE_EXCEPTION << "GetProcAddress cannot locate method '" << symbolName << "': " << GetLastError();

        return procAddr;
    }
};

}  // namespace details
}  // namespace InferenceEngine
