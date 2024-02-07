#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using user = std::string;
using recipient = user;

/**
 * Mail
 */
struct mail {
    user from{};
    std::unordered_set<recipient> recipients{};
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
    std::unordered_map<recipient, maildrop> maildrops;
    std::mutex write_lock;
};