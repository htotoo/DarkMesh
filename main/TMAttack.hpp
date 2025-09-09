#ifndef TMATTACK_HPP
#define TMATTACK_HPP

#include <string>
#include "MeshtasticCompact.hpp"
#include "esp_random.h"
#include "esp_log.h"

enum class AttackType {
    NONE,
    POS_POISON,
    MSG_INJECT,
    NODE_IMPERSONATE
};

class TMAttack {
   public:
    void setRadio(MeshtasticCompact* radio) {
        meshtasticCompact = radio;
    }
    void setAttackType(AttackType type) {
        current_attack = type;
    }
    void loop();
    void setPosAttackParams(double min_latitude, double max_latitude, double min_longitude, double max_longitude) {
        min_lat = min_latitude;
        max_lat = max_latitude;
        min_lon = min_longitude;
        max_lon = max_longitude;
    }

   private:
    MeshtasticCompact* meshtasticCompact = nullptr;
    std::string emoji = "😈";
    double min_lat = -90.0;
    double max_lat = 90.0;
    double min_lon = -180.0;
    double max_lon = 180.0;
    uint32_t timer = 0;  // 0.1 second timer
    AttackType current_attack = AttackType::NONE;
};
#endif  // TMATTACK_HPP