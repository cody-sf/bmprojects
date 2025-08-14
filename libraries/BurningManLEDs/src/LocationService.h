#ifndef LOCATIONSERVICE_H
#define LOCATIONSERVICE_H

#include <TinyGPS++.h>
#include "Position.h"

#define SPEED_HISTORY_SIZE 10
#define GPS_SAMPLE_INTERVAL 1000
#define TIMEZONE_OFFSET (7 * 60 * 60) // GMT-7


#define GPS_RX_PIN 21 // Example pin for GPS RX
#define GPS_TX_PIN 22 // Example pin for GPS TX

class LocationService
{
public:
    LocationService();
    void start_tracking_position();
    void update_position();
    bool is_initial_position_available() { return initial_gps_sample_acquired_; }
    bool is_current_position_available(unsigned long max_age = 30000);
    Position initial_position() { return initial_position_; }
    Position current_position() { return current_position_; }
    float current_speed() { return current_speed_; }
    float recent_average_speed();
    bool is_time_available() { return initial_gps_sample_acquired_; }; // Implement using ESP32 time functions
    void update_time(int year, int month, int day, int hour, int minute, int second);

private:
    bool initial_gps_sample_acquired_;
    unsigned long latest_gps_sample_time_;
    Position initial_position_;
    Position current_position_;
    float current_speed_;
    float speed_history_[SPEED_HISTORY_SIZE];
    size_t speed_history_index_;
    TinyGPSPlus gps;          // TinyGPS++ object for parsing GPS data
    HardwareSerial gpsSerial; // ESP32 hardware serial object for GPS communication
};

#endif // LOCATIONSERVICE_H