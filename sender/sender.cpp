#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#include <set>
#include <mutex>
#include <atomic>
#include <cstring>

#include "../common/err.h"
#include "../common/helper.h"
#include "../common/logger.h"
#include "../common/debug.h"

const std::string LOOKUP("ZERO_SEVEN_COME_IN\n");
const std::string REXMIT("LOUDER_PLEASE");

void listenForControlData(
        std::atomic<bool> &end, std::string mcast_addr, uint16_t controlPort, uint32_t dataPort, std::string nazwa_stacji,
        std::set<uint64_t > *packetsToRetransmit, std::mutex &mutex) {

    logger logger("sender/listenForControlData");

    int sock;
    struct sockaddr_in local_address;
    struct sockaddr_in client_address;
    socklen_t rcva_len;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    // non blocking
    int on = 1;
    ioctl(sock, FIONBIO, &on);

    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(controlPort);
    if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
        syserr("bind");

    while (!end.load()) {
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
                if (debug) logger.log() << "bląd" << strerror(errno) << std::endl;
                break;
            }
        } else if (bytes == 0) {
            if (debug) logger.log() << "Zakończono połączenie" << std::endl;
            break;
        }

        std::string str(buffer);
        if (debug) logger.log() << "odebrano" << str << std::endl;

        if (str == LOOKUP) {
            int length = snprintf (buffer, sizeof(buffer), "BOREWICZ_HERE %s %d %s\n",  mcast_addr.c_str(), dataPort, nazwa_stacji.c_str());
            std::string out(buffer);
            if (debug) logger.log() << "odebrano lookup" << out << std::endl;
            flags = 0;
            sendto(sock, buffer, (size_t) length, flags,
                   (struct sockaddr *) &client_address, sizeof(client_address));

        } else if (contains(str, REXMIT)) {
            std::set<uint64_t > toRetransmit = parsePacketNumbersFromRexmit(str);
            mutex.lock();
            packetsToRetransmit->insert(toRetransmit.begin(), toRetransmit.end());
            mutex.unlock();

            if (debug) {
                std::ofstream& out = logger.log();
                out << "mutexes to retransmit: " << std::endl;
                for (auto i : toRetransmit) {
                    out << i  << std::endl;
                }
            }

        }

    }

    close(sock);
    exit(EXIT_SUCCESS);
}

