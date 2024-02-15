#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct mail {
    std::unordered_map<std::string, std::string> headers{};
    std::string body{};
};

using mail_storage = std::vector<mail>;