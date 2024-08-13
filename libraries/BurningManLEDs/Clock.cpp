#include <Arduino.h>
#include "Clock.h"

Clock::Clock() : offset_(0),
                 initial_reference_time_(0),
                 initial_local_time_(0)
{
}

unsigned long Clock::now() const
{
    return millis() + offset_;
}

void Clock::synchronize(const TimeReference &time_reference)
{
    unsigned long now = millis();

    if (!initial_reference_time_ && !initial_local_time_)
    {
        initial_reference_time_ = time_reference.timestamp;
        initial_local_time_ = now;
        return;
    }

    unsigned long reference_diff = time_reference.timestamp - initial_reference_time_;
    unsigned long local_diff = now - initial_local_time_;
    offset_ = reference_diff - local_diff;
}
