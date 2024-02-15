#include "mail.h"
#include "gui.h"

int main() {
    auto storage = mail_storage{};
    return start_gui(storage);;
}