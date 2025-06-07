#include "MyWebSocketClient.h"
#include <chrono>
#include <thread>


int main() {
    MyWebSocketClient wsClient("localhost", "8004");
    wsClient.connect("/",
                     [](const std::string &msg) {
                         std::cout << "Received: " << msg << std::endl;
                     },
                     [](const std::string &err) {
                         std::cerr << "Error: " << err << std::endl;
                     }
    );

    wsClient.send("Hello WebSocket!");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    wsClient.close();
}