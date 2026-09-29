#pragma once

#include <optional>
#include <string>

namespace etwc {

// Chuẩn hóa đường dẫn thiết bị thô của kernel
//   \Device\HarddiskVolume4\Windows\...  ->  C:\Windows\...
// Trả về nullopt nếu không map được volume.
std::optional<std::string> device_path_to_dos(const std::string& nt_path);

// Refresh bảng ánh xạ volume (gọi khi có thay đổi ổ đĩa).
void refresh_volume_map();

}  // namespace etwc
