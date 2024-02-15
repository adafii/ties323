#pragma once
#include "asio/awaitable.hpp"
#include "mail.h"
#include "tls_socket.h"
#include <string_view>

/**
 * Connects to a pop3 server and fetches mail
 * @param username Username
 * @param password Password
 * @param host Host
 * @param port Port
 * @param[out] storage Storage for mails
 * @return Status
 */
std::string pop3_get_mail_tls(const std::string& username,
                       const std::string& password,
                       const std::string& host,
                       const std::string& port,
                       mail_storage& storage);

/**
 * Connects to a pop3 server and fetches mail
 * @param username Username
 * @param password Password
 * @param host Host
 * @param port Port
 * @param[out] storage Storage for mails
 * @return Status
 */
std::string pop3_get_mail(const std::string& username,
                   const std::string& password,
                   const std::string& host,
                   const std::string& port,
                   mail_storage& storage);