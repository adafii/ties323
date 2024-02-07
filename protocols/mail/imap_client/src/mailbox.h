#pragma once
#include <string>
#include <vector>

struct mail {
    std::string from;
    std::string to;
    std::string date;
    std::string subject;
};

using mailbox_name = std::string;
using mailbox = std::vector<mail>;
using mailbox_storage = std::vector<std::pair<mailbox_name, mailbox>>;