void retransmit(std::atomic<bool> &end, const std::string &mcast_addr,const int data_port,const int rtime,
                std::set<uint64_t > *packetsToRetransmit, std::mutex &mutex,
                std::atomic<uint64_t> &currentPacket, uint64_t fifoSize, const char *fifo, std::mutex fifoMutexes[],
                const int psize, uint64_t sessionId) {

    logger logger("sender/retransmit");

    int sock, optval;
    struct sockaddr_in remote_address;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) syserr("socket");

    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(data_port);
    if (inet_aton(mcast_addr.c_str(), &remote_address.sin_addr) == 0)
        syserr("inet_aton");
    if (connect(sock, (struct sockaddr *)&remote_address, sizeof remote_address) < 0)
        syserr("connect");

    char buffer[psize + 16];

    while (!end.load()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(std::chrono::milliseconds(rtime));

        mutex.lock();
        std::set<uint64_t> toRetransmit = *packetsToRetransmit;
        packetsToRetransmit->clear();
        mutex.unlock();

        for (auto packet : toRetransmit) {
            uint64_t packetNumber = packet / psize;
            if (currentPacket - fifoSize + 1 <= packetNumber) {
                if (fifoMutexes[packetNumber % fifoSize].try_lock()) {
                    for (int i = 0; i < psize; i++) {
                        buffer[i + 16] = fifo[(packetNumber % fifoSize) + i];
                    }
                    fifoMutexes[packetNumber % fifoSize].unlock();

                    *((uint64_t *) buffer) = htobe64(sessionId);
                    *((uint64_t *) (buffer + 8)) = htobe64(packet);

                    write(sock, buffer, psize + 16);

                    if (debug) logger.log() << "packet " << packet << " resend" << std::endl;
                } else {
                    //we are little late
                    if (debug) logger.log() << "packet " << packet << " little late to retransmit" << std::endl;
                }
            } else {
                //packet is to old
                if (debug) logger.log() << "packet " << packet << " to old to retransmit" << std::endl;
            }

        }

    }

    close(sock);
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {

    //-a
    std::string multicastAddress("239.0.0.1");

    //-P
    int dataPort = 20000;

    //-C, used for control packet transmission
    int ctrlPort = 30000;

    //-p
    int packetSize = 512;

    //-f
    int fifoQueueSize = 128000;

    //-R, time (in miliseconds) between packets retransmission
    int retransmitTime = 250;

    //-n
    std::string radioName("No name sender");

    InputParser input(argc, argv);
    if(!input.cmdOptionExists("-a")){
        std::cout << "no required multicast address parameter -1" << std::endl;
        return 1;
    }

    std::string option = input.getCmdOption("-a");
    if (!option.empty()){
        multicastAddress = option;
    }

    option = input.getCmdOption("-P");
    if (!option.empty()){
        dataPort = std::stoi(option);
    }

    option = input.getCmdOption("-C");
    if (!option.empty()){
        ctrlPort = std::stoi(option);
    }

    option = input.getCmdOption("-p");
    if (!option.empty()){
        packetSize = std::stoi(option);
    }

    option = input.getCmdOption("-f");
    if (!option.empty()){
        fifoQueueSize = std::stoi(option);
    }

    option = input.getCmdOption("-R");
    if (!option.empty()){
        retransmitTime = std::stoi(option);
    }

    option = input.getCmdOption("-n");
    if (!option.empty()){
        radioName = option;
    }

    int sock, optval;
    struct sockaddr_in remote_address;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) syserr("socket");

    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(static_cast<uint16_t>(dataPort));
    if (inet_aton(multicastAddress.c_str(), &remote_address.sin_addr) == 0)
        syserr("inet_aton");
    if (connect(sock, (struct sockaddr *)&remote_address, sizeof remote_address) < 0)
        syserr("connect");

    std::atomic<bool> endOfProgram(false);
    auto *packetsToRetransmit = new std::set<uint64_t>;
    std::mutex packetsToRetransmitMutex;

    auto fifoSize = static_cast<uint64_t>(floor(fifoQueueSize / packetSize));
    auto *fifo = new char[fifoSize * packetSize];
    std::atomic<uint64_t> currentPacket(0);

    auto *fifoMutexes = new std::mutex[fifoSize];

    time_t time_buffer;
    time(&time_buffer);

    uint64_t sessionId = time_buffer;

    std::thread listener{[&endOfProgram, multicastAddress, ctrlPort, dataPort, radioName, packetsToRetransmit, &packetsToRetransmitMutex]
                         { listenForControlData(endOfProgram, multicastAddress, ctrlPort, dataPort,
                                                radioName, packetsToRetransmit, packetsToRetransmitMutex); }};

    std::thread retrasmitter{[&endOfProgram, multicastAddress, dataPort, retransmitTime, packetsToRetransmit, &packetsToRetransmitMutex,
                                     &currentPacket, fifoSize, fifo, fifoMutexes, packetSize, sessionId]
                             {retransmit(endOfProgram, multicastAddress, dataPort, retransmitTime, packetsToRetransmit, packetsToRetransmitMutex,
                                         currentPacket, fifoSize, fifo, fifoMutexes, packetSize, sessionId); }};

    logger logger("sender/main");

    char temp[packetSize + 16];

    while (!std::cin.eof()) {
        std::cin.read((temp + 16), packetSize);
        *((uint64_t *) temp) = htobe64(sessionId);
        *((uint64_t *) (temp + 8)) = htobe64(currentPacket * packetSize);

        write(sock, temp, packetSize + 16);

        fifoMutexes[currentPacket % fifoSize].lock();
        for (int j = 0; j < packetSize; j++) {
            fifo[((currentPacket % fifoSize) * packetSize) + j] = temp[j + 16];
        }
        fifoMutexes[currentPacket % fifoSize].unlock();

        if (debug) logger.log() << "send packet: " << (currentPacket * packetSize) << std::endl;

        currentPacket++;
    }

    endOfProgram.store(true);

    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}