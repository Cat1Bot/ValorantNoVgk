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

#define SERVICE_NAME L"vgc"

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
                path = path.substr(0, lastSlash + 1) + L"vgc_service.log";
                log_file.open(path, std::ios::out | std::ios::app);
            }
        }
    }

    if (log_file.is_open()) {
        log_file << msg << std::endl;
        log_file.flush();
    }
}

// Forward declarations
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

    // Service stopped
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
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

        // Signal shutdown to pipe server and service main loop
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
        PSECURITY_DESCRIPTOR pSD = NULL;

        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;OICI;GRGW;;;WD)", SDDL_REVISION_1, &pSD, NULL)) {
            Log(L"Failed to create security descriptor for pipe.");
            Sleep(1000);
            continue;
        }

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = pSD;
        sa.bInheritHandle = FALSE;

        HANDLE pipe = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096,
            4096,
            0,
            &sa);

        LocalFree(pSD);

        if (pipe == INVALID_HANDLE_VALUE) {
            Log(L"Failed to create named pipe.");
            Sleep(1000);
            continue;
        }

        Log(L"Waiting for client connection...");

        BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected) {
            CloseHandle(pipe);
            Log(L"Failed to connect client.");
            continue;
        }

        Log(L"Client connected.");

        char buffer[4096];
        DWORD bytesRead;
        int messageCount = 0;

        // Read loop
        while (!shutdown_event.load()) {
            BOOL success = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, NULL);

            if (!success || bytesRead == 0) {
                Log(L"Client disconnected.");
                client_disconnected = true;
                break;
            }

            messageCount++;

            std::wstringstream hexStream;
            hexStream << L"Received message #" << messageCount << L": ";
            for (DWORD i = 0; i < bytesRead; ++i) {
                hexStream << std::hex << std::setw(2) << std::setfill(L'0')
                    << static_cast<int>(static_cast<unsigned char>(buffer[i])) << L' ';
            }
            Log(hexStream.str());

            // If it's a 40-byte message with opcode 0x03 (HeartbeatSync), respond with 0x04 (HeartbeatAck) not sure if this is correct, reverseing the game there are 2 functions heartbeat ack and sync not sure if im resonding properly
            if (bytesRead == 40 && buffer[0] == 0x03) {
                char response[40];
                memcpy(response, buffer, 40);
                response[0] = 0x04;

                Log(L"Responding with fake HeartbeatAck");

                DWORD bytesWritten;
                WriteFile(pipe, response, 40, &bytesWritten, NULL);
                FlushFileBuffers(pipe);
            }

        }

        if (shutdown_event.load()) {
            Log(L"Disconnecting client due to service stop.");
        }

        CloseHandle(pipe);

        if (shutdown_event.load() || client_disconnected.load()) {
            break;
        }
    }
}
