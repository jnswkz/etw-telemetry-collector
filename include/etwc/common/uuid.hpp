#pragma once

#include <string>

namespace etwc {

// Generate a UUID v4 string (step 2: gán định danh duy nhất cho mỗi sự kiện).
std::string generate_uuid_v4();

}  // namespace etwc
