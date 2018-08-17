//
// Created by pawel on 13.06.18.
//

#ifndef ODBIORNIK_TELNET_H
#define ODBIORNIK_TELNET_H

#include <mutex>
#include <atomic>
#include "../common/helper.h"
#include "radioStation.h"

int telnet(int uiPort, std::atomic<int> &mainSession, std::atomic<bool> &changeUi, std::atomic<int> &stationSelection,
           std::vector<RadioStation> *radioStations, std::mutex &radioStationsMutex);

#endif //ODBIORNIK_TELNET_H
