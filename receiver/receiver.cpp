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
#include <cmath>
#include <future>

#include "../common/err.h"
#include "../common/helper.h"
#include "telnet.h"
#include "discover.h"
#include "retransmit.h"
#include "outUploader.h"
#include "radioStation.h"
#include "../common/logger.h"
#include "../common/debug.h"

int main (int argc, char *argv[]) {

    //adres używany przez odbiornik do wykrywania aktywnych nadajników,
    //ustawiany parametrem -d odbiornika, domyślnie 255.255.255.255
    std::string discoverAddress("255.255.255.255");

    //port TCP, na którym udostępniany jest prosty interfejs tekstowy do przełączania się między stacjami,
    // domyślnie 10000 + (numer_albumu % 10000); ustawiany parametrem -U odbiornika
    int uiPort = 10007;

    //port UDP używany do transmisji pakietów kontrolnych, ustawiany parametrem -C
    // nadajnika i odbiornika, domyślnie 30000 + (numer_albumu % 10000)
    int ctrlPort = 30000;

    //rozmiar w bajtach bufora, ustawiany parametrem -b odbiornika, domyślnie 64kB (65536B)
    int bsize = 65536*8;

    //czas (w milisekundach) pomiędzy wysłaniem kolejnych raportów o brakujących paczkach
    // (dla odbiorników) oraz czas pomiędzy kolejnymi retransmisjami paczek, ustawiany parametrem -R, domyślnie 250.
    int rtime = 250;

    InputParser input(argc, argv);

    std::string option = input.getCmdOption("-d");
    if (!option.empty()){
        discoverAddress = option;
    }

    option = input.getCmdOption("-U");
    if (!option.empty()){
        uiPort = std::stoi(option);
    }

    option = input.getCmdOption("-C");
    if (!option.empty()){
        ctrlPort = std::stoi(option);
    }

    option = input.getCmdOption("-b");
    if (!option.empty()){
        bsize = std::stoi(option);
    }

    option = input.getCmdOption("-R");
    if (!option.empty()){
        rtime = std::stoi(option);
    }


    /* zmienne i struktury opisujące gniazda */
    ////////////////////////
    char *packetBuffer = new char[bsize];

    std::vector<RadioStation> *radioStations = new std::vector<RadioStation>;
    std::mutex radioStationsMutex;

    std::atomic<int> mainSession(0);//any change to mainSession results in again playback

    std::atomic<bool> changeUi(true);
    std::atomic<int> stationSelection(0);

    std::atomic<uint64_t > currentlyWritingPacket(0);

    std::thread listenerThread{[&mainSession, &stationSelection, discoverAddress, ctrlPort, &changeUi, radioStations, &radioStationsMutex]
                               { discover(mainSession, stationSelection, discoverAddress, ctrlPort, changeUi, radioStations, radioStationsMutex); }};

    std::thread telnetThread{[uiPort, &mainSession, &changeUi, &stationSelection, radioStations, &radioStationsMutex]
                             { telnet(uiPort, mainSession, changeUi, stationSelection, radioStations, radioStationsMutex); }};

    ///////////////

    /* czytanie tego, co odebrano */
    bool finishIt = false;

    logger mainLogger("receiver/main");
    logger retransmitLogger("receiver/retransmit");
    logger outUploaderLogger("receiver/outUploader");

    while (!finishIt) {
        if (debug) mainLogger.log() << "receiving started" << std::endl;

        int currentMainSession = mainSession.load();
        currentlyWritingPacket.store(0);
        std::vector<RadioStation> currentRadioStations;
        radioStationsMutex.lock();
        currentRadioStations = *radioStations;
        radioStationsMutex.unlock();

        if (currentRadioStations.empty()) {
            if (debug) mainLogger.log() << "no radio stations" << std::endl;
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(200ms);
            continue;
        }
        struct RadioStation currentRadioStation = currentRadioStations.at(stationSelection.load());
        std::string multicastAddress(currentRadioStations.at(stationSelection.load()).address);
        int dataPort = currentRadioStations.at(stationSelection.load()).dataPort;

        int sock;
        struct sockaddr_in local_address;
        struct ip_mreq ip_mreq;

        /* otworzenie gniazda */
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
            syserr("socket");

        /* podpięcie się do grupy rozsyłania (ang. multicast) */
        ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (inet_aton(multicastAddress.c_str(), &ip_mreq.imr_multiaddr) == 0)
            syserr("inet_aton");

        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
            syserr("setsockopt");

        /* podpięcie się pod lokalny adres i port */
        local_address.sin_family = AF_INET;
        local_address.sin_addr.s_addr = htonl(INADDR_ANY);
        local_address.sin_port = htons(static_cast<uint16_t>(dataPort));
        if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
            syserr("bind ehh");

        /* zmienne obsługujące komunikację */
        char temp[bsize];
        ssize_t rcv_len;
        int i;

        rcv_len = read(sock, (void*) temp, sizeof temp);
        ssize_t packetSize = rcv_len - 16;

        uint64_t sessionId = be64toh(*((uint64_t *) temp));
        uint64_t byte0 = be64toh(*((uint64_t *) (temp + 8)));

        uint64_t bufferSize = floor(bsize / packetSize);
        std::atomic<bool> *blocksReady = new std::atomic<bool>[bufferSize];

        blocksReady[0].store(true);
        for (int i = 1; i < bufferSize; i++) {
            blocksReady[i].store(false);
        }

        uint64_t lastGoodSegment = 0;
        uint64_t lastRetransmitSegment = 0;

        bool outUploaderStarted = false;

        while (mainSession.load() == currentMainSession) {
            memset(temp, 0, sizeof(temp));
            rcv_len = read(sock, (void*) temp, sizeof temp);

            uint64_t currentSessionId = be64toh(*((uint64_t *) temp));
            uint64_t currentByte = be64toh(*((uint64_t *) (temp + 8)));

            uint64_t currentSegment = (currentByte - byte0) / packetSize;

            if (debug) mainLogger.log() << "current session id: "
                << currentSessionId << " current byte: " << currentByte << " current segment" << currentSegment << std::endl;

            if (currentSessionId > sessionId) {
                mainSession++;
                break;
            }

            if (currentSessionId < sessionId || currentByte < byte0 || currentSegment <= lastGoodSegment) {
                continue;
            }

            if (currentlyWritingPacket.load() + bufferSize < currentSegment) {
                mainSession++;
                break;
            }


            if (currentSegment > lastGoodSegment + 1) {
                //we have hole, schedule LOUDER PLEASE
                if (debug) mainLogger.log() << "we have a hole, schedule louder" << std::endl;

                uint64_t from = std::max(lastGoodSegment + 1, lastRetransmitSegment + 1); //inclusive
                uint64_t to = currentSegment - 1; //inclusive
                lastRetransmitSegment = to;

                std::thread([&mainSession, currentMainSession, blocksReady, from, to, currentRadioStation, packetSize, rtime, byte0, bufferSize, &retransmitLogger]
                            { retransmit(mainSession, currentMainSession, blocksReady, from, to, currentRadioStation, packetSize, rtime, byte0, bufferSize, retransmitLogger); }).detach();

            }

            if (currentSegment == lastGoodSegment + 1){
                lastGoodSegment = currentSegment;
            }

            for (int j = 0; j < packetSize; j++) {
                packetBuffer[(currentSegment % bufferSize) * packetSize + j] = temp[j + 16];
            }

            blocksReady[currentSegment % bufferSize].store(true);

            if (!outUploaderStarted && currentSegment == floor(3*bufferSize/4)) {
                //shedule uploader
                if (debug) mainLogger.log() << "schedule out uploader" << std::endl;

                outUploaderStarted = true;

                std::thread([packetBuffer, packetSize, blocksReady, &mainSession,
                             currentMainSession, bufferSize, &currentlyWritingPacket, &outUploaderLogger]

                            { makeUpload(packetBuffer, packetSize, blocksReady, mainSession,
                                    currentMainSession, bufferSize, currentlyWritingPacket, outUploaderLogger); }

                            ).detach();
            }
        }

        //unlink from broadcast group
        if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
            syserr("setsockopt");

        close(sock);

    }

//    close(sock);
    exit(EXIT_SUCCESS);
}