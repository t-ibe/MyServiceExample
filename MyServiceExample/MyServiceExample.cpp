// Windows NT サービスのサンプルプログラム
// 参考) http://hp.vector.co.jp/authors/VA015815/200234.html#D20020821

#include "stdafx.h"

static TCHAR* SERVICE_NAME = _T("MyServiceExample");
static TCHAR* DISPLAY_NAME = _T("My Service Example");
static TCHAR* DESCRIPTION = _T("サービスのサンプルです。");

static SERVICE_STATUS g_srv_status = {
    SERVICE_WIN32_OWN_PROCESS,
    SERVICE_START_PENDING,
    SERVICE_ACCEPT_STOP,
    NO_ERROR,
    NO_ERROR,
    0,
    0
};

static SERVICE_STATUS_HANDLE g_srv_status_handle;

struct OpCloseServiceHandle
{
    void operator()(SC_HANDLE handle) const {
        if (handle)
            CloseServiceHandle(handle);
    }
};

typedef std::unique_ptr<std::remove_pointer<SC_HANDLE>::type, OpCloseServiceHandle> UniqueSCHandle;

std::string GetErrorMessage(DWORD error)
{
    std::string message;
    LPVOID lpMessageBuffer;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // デフォルト ユーザー言語 
        (LPSTR)&lpMessageBuffer,
        0,
        NULL);

    message = (LPSTR)lpMessageBuffer;
    LocalFree(lpMessageBuffer);
    return message;
}

void WINAPI Handler(DWORD ctrl)
{
    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
        g_srv_status.dwCurrentState = SERVICE_STOP_PENDING;
        g_srv_status.dwWin32ExitCode = 0;
        g_srv_status.dwCheckPoint = 0;
        g_srv_status.dwWaitHint = 1000;
        break;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }
    SetServiceStatus(g_srv_status_handle, &g_srv_status);
}

void WINAPI ServiceMain(DWORD ac, TCHAR **av)
{
    g_srv_status_handle = RegisterServiceCtrlHandler(SERVICE_NAME, Handler);
    if (!g_srv_status_handle)
        return;

    g_srv_status.dwCurrentState = SERVICE_START_PENDING;
    g_srv_status.dwCheckPoint = 0;
    g_srv_status.dwWaitHint = 1000;
    SetServiceStatus(g_srv_status_handle, &g_srv_status);

    /* 初期化をおこなう */

    g_srv_status.dwCurrentState = SERVICE_RUNNING;
    g_srv_status.dwCheckPoint = 0;
    g_srv_status.dwWaitHint = 0;
    SetServiceStatus(g_srv_status_handle, &g_srv_status);

    while (g_srv_status.dwCurrentState != SERVICE_STOPPED)
    {
        switch (g_srv_status.dwCurrentState)
        {
        case SERVICE_STOP_PENDING:

            /* 停止処理をおこなう */

            g_srv_status.dwCurrentState = SERVICE_STOPPED;
            g_srv_status.dwWin32ExitCode = 0;
            g_srv_status.dwCheckPoint = 0;
            g_srv_status.dwWaitHint = 0;
            SetServiceStatus(g_srv_status_handle, &g_srv_status);
            break;

        default:

            /* サービスの処理をおこなう */

            Sleep(1000);
            break;
        }
    }
}

void Install()
{
    TCHAR path[MAX_PATH];
    auto length = GetModuleFileName(0, path, MAX_PATH);
    if (length == 0)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto scm = UniqueSCHandle(OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS));
    if (!scm)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto srv = UniqueSCHandle(CreateService(scm.get(), SERVICE_NAME, DISPLAY_NAME, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        path, 0, 0, 0, 0, 0));
    if (!srv)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    SERVICE_DESCRIPTION desc;
    desc.lpDescription = DESCRIPTION;
    auto succeeded = ChangeServiceConfig2(srv.get(), SERVICE_CONFIG_DESCRIPTION, &desc);
    if (!succeeded)
        throw std::runtime_error(GetErrorMessage(GetLastError()));
}

void Remove()
{
    auto scm = UniqueSCHandle(OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS));
    if (!scm)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto srv = UniqueSCHandle(OpenService(scm.get(), SERVICE_NAME, DELETE));
    if (!srv)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto succeeded = DeleteService(srv.get());
    if (!succeeded)
        throw std::runtime_error(GetErrorMessage(GetLastError()));
}

void Start()
{
    auto scm = UniqueSCHandle(OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS));
    if (!scm)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto srv = UniqueSCHandle(OpenService(scm.get(), SERVICE_NAME, SERVICE_START | SERVICE_QUERY_STATUS));
    if (!srv)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto succeeded = StartService(srv.get(), 0, 0);
    if (!succeeded)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    SERVICE_STATUS st = {};
    succeeded = QueryServiceStatus(srv.get(), &st);
    if (!succeeded)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    while (st.dwCurrentState == SERVICE_START_PENDING) {
        Sleep(std::max(st.dwWaitHint, 1000ul));

        succeeded = QueryServiceStatus(srv.get(), &st);
        if (!succeeded)
            throw std::runtime_error(GetErrorMessage(GetLastError()));
    }

    if (st.dwCurrentState != SERVICE_RUNNING)
        throw std::runtime_error("service status is " + std::to_string(st.dwCurrentState));
}

void Stop()
{
    auto scm = UniqueSCHandle(OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!scm)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    auto srv = UniqueSCHandle(OpenService(scm.get(), SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS));
    if (!srv)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    SERVICE_STATUS st = {};
    auto succeeded = ControlService(srv.get(), SERVICE_CONTROL_STOP, &st);
    if (!succeeded)
        throw std::runtime_error(GetErrorMessage(GetLastError()));

    while (st.dwCurrentState == SERVICE_STOP_PENDING) {
        Sleep(std::max(st.dwWaitHint, 1000ul));

        succeeded = QueryServiceStatus(srv.get(), &st);
        if (!succeeded)
            throw std::runtime_error(GetErrorMessage(GetLastError()));
    }

    if (st.dwCurrentState != SERVICE_STOPPED)
        throw std::runtime_error("service status is " + std::to_string(st.dwCurrentState));
}

int _tmain(int argc, TCHAR* argv[])
{
    try {
        if (argc >= 2) {
            if (_tcscmp(argv[1], _T("install")) == 0) {
                Install();
                std::cout << "サービスをインストールしました。" << std::endl;
            }
            else if (_tcscmp(argv[1], _T("remove")) == 0) {
                Remove();
                std::cout << "サービスをアンインストールしました。" << std::endl;
            }
            else if (_tcscmp(argv[1], _T("start")) == 0) {
                Start();
                std::cout << "サービスを開始しました。" << std::endl;
            }
            else if (_tcscmp(argv[1], _T("stop")) == 0) {
                Stop();
                std::cout << "サービスを停止しました。" << std::endl;
            }
            else {
                throw std::runtime_error("不正なオプションです。");
            }

            return 0;
        }

        SERVICE_TABLE_ENTRY ent[] = { { SERVICE_NAME, ServiceMain },{ 0, 0 } };
        BOOL succeeded = StartServiceCtrlDispatcher(ent);
        if (!succeeded)
            throw std::runtime_error(GetErrorMessage(GetLastError()));

    }
    catch (std::exception& ex) {
        std::cout << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
