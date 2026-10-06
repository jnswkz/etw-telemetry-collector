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

    // Chạy dưới LocalSystem (account = nullptr) để mở ETW kernel session.
    SC_HANDLE svc =
        CreateServiceW(scm, kServiceName, kDisplayName, SERVICE_ALL_ACCESS,
                       SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path,
                       nullptr, nullptr, nullptr, nullptr, nullptr);

    int rc = svc ? 0 : 3;
    if (svc) {
        // Mô tả hiển thị trong services.msc.
        SERVICE_DESCRIPTIONW desc{};
        desc.lpDescription =
            const_cast<LPWSTR>(L"ETW-based provenance telemetry collector (UNICORN-inspired).");
        ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

        // Recovery: tự khởi động lại khi crash (sau 5s), reset bộ đếm mỗi ngày.
        SC_ACTION actions[3] = {
            {SC_ACTION_RESTART, 5000},
            {SC_ACTION_RESTART, 5000},
            {SC_ACTION_NONE, 0},
        };
        SERVICE_FAILURE_ACTIONSW fa{};
        fa.dwResetPeriod = 86400;
        fa.cActions = 3;
        fa.lpsaActions = actions;
        ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);

        CloseServiceHandle(svc);
    }
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
