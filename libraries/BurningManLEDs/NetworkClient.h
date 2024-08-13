// #ifndef NETWORKCLIENT_H
// #define NETWORKCLIENT_H

// #include <WiFi.h>
// #include <NetworkMessages.h>

// class NetworkClient
// {
// public:
//     NetworkClient(const char *wifi_ssid, const char *wifi_password, IPAddress multicast_listening_address, uint16_t multicast_listening_port);
//     void update_connectivity();
//     bool is_connected();
//     size_t receive_message(NetworkMessage *message);

// private:
//     static constexpr unsigned long min_wifi_begin_interval_ = 30000;

//     const char *wifi_ssid_;
//     const char *wifi_password_;
//     const IPAddress multicast_listening_address_;
//     uint16_t multicast_listening_port_;
//     uint8_t wifi_status_;
//     WiFiUDP udp_;
//     unsigned long last_wifi_begin_time_;
// };

// #endif // NETWORKCLIENT_H
