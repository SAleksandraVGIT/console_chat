#include "console_chat/chat_service.h"
#include "console_chat/console.h"

#include <iostream>


int main() {
    try {
        console_chat::ChatService service;
        console_chat::Console console(service);

        console.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error." << std::endl;
        return 1;
    }

    return 0;
}
