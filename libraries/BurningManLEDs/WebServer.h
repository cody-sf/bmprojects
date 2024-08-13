#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <WiFi.h>
#include "ControlCenter.h"

#define HTTP_REQUEST_MAX_SIZE 100
#define HTTP_REQUEST_MAX_PARAMS 10
#define HTTP_MAX_WRITE_SIZE 1024

class WebServer
{
public:
    WebServer(uint16_t port, ControlCenter &control_center);
    void start_serving_requests();
    void process_request();

private:
    void write_standard_response_(WiFiClient &client, bool success);
    void write_status_json_(WiFiClient &client);
    void write_command_center_html_(WiFiClient &client);

    WiFiServer http_server_;
    ControlCenter &control_center_;
};

#endif // WEBSERVER_H
