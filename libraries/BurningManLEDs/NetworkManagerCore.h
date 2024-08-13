#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <WiFi.h>
#include <NetworkMessages.h>

class NetworkManagerCore
{
public:
    NetworkManagerCore(const char *wifi_ssid, const char *wifi_password, const IPAddress &unicast_local_address,
                       const IPAddress &multicast_local_address, const IPAddress &multicast_remote_address,
                       const uint16_t multicast_port);
    void create_access_point();
    void update_connectivity();
    bool devices_connected() { return devices_connected_; }
    void send_message(const NetworkMessage *message);

private:
    const char *wifi_ssid_;
    const char *wifi_password_;
    const IPAddress unicast_local_address_;
    const IPAddress multicast_local_address_;
    const IPAddress multicast_remote_address_;
    const uint16_t multicast_port_;
    bool access_point_started_;
    bool devices_connected_;
    uint8_t wifi_status_;
    WiFiUDP udp_;
};

#endif // NETWORKMANAGER_H
