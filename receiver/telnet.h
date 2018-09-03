#ifndef ODBIORNIK_TELNET_H
#define ODBIORNIK_TELNET_H

#include <mutex>
#include <atomic>
#include "../common/helper.h"
#include "radioStation.h"

void telnet(int uiPort, std::atomic<int> &mainSession, std::atomic<bool> &changeUi, std::atomic<unsigned int> &stationSelection,
           std::vector<RadioStation> *radioStations, std::mutex &radioStationsMutex);

#endif
