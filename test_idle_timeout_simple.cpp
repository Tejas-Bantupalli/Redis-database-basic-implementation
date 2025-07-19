#include <iostream>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../common.h"
#include "../timer.h"
#include "../utils.h"
#include "../DList.h"

int main() {
    std::cout << "Testing idle timeout functionality..." << std::endl;
    
    // Initialize global data
    dList_init(&g_data.idle_list);
    
    // Create a test connection
    Conn *conn = (Conn *)malloc(sizeof(Conn));
    memset(conn, 0, sizeof(Conn));
    conn->fd = 999; // dummy fd
    conn->state = STATE_REQ;
    conn->idle_start = get_monotonic_usec();
    
    // Initialize the idle_list
    dList_init(&conn->idle_list);
    list_insert_before(&g_data.idle_list, &conn->idle_list);
    
    std::cout << "Connection created with idle_start: " << conn->idle_start << std::endl;
    std::cout << "Timeout is set to: " << k_idle_timeout_ms << " ms" << std::endl;
    
    // Test next_timer_ms
    uint32_t timeout = next_timer_ms();
    std::cout << "next_timer_ms returned: " << timeout << " ms" << std::endl;
    
    // Wait for the timeout
    std::cout << "Waiting for timeout..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(k_idle_timeout_ms + 1000));
    
    // Check if connection should be timed out
    uint64_t now_us = get_monotonic_usec();
    uint64_t next_us = conn->idle_start + k_idle_timeout_ms * 1000;
    
    std::cout << "Current time: " << now_us << std::endl;
    std::cout << "Timeout time: " << next_us << std::endl;
    std::cout << "Should timeout: " << (now_us >= next_us ? "YES" : "NO") << std::endl;
    
    // Call process_timers
    std::cout << "Calling process_timers..." << std::endl;
    process_timers();
    
    // Check if connection was removed
    bool still_in_list = !dList_empty(&g_data.idle_list);
    std::cout << "Connection still in list: " << (still_in_list ? "YES" : "NO") << std::endl;
    
    if (still_in_list) {
        std::cout << "ERROR: Connection was not removed by timeout!" << std::endl;
        return 1;
    } else {
        std::cout << "SUCCESS: Connection was properly removed by timeout!" << std::endl;
    }
    
    free(conn);
    return 0;
} 