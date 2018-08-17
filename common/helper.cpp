#include <string>
#include <vector>
#include <sstream>
#include <set>
#include <algorithm>

#include "helper.h"

std::string toString(char array[]) {
    std::string toReturn(array);
    return toReturn;
}

bool contains(const std::string &str, const std::string &substr) {
    return str.find(substr) != std::string::npos;
}

std::set<uint64_t > parsePacketNumbersFromRexmit(const std::string &str) {
    std::set<uint64_t > vect;

    std::stringstream ss(str.substr(14, std::string::npos));

    int i;

    while (ss >> i) {
        vect.emplace(i);

        if (ss.peek() == ',')
            ss.ignore();
    }

    return vect;
}

InputParser::InputParser(int &argc, char **argv) {
    for (int i=1; i < argc; ++i) {
        this->tokens.emplace_back(argv[i]);
    }
}

const std::string &InputParser::getCmdOption(const std::string &option) const {
    std::vector<std::string>::const_iterator itr;

    itr =  std::find(this->tokens.begin(), this->tokens.end(), option);

    if (itr != this->tokens.end() && ++itr != this->tokens.end()){
        return *itr;
    }

    static const std::string empty_string;
    return empty_string;
}

bool InputParser::cmdOptionExists(const std::string &option) const {
    return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
}
