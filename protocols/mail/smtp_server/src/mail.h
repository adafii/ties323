#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using username = std::string;

/**
 * Mail
 */
struct mail {
    username from{};
    std::unordered_set<username> recipients{};
    std::string message{};
};

/**
 * All user's mails
 */
struct maildrop {
    std::vector<mail> mails{};
    std::mutex transaction{};
};

/**
 * Volatile storage for mail
 */
struct mail_storage {
    std::unordered_map<username, maildrop> maildrops;  // recipient, maildrop
    std::mutex write_lock;
};