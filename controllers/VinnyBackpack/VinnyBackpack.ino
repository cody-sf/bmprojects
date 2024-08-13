#include <math.h>
#include <WiFi.h>
#include <FastLED.h>
#include <GlobalDefaults.h>
#include <LightShow.h>
#include <ControlCenter.h>
#include <LocationService.h>
#include <NetworkManagerCore.h>
#include <Position.h>
#include <WebServer.h>
#include <NetworkMessages.h>
#include <DeviceRoles.h>

#define WEB_SERVER_LISTENING_PORT 80
#define LED_OUTPUT_PIN 26
#define NUM_LEDS 192
#define COLOR_ORDER GRB
#define LIGHT_SCENE_RESEND_TIME 3000
#define TIME_REFERENCE_RESEND_TIME 3000
#define BUTTON_PIN 2

static Device backpack;
static const char wifi_ssid[] = "Vinny's LED Controller";
static const char wifi_password[] = "vinny1234";
static const IPAddress local_address(10, 0, 0, 1);
static const IPAddress multicast_listening_address(239, 0, 0, 1);
static const IPAddress multicast_remote_address(239, 0, 0, 2);
static CRGB leds[NUM_LEDS];
static Clock backpackClock;
std::vector<CLEDController *> led_controllers;
static LightShow *light_show = nullptr;
static NetworkManagerCore network_manager(wifi_ssid, wifi_password, local_address,
                                          multicast_listening_address, multicast_remote_address, MULTICAST_LISTENING_PORT);
static LocationService location_service;
static ControlCenter control_center(location_service, *light_show, backpack);
static WebServer web_server(WEB_SERVER_LISTENING_PORT, control_center);
static unsigned long last_light_scene_resend_time = 0;
static unsigned long last_time_reference_resend_time = 0;

void setup()
{

    Serial.begin(115200);
    Serial.println("system up!");

    // Setup Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifi_ssid, wifi_password);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    Serial.println("starting network manager");
    network_manager.create_access_point();
    web_server.start_serving_requests();
    location_service.start_tracking_position();

    led_controllers.push_back(&FastLED.addLeds<WS2811, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS));
    light_show = new LightShow(led_controllers, backpackClock);
    control_center.update_lightshow(light_show);
}

void loop()
{

    int buttonValue = digitalRead(BUTTON_PIN);

    if (buttonValue == LOW)
    {
        Serial.println("Button Pressed");
        control_center.button_press_cycle_lightshow();
        delay(300);
    }

    unsigned long now = backpackClock.now();

    network_manager.update_connectivity();
    location_service.update_position();
    web_server.process_request();
    control_center.update_state();
    if (network_manager.devices_connected())
    {
        if (light_show->scene_changed() || (now > last_light_scene_resend_time + LIGHT_SCENE_RESEND_TIME))
        {
            last_light_scene_resend_time = now;

            NetworkMessage message_buffer;
            memset(&message_buffer, 0, sizeof(NetworkMessage));
            message_buffer.type = NetworkMessage::MessageType::light_scene;
            message_buffer.selected_devices = control_center.selected_devices();
            light_show->export_scene(&message_buffer.messages.light_scene);
            network_manager.send_message(&message_buffer);
        }

        if (now > last_time_reference_resend_time + TIME_REFERENCE_RESEND_TIME)
        {
            last_time_reference_resend_time = now;

            NetworkMessage message_buffer;
            memset(&message_buffer, 0, sizeof(NetworkMessage));
            message_buffer.type = NetworkMessage::MessageType::time_reference;
            message_buffer.selected_devices = DEVICE_ROLE_ALL;
            message_buffer.messages.time_reference.timestamp = now;
            network_manager.send_message(&message_buffer);
        }
    }

    static bool is_position_known = false;
    if (is_position_known != location_service.is_current_position_available())
    {
        if (location_service.is_current_position_available())
        {
          Serial.println("Current Position Availablke");
        }
        else
        {
          Serial.println("Current Position Not Available");
        }

        is_position_known = location_service.is_current_position_available();
    }

    light_show->render();
}
