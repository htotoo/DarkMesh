
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_random.h"
#include "MeshtasticCompact.hpp"

static const char* TAG = "DarkMesh";

extern "C" {
void app_main();
}

Radio_PINS radio_pins = {9, 11, 10, 8, 14, 12, 13};  // Default radio pins for Heltec WSL V3.
LoraConfig lora_config = {
    .frequency = 433.125,  // default frequency
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

void app_main(void) {
    ESP_LOGI(TAG, "Radio initializing...");
    meshtasticCompact.RadioInit(RadioType::SX1262, radio_pins, lora_config);
    ESP_LOGI(TAG, "Radio initialized.");
    meshtasticCompact.setAutoFullNode(false);                                                                                               // we don't want to be a full node
    meshtasticCompact.setSendHopLimit(7);                                                                                                   // max hop limit
    meshtasticCompact.setStealthMode(true);                                                                                                 // stealth mode, we don't
    meshtasticCompact.setSendEnabled(true);                                                                                                 // we want to send packets
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
            meshtasticCompact.SendMyNodeInfo();
            meshtasticCompact.SendMyPosition();
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // wait 100 milliseconds
    }
}
