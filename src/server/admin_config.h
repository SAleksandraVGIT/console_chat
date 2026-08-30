#pragma once

#include "request_router.h"

#include <string>

namespace console_chat::server {

bool LoadAdminCredentials(
    const std::string& filePath,
    AdminCredentials& credentials,
    std::string& error);

} // namespace console_chat::server
