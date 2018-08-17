//
// Created by pawel on 12.06.18.


//
#ifndef NADAJNIK_HELPER_H
#define NADAJNIK_HELPER_H

#include <string>
#include <set>
#include <vector>

std::string toString(char array[]);

bool contains(const std::string &str, const std::string &substr);
std::set<uint64_t > parsePacketNumbersFromRexmit(const std::string &str);

class InputParser{
private:
    std::vector<std::string> tokens;

public:
    InputParser (int &argc, char **argv);
    const std::string& getCmdOption(const std::string &option) const;
    bool cmdOptionExists(const std::string &option) const;
};

#endif //NADAJNIK_HELPER_H
