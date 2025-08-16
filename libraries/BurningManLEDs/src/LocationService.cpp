#include "LocationService.h"
#include <time.h>

LocationService::LocationService() : initial_gps_sample_acquired_(false),
                                     latest_gps_sample_time_(0),
                                     current_speed_(0),
                                     speed_history_index_(0),
                                     gpsSerial(2),
                                     last_logged_speed_(0),
                                     last_gps_log_time_(0)
{
    for (size_t i = 0; i < SPEED_HISTORY_SIZE; i++)
    {
        speed_history_[i] = 0;
    }
}

void LocationService::start_tracking_position()
{
    Serial.printf("[LocationService] Starting GPS tracking on pins RX:%d TX:%d @ 9600 baud\n", GPS_RX_PIN, GPS_TX_PIN);
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    // Give GPS module more time to initialize
    Serial.println("[LocationService] Waiting for GPS module to initialize...");
    delay(2000); // Increased from 1000ms to 2000ms
    
    int available = gpsSerial.available();
    Serial.printf("[LocationService] GPS initialized. %d bytes available after 2 seconds\n", available);
    
    if (available > 0) {
        Serial.println("[LocationService] GPS is receiving data!");
        
        // Read and display first few bytes to verify data format
        String initialData = "";
        int charCount = 0;
        while (gpsSerial.available() > 0 && charCount < 100) {
            char c = gpsSerial.read();
            initialData += c;
            charCount++;
        }
        
        Serial.printf("[LocationService] Initial GPS data (%d chars): %s\n", charCount, initialData.c_str());
        
        // Check if data looks like valid NMEA
        if (initialData.indexOf("$GP") >= 0 || initialData.indexOf("$GN") >= 0 || initialData.indexOf("$GL") >= 0) {
            Serial.println("[LocationService] *** VALID NMEA DATA DETECTED! ***");
        } else {
            Serial.println("[LocationService] WARNING: Data doesn't look like valid NMEA sentences");
        }
    } else {
        Serial.println("[LocationService] WARNING: No GPS data detected on initialization");
        Serial.println("[LocationService] Check wiring: RX->TX, TX->RX, VCC->3.3V, GND->GND");
    }
    
    Serial.println("[LocationService] GPS tracking started. Waiting for satellite fix...");
    Serial.println("[LocationService] Note: First fix can take 30-60 seconds outdoors with clear sky view");
}

void LocationService::update_position()
{
    unsigned long now = millis();
    static unsigned long lastDebug = 0;
    static unsigned long totalChars = 0;
    static unsigned long lastSatelliteCheck = 0;

    if (now < (latest_gps_sample_time_ + GPS_SAMPLE_INTERVAL))
    {
        return;
    }
    
    int bytesRead = 0;
    while (gpsSerial.available() > 0)
    {
        char c = gpsSerial.read();
        bytesRead++;
        totalChars++;
        
        if (gps.encode(c))
        {
            if (gps.location.isValid())
            {
                latest_gps_sample_time_ = now;
                current_position_ = Position(gps.location.lat(), gps.location.lng());
                current_speed_ = gps.speed.kmph(); // Speed in km/h; convert if necessary

                if (!initial_gps_sample_acquired_)
                {
                    Serial.println("[LocationService] Initial GPS position acquired");
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
                
                // Only log GPS fixes when there are significant changes or every 30 seconds
                unsigned long now = millis();
                bool position_changed = (abs(current_position_.latitude() - last_logged_position_.latitude()) > 0.0001 ||
                                       abs(current_position_.longitude() - last_logged_position_.longitude()) > 0.0001);
                bool speed_changed = abs(current_speed_ - last_logged_speed_) > 1.0; // 1 km/h threshold
                bool time_to_log = (now - last_gps_log_time_) > 30000; // Every 30 seconds
                
                if (position_changed || speed_changed || time_to_log || last_gps_log_time_ == 0) {
                    Serial.printf("[LocationService] GPS fix: %.6f, %.6f, speed: %.2f km/h\n", 
                                 current_position_.latitude(), current_position_.longitude(), current_speed_);
                    
                    // Update last logged values
                    last_logged_position_ = current_position_;
                    last_logged_speed_ = current_speed_;
                    last_gps_log_time_ = now;
                }
            }
        }
    }
    
    // Enhanced debug output every 60 seconds
    if (now - lastDebug > 60000) {
        Serial.printf("[LocationService] Debug - Bytes read: %d, Total chars: %lu, Satellites: %d, Sentences: %lu, Failed: %lu\n",
                     bytesRead, totalChars, gps.satellites.value(), gps.sentencesWithFix(), gps.failedChecksum());
        
        // Additional GPS status information
        Serial.printf("[LocationService] GPS Status - Location valid: %s, Date valid: %s, Time valid: %s\n",
                     gps.location.isValid() ? "YES" : "NO",
                     gps.date.isValid() ? "YES" : "NO", 
                     gps.time.isValid() ? "YES" : "NO");
        
        if (gps.satellites.isValid()) {
            Serial.printf("[LocationService] Satellites in view: %d\n", gps.satellites.value());
        }
        
        if (gps.hdop.isValid()) {
            Serial.printf("[LocationService] HDOP (Horizontal Dilution of Precision): %.1f\n", gps.hdop.hdop());
        }
        
        if (gps.altitude.isValid()) {
            Serial.printf("[LocationService] Altitude: %.1f meters\n", gps.altitude.meters());
        }
        
        // Provide helpful guidance
        if (!gps.location.isValid()) {
            Serial.println("[LocationService] No GPS fix yet. This is normal if:");
            Serial.println("  - Device is indoors or has limited sky view");
            Serial.println("  - GPS module is still acquiring satellites (can take 30-60 seconds)");
            Serial.println("  - This is the first time using the GPS module");
            Serial.println("  - Move device outdoors with clear sky view for best results");
        }
        
        lastDebug = now;
    }
    
    // Check satellite count less frequently (every 30 seconds) and only log significant changes
    static int lastSatCount = -1;
    if (now - lastSatelliteCheck > 30000) {
        if (gps.satellites.isValid()) {
            int satCount = gps.satellites.value();
            // Only log if satellite count changed significantly or is 0
            if (satCount != lastSatCount && (abs(satCount - lastSatCount) > 2 || satCount == 0 || lastSatCount == -1)) {
                if (satCount > 0) {
                    Serial.printf("[LocationService] Satellites in view: %d\n", satCount);
                } else {
                    Serial.println("[LocationService] No satellites detected - check antenna and sky view");
                }
                lastSatCount = satCount;
            }
        }
        lastSatelliteCheck = now;
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