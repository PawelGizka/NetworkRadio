//
// Created by pawel on 13.06.18.
//

#ifndef ODBIORNIK_DISCOVER_H
#define ODBIORNIK_DISCOVER_H

#include <mutex>
#include <atomic>
#include "../common/helper.h"
#include "radioStation.h"

void discover(std::atomic<int> &mainSession, std::atomic<int> &stationSelection,
              std::string discover_addr, uint16_t controlPort, std::atomic<bool> &changeUi,
              std::vector<RadioStation> *radioStations, std::mutex &radioStationsMutex);

#endif //ODBIORNIK_DISCOVER_H
