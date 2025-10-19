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
std::mutex log_mutex;
std::wofstream log_file;

std::atomic_bool client_disconnected(false);

static void Log(const std::wstring& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);

    if (!log_file.is_open()) {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
            std::wstring path(exePath);
            size_t lastSlash = path.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) {
                path = path.substr(0, lastSlash + 1) + L"vService.log";

                DeleteFileW(path.c_str());

                log_file.open(path, std::ios::out | std::ios::app);
            }
        }
    }

    if (log_file.is_open()) {
        log_file << msg << std::endl;
        log_file.flush();
    }
}


void ServiceMain(DWORD argc, LPWSTR* argv);
void WINAPI ServiceCtrlHandler(DWORD);
void PipeServerThread();

static int WINAPI wmain(int argc, wchar_t* argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { const_cast<LPWSTR>(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        Log(L"StartServiceCtrlDispatcher failed");
        return 1;
    }

    return 0;
}

void ServiceMain(DWORD argc, LPWSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_StatusHandle) {
        Log(L"RegisterServiceCtrlHandler failed");
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
        Log(L"CreateEvent failed");
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
            Log(L"Service stop event signaled.");
            shutdown_event = true;
            break;
        }
        if (client_disconnected.load()) {
            Log(L"Client disconnected, stopping service.");
            shutdown_event = true;
            break;
        }
    }

    if (pipeThread.joinable()) {
        pipeThread.join();
    }

    CloseHandle(g_ServiceStopEvent);
    g_ServiceStopEvent = NULL;

    if (log_file.is_open()) {
        log_file.close();
    }

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        Log(L"Service stop requested.");

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
            Log(L"Failed to create security descriptor for pipe.");
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
            Log(L"CreateNamedPipeW failed.");
            Sleep(1000);
            continue;
        }

        Log(L"Waiting for client connection...");

        OVERLAPPED ov{};
        ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) {
            Log(L"CreateEvent for overlapped failed.");
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
                    Log(L"Service stop requested before client connection.");
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
                std::wstringstream ss;
                ss << L"ConnectNamedPipe failed, error " << err;
                Log(ss.str());
            }
        }

        CloseHandle(ov.hEvent);

        if (!connected) {
            Log(L"No client connected. Recreating pipe...");
            CloseHandle(pipe);
            continue;
        }

        Log(L"Client connected.");

        char buffer[4096]{};
        DWORD bytesRead = 0;
        int messageCount = 0;

        while (!shutdown_event.load()) {
            BOOL success = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr);

            if (!success || bytesRead == 0) {
                DWORD err = GetLastError();
                if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA) {
                    Log(L"Client disconnected normally.");
                }
                else {
                    std::wstringstream ss;
                    ss << L"ReadFile failed, error " << err;
                    Log(ss.str());
                }
                client_disconnected = true;
                break;
            }

            messageCount++;

            std::wstringstream hexStream;
            hexStream << L"Received message #" << messageCount << L": ";
            for (DWORD i = 0; i < bytesRead; ++i)
                hexStream << std::hex << std::setw(2) << std::setfill(L'0')
                << static_cast<int>(static_cast<unsigned char>(buffer[i])) << L' ';
            Log(hexStream.str());

            // Send one-time message after second message received
            if (messageCount == 2)
            {
                unsigned char oneTimeMsg[36] = {
                    0xE9,0x03,0x00,0x00,0x24,0x00,0x00,0x00,
                    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00
                };

                DWORD bytesWritten = 0;
                BOOL success = WriteFile(pipe, oneTimeMsg, sizeof(oneTimeMsg), &bytesWritten, nullptr);
                FlushFileBuffers(pipe);

                if (success)
                    Log(L"Sent one-time message after second message received.");
                else {
                    DWORD err = GetLastError();
                    std::wstringstream ss;
                    ss << L"Failed to send one-time message, error " << err;
                    Log(ss.str());
                }
            }


            // Handles HeartbeatSync -> HeartbeatAck
            if (bytesRead == 40 && buffer[0] == 0x03) {
                char response[40];
                memcpy(response, buffer, 40);
                response[0] = 0x04;

                Log(L"Responding with HeartbeatAck.");

                DWORD bytesWritten;
                WriteFile(pipe, response, 40, &bytesWritten, nullptr);
                FlushFileBuffers(pipe);
            }
        }

        if (shutdown_event.load()) {
            Log(L"Sending disconnect message to client...");

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

            if (success)
                Log(L"Sent disconnect message to client successfully.");
            else {
                DWORD err = GetLastError();
                std::wstringstream ss;
                ss << L"Failed to send disconnect message, error " << err << L". Assuming no client connected.";
                Log(ss.str());
                client_disconnected = true;
            }
            break;
        }

        if (client_disconnected.load()) break;
    }
}