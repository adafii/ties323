#pragma once
#include "mail.h"
#include <functional>
#include <string>

/**
 * Start gui
 * @param fetch_mail_tls
 * @param fetch_mail
 * @param storage
 * @return exit code
 */
int start_gui(mail_storage& storage);
