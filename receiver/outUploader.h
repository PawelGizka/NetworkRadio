#ifndef ODBIORNIK_OUTUPLOADER_H
#define ODBIORNIK_OUTUPLOADER_H

#include <atomic>
#include "../common/logger.h"

int makeUpload(const char *packetBuffer, int psize, std::atomic<bool> *blocksReady,
               std::atomic<int> &mainSession, int mySession, uint64_t bufferSize,
               std::atomic<uint64_t > &currentlyWritingPacket, logger &logger);

#endif //ODBIORNIK_OUTUPLOADER_H
