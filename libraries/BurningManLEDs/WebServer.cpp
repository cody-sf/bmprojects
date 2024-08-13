#include "WebServer.h"
#include "StringUtils.h"

WebServer::WebServer(uint16_t port, ControlCenter& control_center) :
    http_server_(WiFiServer(port)),
    control_center_(control_center)
{}

void WebServer::start_serving_requests() {
    http_server_.begin();
}

void WebServer::process_request()
{
    WiFiClient client = http_server_.available();

    if (!client) {
        return;
    }

    char request_buffer[HTTP_REQUEST_MAX_SIZE];
    size_t index = 0;

    while (client.connected() && client.available()) {
        char c = client.read();
        if ((index < HTTP_REQUEST_MAX_SIZE - 1) && (c != '\n')) {
            request_buffer[index++] = c;
        }
        else {
            break;
        }
    }

    request_buffer[index] = '\0';
    
    if (client.connected()) {
        // The first line of the request will look like this: GET /modes/color-wheel HTTP/1.1
        // First slice off the HTTP version.
        char* cp = strrchr(request_buffer, ' ');
        if (cp) {
            *cp = '\0';
        }

        // Now slice off the GET part and leading '/'.
        char* request = request_buffer;
        cp = strchr(request_buffer, '/');
        if (cp) {
            request = cp + 1;
        }

        if (strlen(request) > 0) {
            if (StringUtils::strings_match(request, "status")) {
                write_status_json_(client);
            }
            else {
                char* request_params[HTTP_REQUEST_MAX_PARAMS];
                index = 0;
                char* param = strtok(request, "/");

                while (param && (index < HTTP_REQUEST_MAX_PARAMS)) {
                    request_params[index++] = param;
                    param = 
                    strtok(nullptr, "/");
                }

                bool success = control_center_.execute_command(request_params, index);
                write_standard_response_(client, success);
            }
        }
        else {
            write_command_center_html_(client);
        }
    }

    client.stop();
}

void WebServer::write_standard_response_(WiFiClient& client, bool success)
{
    if (success) {
        client.println("HTTP/1.1 200 OK");
    }
    else {
        client.println("HTTP/1.1 404 Not Found");
    }
    
    client.println("Content-type:text/html");
    client.println();
}

void WebServer::write_status_json_(WiFiClient& client)
{
    write_standard_response_(client, true);
    control_center_.write_status(client);
    client.println();
}

void WebServer::write_command_center_html_(WiFiClient& client)
{
    write_standard_response_(client, true);
    size_t max_chunk_size = HTTP_MAX_WRITE_SIZE;
    size_t bytes_remaining = sizeof(control_center_html);
    const char* position = control_center_html;

    while (bytes_remaining > 0) {
        size_t chunk_size = std::min(bytes_remaining, max_chunk_size);
        size_t bytes = client.write(position, chunk_size);
        Serial.println(bytes);
        position += chunk_size;
        bytes_remaining -= chunk_size;
    }

    client.println();
}
