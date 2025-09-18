#pragma once

#include <string>
#include <nvs_flash.h>
#include <nvs.h>
#include "MeshasticCompactStructs.hpp"

class TConfig {
   public:
    std::string ssid;
    std::string password;
    int channel;

    TConfig() : ssid(""), password(""), channel(1) {}

    bool load_radio(LoraConfig& lora_config) {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("radio_cfg", NVS_READONLY, &handle);
        if (err != ESP_OK) return false;

        char tmpstr[64] = {0};
        size_t tmpstrlen = sizeof(tmpstr);
        err = nvs_get_str(handle, "freqstr", tmpstr, &tmpstrlen);
        if (err == ESP_OK) {
            // Parse frequency string
            std::string freq_string(tmpstr);
            lora_config.frequency = std::stof(freq_string);
        }
        err = nvs_get_str(handle, "bwstr", tmpstr, &tmpstrlen);
        if (err == ESP_OK) {
            // Parse bandwidth string
            std::string bw_string(tmpstr);
            lora_config.bandwidth = std::stof(bw_string);
        }
        err = nvs_get_u8(handle, "sf", &lora_config.spreading_factor);
        if (err != ESP_OK) lora_config.spreading_factor = 11;
        err = nvs_get_u8(handle, "cr", &lora_config.coding_rate);
        if (err != ESP_OK) lora_config.coding_rate = 5;
        err = nvs_get_i8(handle, "outpower", &lora_config.output_power);
        if (err != ESP_OK) lora_config.output_power = 20;

        nvs_close(handle);
        return true;
    }

    bool
    save_radio(LoraConfig& lora_config) {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("radio_cfg", NVS_READWRITE, &handle);
        if (err != ESP_OK) return false;

        err = nvs_set_str(handle, "freqstr", std::to_string(lora_config.frequency).c_str());
        err = nvs_set_str(handle, "bwstr", std::to_string(lora_config.bandwidth).c_str());
        err = nvs_set_u8(handle, "sf", lora_config.spreading_factor);
        err = nvs_set_u8(handle, "cr", lora_config.coding_rate);
        err = nvs_set_i8(handle, "outpower", lora_config.output_power);

        nvs_commit(handle);
        nvs_close(handle);
        return true;
    }
};