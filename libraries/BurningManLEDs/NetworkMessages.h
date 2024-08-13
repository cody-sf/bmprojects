#ifndef NETWORKMESSAGES_H
#define NETWORKMESSAGES_H

#include <LightShow.h>
#include <Clock.h>

struct NetworkMessage {
    enum class MessageType : uint8_t {
        light_scene,
        time_reference
    };

    MessageType type;
    uint8_t selected_devices;
    union {
        LightScene light_scene;
        TimeReference time_reference;
    } messages;
};

#endif // NETWORKMESSAGES_H
