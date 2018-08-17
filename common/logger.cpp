#include <chrono>
#include "logger.h"
#include "debug.h"

std::ofstream& logger::log() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    auto miliseconds = std::chrono::system_clock::now().time_since_epoch();

    fileToLog << ltm->tm_hour << ":" << ltm->tm_min << ":"
                << ltm->tm_sec << "-" << (miliseconds.count() % 1000000000) << " ";

    return fileToLog;
}

logger::logger(const std::string &fileName)  {
    if (debug) fileToLog.open(fileName);
}


