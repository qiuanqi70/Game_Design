#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace alleyfist {

using EventNotification = std::function<void(std::uint32_t eventId)>;

class EventTrigger {
public:
    EventTrigger() noexcept = default;
    EventTrigger(const EventTrigger&) = delete;
    ~EventTrigger() noexcept = default;

    EventTrigger& operator=(const EventTrigger&) = delete;

    void clear_notifications() noexcept
    {
        m_notifications.clear();
    }

    std::uintptr_t add_notification(EventNotification&& notification)
    {
        std::uintptr_t index = 0;
        for (auto& item : m_notifications) {
            if (item == nullptr) {
                item = std::move(notification);
                return index + 1;
            }
            ++index;
        }

        m_notifications.push_back(std::move(notification));
        return index + 1;
    }

    void remove_notification(std::uintptr_t cookie) noexcept
    {
        assert(cookie > 0 && cookie <= m_notifications.size());
        m_notifications[cookie - 1] = nullptr;
    }

protected:
    void fire(std::uint32_t eventId)
    {
        for (auto& notification : m_notifications) {
            if (notification != nullptr) {
                notification(eventId);
            }
        }
    }

private:
    std::vector<EventNotification> m_notifications;
};

} // namespace alleyfist
