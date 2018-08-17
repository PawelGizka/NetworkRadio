//
// Created by pawel on 14.06.18.
//

#ifndef RETRANSMIT_H
#define RETRANSMIT_H

#include <atomic>
#include "../common/helper.h"
#include "../common/logger.h"

void retransmit(std::atomic<int> &mainSession, int mySession, std::atomic<bool> *blocksReady,
                int from, int to, struct RadioStation station, int packetSize, int rtime, uint64_t byte0,
                uint64_t bufferSize, logger &logger);

#endif //ODBIORNIK_RETRANSMIT_H
