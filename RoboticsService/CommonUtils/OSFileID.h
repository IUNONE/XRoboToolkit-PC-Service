// Platform-specific file and pipe handling definitions
#ifndef OSFILEID_H
#define OSFILEID_H

#ifdef _WIN32
#include <Windows.h>
#define OS_File_ID HANDLE
#elif defined(__linux__) || defined(__APPLE__)
#define OS_File_ID int
#endif
#include <string>
inline std::string GetVideoPipeName(const char* deviceSn)
{
#ifdef _WIN32
    return std::string(R"(\\.\pipe\)")+deviceSn+"_videopipe";
#elif defined(__linux__) || defined(__APPLE__)
    return std::string(deviceSn)+ "_videopipe";
#endif
}

#endif // OSFILEID_H
