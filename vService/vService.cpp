#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <mutex>
#include <fstream>
#include <sddl.h> 
#include <sstream>
#include <iomanip>

constexpr auto SERVICE_NAME = L"vgc";

SERVICE_STATUS        g_ServiceStatus = {};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

std::atomic_bool shutdown_event(false);
std::atomic_bool client_disconnected(false);

void ServiceMain(DWORD argc, LPWSTR* argv);
void WINAPI ServiceCtrlHandler(DWORD);
void PipeServerThread();

static int WINAPI wmain(int argc, wchar_t* argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { const_cast<LPWSTR>(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        return 1;
    }

    return 0;
}

void ServiceMain(DWORD argc, LPWSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_StatusHandle) {
        return;
    }

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    client_disconnected = false;
    shutdown_event = false;

    std::thread pipeThread(PipeServerThread);

    HANDLE waitHandles[] = { g_ServiceStopEvent };
    while (true) {
        DWORD waitResult = WaitForSingleObject(g_ServiceStopEvent, 100);
        if (waitResult == WAIT_OBJECT_0) {
            shutdown_event = true;
            break;
        }
        if (client_disconnected.load()) {
            shutdown_event = true;
            break;
        }
    }

    if (pipeThread.joinable()) {
        pipeThread.join();
    }

    CloseHandle(g_ServiceStopEvent);
    g_ServiceStopEvent = NULL;

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

        shutdown_event = true;
        if (g_ServiceStopEvent) {
            SetEvent(g_ServiceStopEvent);
        }
        break;

    default:
        break;
    }
}

void PipeServerThread() {
    const std::wstring pipeName = L"\\\\.\\pipe\\933823D3-C77B-4BAE-89D7-A92B567236BC";

    while (!shutdown_event.load()) {
        SECURITY_ATTRIBUTES sa{};
        PSECURITY_DESCRIPTOR pSD = nullptr;

        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;OICI;GRGW;;;WD)", SDDL_REVISION_1, &pSD, nullptr))
        {
            Sleep(1000);
            continue;
        }

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = pSD;
        sa.bInheritHandle = FALSE;

        HANDLE pipe = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096, 0, &sa);

        LocalFree(pSD);

        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        OVERLAPPED ov{};
        ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) {
            CloseHandle(pipe);
            Sleep(1000);
            continue;
        }

        BOOL connected = ConnectNamedPipe(pipe, &ov);
        if (!connected) {
            DWORD err = GetLastError();

            if (err == ERROR_IO_PENDING) {
                HANDLE waitHandles[2] = { ov.hEvent, g_ServiceStopEvent };
                DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

                if (waitResult == WAIT_OBJECT_0) {
                    connected = TRUE;
                }
                else {
                    CancelIo(pipe);
                    CloseHandle(ov.hEvent);
                    CloseHandle(pipe);
                    return; // clean exit
                }
            }
            else if (err == ERROR_PIPE_CONNECTED) {
                connected = TRUE;
            }
            else {
            }
        }

        CloseHandle(ov.hEvent);

        if (!connected) {
            CloseHandle(pipe);
            continue;
        }

        char buffer[4096]{};
        DWORD bytesRead = 0;
        int messageCount = 0;

        while (!shutdown_event.load()) {
            BOOL success = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr);

            if (!success || bytesRead == 0) {
                client_disconnected = true;
                break;
            }

            messageCount++;

            if (messageCount == 2)
            {
                unsigned char connectMsg[36] = {
                    0xE9,0x03,0x00,0x00,0x24,0x00,0x00,0x00,
                    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00
                };

                DWORD bytesWritten = 0;
                WriteFile(pipe, connectMsg, sizeof(connectMsg), &bytesWritten, nullptr);
                FlushFileBuffers(pipe);
            }

            if (bytesRead == 40 && buffer[0] == 0x03) {
                char response[40];
                memcpy(response, buffer, 40);
                response[0] = 0x04;

                DWORD bytesWritten;
                WriteFile(pipe, response, 40, &bytesWritten, nullptr);
                FlushFileBuffers(pipe);
            }
        }

        if (shutdown_event.load()) {

            unsigned char disconnectMessage[36] = {
                0x02,0x00,0x00,0x00,0x24,0x00,0x00,0x00,
                0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
                0x39,0x05,0x00,0x00 // 1337 little-endian
            };

            DWORD bytesWritten = 0;
            BOOL success = WriteFile(pipe, disconnectMessage, sizeof(disconnectMessage), &bytesWritten, nullptr);
            FlushFileBuffers(pipe);

            if (!success)
            {
                client_disconnected = true;
            }
            break;
        }

        if (client_disconnected.load()) break;
    }
}
