#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Mail
 */
struct mail {
    std::string from{};
    std::unordered_set<std::string> recipients{};
    std::string message{};
};

/**
 * Volatile storage for mail
 */
struct mail_storage {
    std::unordered_map<std::string, std::vector<mail>> mails;
    std::mutex write_lock;
};