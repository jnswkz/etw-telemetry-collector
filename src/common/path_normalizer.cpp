#include "etwc/common/path_normalizer.hpp"

// TODO: hiện thực bằng QueryDosDeviceW để dựng bảng \Device\HarddiskVolumeN -> X:
//       và cache lại, refresh khi WM_DEVICECHANGE.

namespace etwc {

std::optional<std::string> device_path_to_dos(const std::string& nt_path) {
    // Stub: trả về nguyên trạng cho tới khi map volume được hiện thực.
    return nt_path;
}

void refresh_volume_map() {
    // TODO
}

}  // namespace etwc
