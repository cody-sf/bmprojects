
// #include "NetworkClient.h"

// NetworkClient::NetworkClient(const char* wifi_ssid, const char* wifi_password, IPAddress multicast_listening_address, uint16_t multicast_listening_port) :
//     wifi_ssid_(wifi_ssid),
//     wifi_password_(wifi_password),
//     multicast_listening_address_(multicast_listening_address),
//     multicast_listening_port_(multicast_listening_port),
//     wifi_status_(WL_IDLE_STATUS),
//     last_wifi_begin_time_(0)
// {}

// void NetworkClient::update_connectivity()
// {
//     unsigned long now = millis();
//     uint8_t new_wifi_status = WiFi.status();

//     if (wifi_status_ != new_wifi_status) {
//         if (new_wifi_status == WL_CONNECTED) {
//             Serial.print("Connected to wifi network: SSID = ");
//             Serial.print(wifi_ssid_);
//             Serial.print(", IP = ");
//             Serial.print(WiFi.localIP());
//             Serial.print(" / ");
//             Serial.print(WiFi.subnetMask());
//             Serial.print(", gateway = ");
//             Serial.println(WiFi.gatewayIP());

//             udp_.beginMulticast(multicast_listening_address_, multicast_listening_port_);
//         }
//         else if ((new_wifi_status != WL_CONNECTED) && (wifi_status_ == WL_CONNECTED)) {
//             udp_.stop();

//             Serial.print("Disconnected from wifi network: SSID = ");
//             Serial.println(wifi_ssid_);
//         }

//         wifi_status_ = new_wifi_status;
//     }

//     if (wifi_status_ != WL_CONNECTED) {
//         if (now > (last_wifi_begin_time_ + min_wifi_begin_interval_)) {
//             WiFi.begin(wifi_ssid_, wifi_password_);
//             last_wifi_begin_time_ = now;
//         }
//     }
// }

// bool NetworkClient::is_connected()
// {
//     return wifi_status_ == WL_CONNECTED;
// }

// size_t NetworkClient::receive_message(NetworkMessage* message) {
//     if (!is_connected()) {
//         return 0;
//     }

//     if (udp_.parsePacket() <= 0) {
//         return 0;
//     }

//     uint8_t* buffer = (uint8_t*) message;
//     size_t message_length = udp_.read(buffer, sizeof(NetworkMessage));

//     Serial.print("Received packet: size = ");
//     Serial.print(message_length);
//     Serial.print(", remote IP = ");
//     Serial.print(udp_.remoteIP());
//     Serial.print(", remote port = ");
//     Serial.println(udp_.remotePort());

//     for (size_t i = 0; i < message_length; i++) {
//         Serial.print((uint8_t)buffer[i]);
//         Serial.print(" ");
//     }
//     Serial.println();

//     return message_length;
// }
