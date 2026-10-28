/* Small Windows launcher for installed copies; no command shell or Python needed. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

static int error(const wchar_t *message) {
    MessageBoxW(NULL, message, L"Tag Team Mod", MB_OK | MB_ICONERROR);
    return 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR args, int show) {
    wchar_t root[32768], path[32768], command[32772];
    STARTUPINFOW start = {0};
    PROCESS_INFORMATION process = {0};
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    wchar_t *environment, *entry, *slash;
    HANDLE log, input;
    DWORD count, status = 1;
    (void)instance; (void)previous; (void)args; (void)show;
    count = GetModuleFileNameW(NULL, root, 32768);
    if (!count || count >= 32768 || !(slash = wcsrchr(root, L'\\')))
        return error(L"Could not find the installation folder. / No se encuentra la carpeta de instalación.");
    *slash = 0;
    if (wcslen(root) > 32600 || !SetCurrentDirectoryW(root))
        return error(L"Could not open the installation folder. / No se puede abrir la carpeta de instalación.");

    /* Never inherit developer/test flags, or paths to another installation. */
    environment = GetEnvironmentStringsW();
    if (!environment) return error(L"Could not read the process environment.");
    for (entry = environment; *entry; entry += wcslen(entry) + 1) {
        if (_wcsnicmp(entry, L"BT3_", 4) == 0) {
            wchar_t *equal = wcschr(entry, L'=');
            if (equal) {
                *equal = 0;
                SetEnvironmentVariableW(entry, NULL);
                *equal = L'=';
            }
        }
    }
    FreeEnvironmentStringsW(environment);
    SetEnvironmentVariableW(L"BT3_TAG_TEAM", L"1");
    SetEnvironmentVariableW(L"BT3_GS", L"gpu");
    SetEnvironmentVariableW(L"BT3_UPDATE_CHECK", L"0");
    SetEnvironmentVariableW(L"BT3_MENU_ITEM4", L"0");
    swprintf(path, 32768, L"%ls\\gamedata", root);
    SetEnvironmentVariableW(L"BT3_DATA", path);
    swprintf(path, 32768, L"%ls\\saves", root);
    SetEnvironmentVariableW(L"BT3_SAVES", path);
    swprintf(path, 32768, L"%ls\\settings.txt", root);
    SetEnvironmentVariableW(L"BT3_SETTINGS", path);
    swprintf(path, 32768, L"%ls\\engine\\TagTeam.exe", root);
    swprintf(command, 32772, L"\"%ls\"", path);

    log = CreateFileW(L"game.log", GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, NULL);
    if (log == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        return error(L"The installation folder is not writable. Move the game to a folder you can write to.\n\nLa carpeta no permite escribir. Mueve el juego a una carpeta con permiso de escritura.");
    }
    start.cb = sizeof(start);
    start.dwFlags = STARTF_USESTDHANDLES;
    start.hStdInput = input;
    start.hStdOutput = start.hStdError = log;
    if (!CreateProcessW(path, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, root, &start, &process)) {
        CloseHandle(input); CloseHandle(log);
        return error(L"Could not start engine\\TagTeam.exe. Keep Play.exe in the complete installation folder.\n\nNo se pudo iniciar el juego. Mantén Play.exe en la carpeta de instalación completa.");
    }
    CloseHandle(input); CloseHandle(log); CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hProcess);
    if (status) return error(L"The game stopped. Check game.log and bt3_crash.txt (if present) in the installation folder.\n\nEl juego se ha detenido. Consulta game.log y bt3_crash.txt (si existe) en la carpeta de instalación.");
    return 0;
}
