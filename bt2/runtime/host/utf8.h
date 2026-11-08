// Text and paths as UTF-8 strings: what the settings file stores and the menus draw, so that a
// folder whose name is not in the system's code page still works.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <filesystem>
#include <string>

inline std::filesystem::path pathFromUtf8(const std::string& text) {return std::filesystem::path(std::u8string(text.begin(),text.end()));}
inline std::string utf8(const std::filesystem::path& path) {const auto text=path.u8string();return std::string(text.begin(),text.end());}
#ifdef _WIN32
// For the system's wide-character calls. Bytes that are not UTF-8 become replacement characters.
inline std::wstring wideFromUtf8(const std::string& text) {
    if(text.empty())return {};
    std::wstring wide(std::size_t(MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0)),L'\0');
    MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),wide.data(),int(wide.size()));
    return wide;
}
#endif
