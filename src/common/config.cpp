#include "etwc/common/config.hpp"

// TODO: parse JSON bằng nlohmann::json khi tích hợp dependency.

namespace etwc {

Config Config::defaults() { return Config{}; }

Config Config::load(const std::filesystem::path& /*file*/) {
    // TODO: đọc file JSON, ghi đè các trường mặc định.
    return defaults();
}

}  // namespace etwc
