#ifndef SIK_RADIO_LOGGER_H
#define SIK_RADIO_LOGGER_H

#include <string>
#include <fstream>
#include <ostream>

class logger {
public:
    explicit logger(const std::string &fileName);

    std::ofstream& log();

private:
    std::ofstream fileToLog;
    long lastWritten;

};

#endif //SIK_RADIO_LOGGER_H
