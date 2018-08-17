#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <fstream>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <climits>
#include <poll.h>
#include <csignal>

#include "../common/err.h"
#include "../common/helper.h"
#include "../common/logger.h"
#include "../common/debug.h"
#include "radioStation.h"

const char LOOKUP[] = "ZERO_SEVEN_COME_IN\n";

void discover(std::atomic<int> &mainSession, std::atomic<int> &stationSelection,
              std::string discover_addr, uint16_t controlPort, std::atomic<bool> &changeUi,
              std::vector<RadioStation> *radioStations, std::mutex &radioStationsMutex) {

    logger logger("receiver/discover");

    int sock, optval;
    struct sockaddr_in remote_address;

    /* otworzenie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) syserr("socket");

    // tryb nie blokujący
    int on = 1;
    ioctl(sock, FIONBIO, &on);

    /* uaktywnienie rozgłaszania (ang. broadcast) */
    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    optval = 0;
    if (setsockopt(sock, SOL_IP, IP_MULTICAST_LOOP, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt loop");

    /* podpięcie się pod lokalny adres i port */
    struct sockaddr_in local_address;
    struct sockaddr_in client_address;
    socklen_t rcva_len;

    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(0);
    if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
        syserr("bind");
//
//    /* ustawienie adresu i portu odbiorcy */
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(controlPort);
    if (inet_aton(discover_addr.c_str(), &remote_address.sin_addr) == 0)
        syserr("inet_aton");

    sendto(sock, LOOKUP, sizeof(LOOKUP), 0, (struct sockaddr *) &remote_address, sizeof(remote_address));
    time_t lastDiscoverRequest;
    time(&lastDiscoverRequest);
    if (debug) logger.log() << "send request" << lastDiscoverRequest << std::endl;

    char buffer[1024];

    while (true) {
        if (debug) {
            logger.log() << "current radio stations:" << std::endl;
            for (auto station : *radioStations) {
                std::string name(station.name);
                logger.log() << name << std::endl;
            }
        }

        time_t currentTime;
        time(&currentTime);
        bool sendDiscoverRequestAndClearOldStations = (currentTime - lastDiscoverRequest)  > 5;
        if (sendDiscoverRequestAndClearOldStations) {
            if (debug) logger.log() << "send discover Request and clear old stations" << std::endl;

            lastDiscoverRequest = currentTime;
            sendto(sock, LOOKUP, sizeof(LOOKUP), 0, (struct sockaddr *) &remote_address, sizeof(remote_address));

            radioStationsMutex.lock();
            std::vector<RadioStation> newRadioStations;
            int i = 0;
            bool makeChangeUi = false;
            for (auto station : *radioStations) {
                if ((currentTime - station.lastSeen) <= 20) {
                    newRadioStations.push_back(station);
                } else if (stationSelection == i) {
                    stationSelection.store(0);
                    makeChangeUi = true;
                    mainSession++;
                    if (debug) logger.log() << "radio station removed from list, restart\n" <<
                                               "current time" << currentTime << " last seen " << station.lastSeen << std::endl;
                } else if (i < stationSelection) {
                    stationSelection--;
                    makeChangeUi = true;
                }
                i++;
            }
            if (newRadioStations.size() < radioStations->size()) {
                makeChangeUi = true;
            }

            radioStations->clear();
            radioStations->assign(newRadioStations.begin(), newRadioStations.end());
            changeUi.store(makeChangeUi);

            radioStationsMutex.unlock();
        }

        int flags = 0; // we do not request anything special

        rcva_len = (socklen_t) sizeof(client_address);
        ssize_t bytes = recvfrom(sock, buffer, sizeof(buffer), flags,
                                 (struct sockaddr *) &client_address, &rcva_len);;

        if (bytes < 0) {
            if (errno == EAGAIN) {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(100ms);
                continue;
            } else {
                if (debug) logger.log() << "error" << strerror(errno) << std::endl;
                break;
            }
        } else if (bytes == 0) {
            if (debug) logger.log() << "Connection closed" << std::endl;
        }

        std::string str(buffer);
        if (debug) logger.log() << "Received: " << str << std::endl;

        if (contains(str, "BOREWICZ_HERE")) {
            if (debug) logger.log() << "Contains BOREWICZ_HERE" << std::endl;

            char mcast[20];
            int dataBig;
            char name[64];
            sscanf(buffer, "%*s %s %d %64[^\n]%c", mcast, &dataBig, name);

            std::string parsedMcast(mcast);
            std::string parsedName(name);
            int parsedPort = dataBig;

            radioStationsMutex.lock();

            if (debug) {
                logger.log() << "parsed mcast " << parsedMcast << std::endl;
                logger.log() << "parsed name " << parsedName << std::endl;
                logger.log() << "parsed port " << parsedPort << std::endl;
            }

            bool found = false;
            time(&currentTime);
            for (auto &station : *radioStations) {
                if (debug) {
                    logger.log() << "station mcast " << toString(station.address) << std::endl;
                    logger.log() << "station name " << toString(station.name) << std::endl;
                    logger.log() << "station port " << station.dataPort << std::endl;
                }

                if (toString(station.address) == parsedMcast
                    && station.dataPort == parsedPort
                    && toString(station.name) == parsedName) {
                    found = true;
                    station.lastSeen = currentTime;
                    if (debug) logger.log() << "last seen updated for: " << station.name << " last seen: " << station.lastSeen << std::endl;
                } else {
                    if (debug) logger.log() << "stations data not match" << std::endl;
                }
            }
            if (!found) {
                struct RadioStation station;
                for (int i = 0; i < 64; i++) {
                    station.name[i] = name[i];
                }
                for (int i = 0; i < 20; i++) {
                    station.address[i] = mcast[i];
                }
                station.dataPort = parsedPort;
                station.lastSeen = currentTime;
                station.sin_addr = client_address.sin_addr;
                station.sin_port = client_address.sin_port;
                radioStations->push_back(station);
                changeUi.store(true);
            }
            radioStationsMutex.unlock();
        }
    }

}

