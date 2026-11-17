#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace Slic3r::PJarczakLinuxBridge {

class EventPump {
public:
    static EventPump& instance();
    void ensure_started();
    void stop();
    // Waits until the event with host sequence number `seq` has been dispatched, or `timeout`.
    void wait_dispatched(std::int64_t seq, std::chrono::milliseconds timeout);

private:
    EventPump() = default;
    EventPump(const EventPump&) = delete;
    EventPump& operator=(const EventPump&) = delete;
    ~EventPump();

    void run();

    bool m_running{false};
    std::mutex m_dispatched_mutex;
    std::condition_variable m_dispatched_cv;
    std::int64_t m_dispatched_seq{0};
};

}
