#include <windows.h>

#include <string>

#include "etwc/common/logging.hpp"
#include "etwc/service/windows_service.hpp"

// Cài đặt / gỡ service với SCM. Cần chạy bằng quyền Administrator.
namespace etwc::service {

int install() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
        return 1;

    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
        return 2;

    SC_HANDLE svc =
        CreateServiceW(scm, kServiceName, kDisplayName, SERVICE_ALL_ACCESS,
                       SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path,
                       nullptr, nullptr, nullptr, nullptr, nullptr);

    int rc = svc ? 0 : 3;
    if (svc)
        CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return rc;
}

int uninstall() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return 2;
    SC_HANDLE svc = OpenServiceW(scm, kServiceName, DELETE | SERVICE_STOP);
    int rc = 0;
    if (svc) {
        SERVICE_STATUS st{};
        ControlService(svc, SERVICE_CONTROL_STOP, &st);
        if (!DeleteService(svc))
            rc = 4;
        CloseServiceHandle(svc);
    } else {
        rc = 3;
    }
    CloseServiceHandle(scm);
    return rc;
}

}  // namespace etwc::service
