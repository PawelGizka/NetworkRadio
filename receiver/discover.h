#ifndef DISCOVER_H
#define DISCOVER_H

#include <mutex>
#include <atomic>
#include "../common/helper.h"
#include "radioStation.h"

void discover(std::atomic<int> &mainSession, std::atomic<unsigned int> &stationSelection,
              std::string discover_addr, uint16_t controlPort, std::atomic<bool> &changeUi,
              std::vector<RadioStation> *radioStations, std::mutex &radioStationsMutex);

#endif
