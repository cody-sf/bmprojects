#include "LocationService.h"
#include <time.h>

LocationService::LocationService() : initial_gps_sample_acquired_(false),
                                     latest_gps_sample_time_(0),
                                     current_speed_(0),
                                     speed_history_index_(0),
                                     gpsSerial(2)
{
    for (size_t i = 0; i < SPEED_HISTORY_SIZE; i++)
    {
        speed_history_[i] = 0;
    }
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); // Adjust baud rate as per your GPS module's specs
}

void LocationService::start_tracking_position()
{
    Serial.println("GPS initialized");
}

void LocationService::update_position()
{
    unsigned long now = millis();

    if (now < (latest_gps_sample_time_ + GPS_SAMPLE_INTERVAL))
    {
        return;
    }
    while (gpsSerial.available() > 0)
    {
        if (gps.encode(gpsSerial.read()))
        {
            if (gps.location.isValid())
            {
                latest_gps_sample_time_ = now;
                current_position_ = Position(gps.location.lat(), gps.location.lng());
                current_speed_ = gps.speed.kmph(); // Speed in km/h; convert if necessary

                if (!initial_gps_sample_acquired_)
                {
                    Serial.println("Initial GPS position acquired");
                    initial_position_ = current_position_;
                    initial_gps_sample_acquired_ = true;
                }

                speed_history_[speed_history_index_] = current_speed_;
                speed_history_index_ = (speed_history_index_ + 1) % SPEED_HISTORY_SIZE;

                // Update time if available
                if (gps.time.isValid())
                {
                    update_time(gps.date.year(), gps.date.month(), gps.date.day(),
                                gps.time.hour(), gps.time.minute(), gps.time.second());
                }
            }
        }
    }
}

bool LocationService::is_current_position_available(unsigned long max_age)
{
    return initial_gps_sample_acquired_ && (millis() < (latest_gps_sample_time_ + max_age));
}

float LocationService::recent_average_speed()
{
    float sum = 0;

    for (size_t i = 0; i < SPEED_HISTORY_SIZE; i++)
    {
        sum += speed_history_[i];
    }

    return sum / SPEED_HISTORY_SIZE;
}

void LocationService::update_time(int year, int month, int day, int hour, int minute, int second)
{
    // Set timezone and daylight savings
    setenv("TZ", "PST8PDT", 1); // Modify according to your timezone
    tzset();

    // Calculate time_t value for the given date and time
    struct tm t;
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

    time_t t_of_day = mktime(&t);

    // Set system time
    struct timeval now = {.tv_sec = t_of_day};
    settimeofday(&now, NULL);
}