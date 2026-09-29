#include "etwc/sensor/peb_reader.hpp"

// TODO: OpenProcess -> NtQueryInformationProcess(ProcessBasicInformation)
//       -> đọc PEB -> ProcessParameters -> CommandLine / ImagePathName
//       qua ReadProcessMemory. Xử lý WOW64 khi cần.

namespace etwc {

std::optional<std::string> read_command_line_from_peb(Pid /*pid*/) {
    return std::nullopt;  // stub
}

std::optional<std::string> read_image_path_from_peb(Pid /*pid*/) {
    return std::nullopt;  // stub
}

}  // namespace etwc
