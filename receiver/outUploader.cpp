#include <atomic>
#include <iostream>
#include <fstream>
#include <unistd.h>

#include "../common/logger.h"
#include "../common/debug.h"

int makeUpload(const char *packetBuffer, int psize, std::atomic<bool> *blocksReady,
               std::atomic<int> &mainSession, int mySession, uint64_t bufferSize,
               std::atomic<uint64_t > &currentlyWritingPacket, logger &logger) {

    if (debug) logger.log() << "Cout Uploader started" << std::endl;

    while (mainSession.load() == mySession) {
        uint64_t currentPacket = currentlyWritingPacket.load();
        if (!blocksReady[currentPacket % bufferSize].load()) {
            mainSession++;
            if (debug) logger.log() << "Lack of packet to cout, break" << std::endl;
            break;
        }
        blocksReady[currentPacket % bufferSize].store(false);
        write(1, (packetBuffer + ((currentPacket % bufferSize)*psize)), psize);

        currentlyWritingPacket++;
        if (debug) logger.log() << "packet: " << currentPacket << " writen to cout" << std::endl;
    }

    if (debug) logger.log() << "session ended, stopping cout uploader" << std::endl;

}
