#include <atomic>
#include <chrono>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <cinttypes>

#include "../common/helper.h"
#include "../common/err.h"
#include "../common/logger.h"
#include "../common/debug.h"
#include "radioStation.h"

void retransmit(std::atomic<int> &mainSession, int mySession, std::atomic<bool> *blocksReady,
                int from, int to, struct RadioStation station, int packetSize, int rtime, uint64_t byte0,
                uint64_t bufferSize, logger &logger) {

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(std::chrono::milliseconds(rtime));

    if (debug) logger.log() << "Retransmitter started" << std::endl;

    if (mainSession != mySession) {
        return;
    }

    int sock;
    struct sockaddr_in remote_address;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) syserr("socket");

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = station.sin_port;
    remote_address.sin_addr = station.sin_addr;

    if (connect(sock, (struct sockaddr *)&remote_address, sizeof remote_address) < 0)
        syserr("connect");

    int count = 0;
    std::string toSend("LOUDER_PLEASE ");
    for (int i = from; i <= to; i++) {
        if (!blocksReady[i % bufferSize].load()) {//check if its still not ready

            if (debug) logger.log() << "sending request for packet: " << i*packetSize + byte0 << std::endl;

            if (count != 0) {
                toSend.append(",");
            }

            char buffer[1024];
            snprintf (buffer, sizeof(buffer), "%" PRIu64 ,  i*packetSize + byte0);
            std::string parsed(buffer);
            toSend.append(parsed);

            count++;
        } else {
            if (debug) logger.log() << "packet: " << i*packetSize + byte0 << " is ready, not sending request" << std::endl;
        }
    }

    if (count > 0) {
        toSend.append("\n");
        write(sock, toSend.c_str(), toSend.length());

        std::thread([&mainSession, mySession, blocksReady, from, to, station, packetSize, rtime, byte0, bufferSize, &logger]
                    { retransmit(mainSession, mySession, blocksReady, from, to, station, packetSize, rtime, byte0, bufferSize, logger); }).detach();
    }

    close(sock);

}
