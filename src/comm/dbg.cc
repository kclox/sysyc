#include "dbg.hpp"
#include <map>

std::map<std::string, bool> dbg_map;

void EnableDbg(std::string name) { dbg_map[name] = true; }

void DisableDbg(std::string name) { dbg_map[name] = false; }

bool DbgEnabled(std::string name) { return dbg_map[name]; }