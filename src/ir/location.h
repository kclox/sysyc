#ifndef __location_h__
#define __location_h__

#include <string>

class Location {
  public:
    std::string filename;
    int line;

    Location() { line = 1; }

    operator std::string() {
        return filename + ":" + std::to_string(line) + ":";
    }

    std::string str() { return this->operator std::string(); }
};

#endif
