#pragma once

#include <cstdint>

namespace alleyfist {

// Event notification is intentionally a plain callback pair:
// a function pointer plus a context pointer to the receiver instance.
using EventNotification = void (*)(std::uint32_t eventId, void* context);

class EventTrigger {
public:
    EventTrigger() noexcept = default;
    EventTrigger(const EventTrigger&) = delete;
    ~EventTrigger() noexcept = default;

    EventTrigger& operator=(const EventTrigger&) = delete;

    void set_notification(EventNotification notification, void* context) noexcept
    {
        m_notification = notification;
        m_context = context;
    }

    void clear_notification() noexcept
    {
        m_notification = nullptr;
        m_context = nullptr;
    }

protected:
    void fire(std::uint32_t eventId) const
    {
        if (m_notification != nullptr) {
            m_notification(eventId, m_context);
        }
    }

private:
    EventNotification m_notification = nullptr;
    void* m_context = nullptr;
};

} // namespace alleyfist
