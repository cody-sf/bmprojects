#include "Position.h"

Position::Position(float latitude, float longitude) :
    latitude_(latitude),
    longitude_(longitude)
{}

float Position::distance_from(Position& origin) {
    float latitude1 = degrees_to_radians_(latitude_);
    float latitude2 = degrees_to_radians_(origin.latitude());
    float longitude1 = degrees_to_radians_(longitude_);
    float longitude2 = degrees_to_radians_(origin.longitude());
    float delta_latitude = latitude2 - latitude1;
    float delta_longitude = longitude2 - longitude1;
    float value = asin(sqrt(sin(delta_latitude / 2) * sin(delta_latitude / 2) +
        cos(latitude1) * cos(latitude2) * sin(delta_longitude / 2) * sin(delta_longitude / 2)));
    return 2 * EARTH_RADIUS_KM * value * 1000;
}

Position Position::operator-(Position& that) {
    return Position(latitude_ - that.latitude_, longitude_ - that.longitude_);
}

bool Position::operator==(Position& that) {
    return latitude_ == that.latitude_ && longitude_ == that.longitude_;
}

float Position::polar_radius()
{
    return sqrt(latitude_ * latitude_ + longitude_ * longitude_);
}

float Position::polar_angle()
{
    if (longitude_ == 0 && latitude_ == 0 ) {
        return 0;
    }
    else if (longitude_ == 0 && 0 < latitude_) {
        return M_PI / 2;
    }
    else if (longitude_ == 0 && latitude_ < 0) {
        return M_PI * 3 / 2;
    }
    else if (longitude_ < 0) {
        return atan(latitude_ / longitude_) + M_PI;
    }
    else if (latitude_ < 0) {
        return atan(latitude_ / longitude_) + 2 * M_PI;
    }
    else {
        return atan(latitude_ / longitude_);
    }
}
