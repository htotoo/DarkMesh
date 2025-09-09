#ifndef TMATTACK_HPP
#define TMATTACK_HPP

#include <string>
#include "MeshtasticCompact.hpp"
#include "esp_random.h"
#include "esp_log.h"

enum class AttackType {
    NONE,
    POS_POISON,
    NODE_FLOOD,
    NAME_CHANGE,
    PKI_POISON,
    DDOS,
};

class TMAttack {
   public:
    void setRadio(MeshtasticCompact* radio) {
        meshtasticCompact = radio;
    }

    void setTarget(uint32_t target) {
        this->target = target;
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
    std::string getCurrentAttackTypeString() const {
        switch (current_attack) {
            case AttackType::NONE:
                return "None";
            case AttackType::POS_POISON:
                return "pos_poison";
            case AttackType::NODE_FLOOD:
                return "node_flood";
            case AttackType::NAME_CHANGE:
                return "name_change";
            case AttackType::PKI_POISON:
                return "pki_poison";
            case AttackType::DDOS:
                return "ddos";
            default:
                return "Unknown";
        }
    }

    uint32_t getRandomTarget();

   private:
    void rndPos();

    MeshtasticCompact* meshtasticCompact = nullptr;
    std::string emoji = "😈";
    double min_lat = -90.0;
    double max_lat = 90.0;
    double min_lon = -180.0;
    double max_lon = 180.0;
    uint32_t timer = 0;            // 0.1 second timer
    uint32_t target = 0xffffffff;  // everybody or flood
    AttackType current_attack = AttackType::NONE;
};
#endif  // TMATTACK_HPP