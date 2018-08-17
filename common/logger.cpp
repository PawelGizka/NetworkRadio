#include <chrono>
#include "logger.h"
#include "debug.h"

std::ofstream& logger::log() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    long miliseconds = std::chrono::system_clock::now().time_since_epoch().count();

    long diff = 0;
    if (lastWritten != -1) {
        diff = miliseconds - lastWritten;
    }

    lastWritten = miliseconds;

    fileToLog << ltm->tm_hour << ":" << ltm->tm_min << ":"
                << ltm->tm_sec << "-" << (miliseconds % 1000000000) << " diff: " << diff << " ";

    return fileToLog;
}

logger::logger(const std::string &fileName)  {
    if (debug) {
        lastWritten = -1;
        fileToLog.open(fileName);
    }
}


