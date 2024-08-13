#ifndef CLOCK_H
#define CLOCK_H

struct TimeReference {
    uint32_t timestamp;
};

class Clock {
public:
    Clock();
    unsigned long now() const;
    void synchronize(const TimeReference& time_reference);

private:
    long offset_;
    unsigned long initial_reference_time_;
    unsigned long initial_local_time_;
};

#endif // CLOCK_H
