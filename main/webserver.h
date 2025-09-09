#include <string.h>

#include <freertos/FreeRTOS.h>
#include <esp_http_server.h>
#include <freertos/task.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_spiffs.h"
#include "spi_flash_mmap.h"
#include "parson.h"

static httpd_handle_t server = NULL;
static bool disable_esp_async = false;  // for example while in file transfer mode, don't send anything else

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
void handle_start_attack(const char* attack_type, JSON_Object* params);
void handle_stop_attack();
void handle_send_message(const char* source_id, const char* message);
void handle_set_config(JSON_Object* params);
void handle_initme();

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
}

// Function to decode a URL-encoded string
std::string url_decode(const std::string encoded_string) {
    std::string decoded_string;
    for (size_t i = 0; i < encoded_string.length(); ++i) {
        if (encoded_string[i] == '%' && i + 2 < encoded_string.length() && isxdigit(encoded_string[i + 1]) && isxdigit(encoded_string[i + 2])) {
            int high = hex_to_int(encoded_string[++i]);
            int low = hex_to_int(encoded_string[++i]);
            decoded_string += static_cast<char>((high << 4) | low);
        } else if (encoded_string[i] == '+') {
            decoded_string += ' ';
        } else {
            decoded_string += encoded_string[i];
        }
    }
    return decoded_string;
}

// root / get handler.
static esp_err_t get_req_handler(httpd_req_t* req) {
    const uint32_t index_len = index_end - index_start;
    int response = httpd_resp_send(req, index_start, index_len);
    return response;
}

// send data to all ws clients.
bool ws_sendall(uint8_t* data, size_t len, bool asyncmsg = false) {
    if (disable_esp_async && asyncmsg) {
        return false;
    }
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

    if (ret != ESP_OK) {
        return false;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
        }
    }
    return true;
}

// websocket handler
static esp_err_t handle_ws_req(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t* buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE("WEBS", "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE("WEBS", "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE("WEB", "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }

        JSON_Value* root_value = json_parse_string((const char*)buf);
        if (root_value == NULL) {
            ESP_LOGE("WEB", "Error parsing JSON.");
            free(buf);
            return ESP_FAIL;
        }

        JSON_Object* root_object = json_value_get_object(root_value);
        const char* action = json_object_get_string(root_object, "action");

        if (action) {
            ESP_LOGI("WEB", "Action found: %s", action);

            if (strcmp(action, "start_attack") == 0) {
                const char* attack_type = json_object_get_string(root_object, "attack_type");
                JSON_Object* params = json_object_get_object(root_object, "params");
                if (attack_type) {
                    handle_start_attack(attack_type, params);
                }
            } else if (strcmp(action, "stop_attack") == 0) {
                handle_stop_attack();
            } else if (strcmp(action, "send_message") == 0) {
                const char* source_id = json_object_get_string(root_object, "source_id");
                const char* message = json_object_get_string(root_object, "message");
                if (source_id && message) {
                    handle_send_message(source_id, message);
                }
            } else if (strcmp(action, "set_config") == 0) {
                JSON_Object* params = json_object_get_object(root_object, "params");
                if (params) {
                    handle_set_config(params);
                }
            } else if (strcmp(action, "initme") == 0) {
                handle_initme();
            } else {
                ESP_LOGE("WEB", "Unknown action: %s", action);
            }
        } else {
            ESP_LOGE("WEB", "Could not find 'action' in JSON.");
        }

        // Cleanup
        json_value_free(root_value);
        free(buf);
    }
    return ESP_OK;
}

// config web server part
static httpd_handle_t setup_websocket_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 10;

    httpd_uri_t uri_get = {.uri = "/",
                           .method = HTTP_GET,
                           .handler = get_req_handler,
                           .user_ctx = NULL,
                           .is_websocket = false,
                           .handle_ws_control_frames = false,
                           .supported_subprotocol = NULL};

    httpd_uri_t ws = {.uri = "/ws",
                      .method = HTTP_GET,
                      .handler = handle_ws_req,
                      .user_ctx = NULL,
                      .is_websocket = true,
                      .handle_ws_control_frames = false,
                      .supported_subprotocol = NULL};

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &ws);
    }

    return server;
}

// start web server, set it up,..
extern "C" void init_httpd() {
    server = setup_websocket_server();
}