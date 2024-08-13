#ifndef POSITION_H
#define POSITION_H

#include <math.h>

#define EARTH_RADIUS_KM 6372.8

class Position {
public:
    Position(float latitude = 0, float longitude = 0);
    float latitude() { return latitude_; };
    float longitude() { return longitude_; };
    float distance_from(Position& origin);
    Position operator-(Position& that);
    bool operator==(Position& that);
    float polar_radius();
    float polar_angle();

private:
    inline double degrees_to_radians_(double angle) { return M_PI * angle / 180.0; };

    float latitude_;
    float longitude_;
};

#endif // POSITION_H
