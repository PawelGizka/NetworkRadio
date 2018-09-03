#ifndef OUTUPLOADER_H
#define OUTUPLOADER_H

#include <atomic>
#include "../common/logger.h"

void makeUpload(const char *packetBuffer, int psize, std::atomic<bool> *blocksReady,
               std::atomic<int> &mainSession, int mySession, uint64_t bufferSize,
               std::atomic<uint64_t > &currentlyWritingPacket, logger &logger);

#endif
