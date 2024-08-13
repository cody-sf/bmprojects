#include "NetworkManagerCore.h"

NetworkManagerCore::NetworkManagerCore(const char *wifi_ssid, const char *wifi_password,
                                       const IPAddress &unicast_local_address, const IPAddress &multicast_local_address,
                                       const IPAddress &multicast_remote_address, const uint16_t multicast_port) : wifi_ssid_(wifi_ssid),
                                                                                                                   wifi_password_(wifi_password),
                                                                                                                   unicast_local_address_(unicast_local_address),
                                                                                                                   multicast_local_address_(multicast_local_address),
                                                                                                                   multicast_remote_address_(multicast_remote_address),
                                                                                                                   multicast_port_(multicast_port),
                                                                                                                   access_point_started_(false),
                                                                                                                   devices_connected_(false),
                                                                                                                   wifi_status_(WL_DISCONNECTED)
{
}

void NetworkManagerCore::create_access_point()
{
    // Configuring a static IP might not be necessary for an Access Point, but it's here if needed
    IPAddress gateway(192, 168, 4, 1);                          // Set to your desired gateway
    IPAddress subnet(255, 255, 255, 0);                         // Set to your desired subnet
    WiFi.softAPConfig(unicast_local_address_, gateway, subnet); // Configure the AP

    bool result = WiFi.softAP(wifi_ssid_, wifi_password_);
    if (!result)
    {
        Serial.println("Creating wifi AP failed");
        return;
    }

    // Assuming you need to start a UDP multicast, adjust as needed
    udp_.begin(multicast_port_);
    access_point_started_ = true;
}

void NetworkManagerCore::update_connectivity()
{
    if (!access_point_started_)
    {
        return;
    }

    uint8_t new_wifi_status = WiFi.status();

    if (wifi_status_ != new_wifi_status)
    {
        wifi_status_ = new_wifi_status;
    }
}

void NetworkManagerCore::send_message(const NetworkMessage *message)
{
    uint8_t *buffer = (uint8_t *)message;
    size_t buffer_size = sizeof(NetworkMessage);

    int result = udp_.beginPacket(multicast_remote_address_, multicast_port_);
    if (!result)
    {
        Serial.println("Initializing UDP packet failed");
        return;
    }

    size_t message_length = udp_.write(buffer, buffer_size);
    if (message_length != buffer_size)
    {
        Serial.println("UDP packet data truncated");
    }

    result = udp_.endPacket();
    if (!result)
    {
        Serial.println("Sending UDP packet failed");
        return;
    }

    Serial.println("Sending UDP packet successful");

    // Serial.print("Sent packet: size = ");
    // Serial.print(message_length);
    // Serial.print(", remote IP = ");
    // Serial.print(multicast_remote_address);
    // Serial.print(", remote port = ");
    // Serial.println(MULTICAST_LISTENING_PORT);

    // for (size_t i = 0; i < buffer_size; i++) {
    //     Serial.print((uint8_t)buffer[i]);
    //     Serial.print(" ");
    // }
    // Serial.println();
}
