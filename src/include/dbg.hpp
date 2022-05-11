#ifndef __dbg_h__
#define __dbg_h__

#include <string>

void EnableDbg(std::string name);
void DisableDbg(std::string name);
bool DbgEnabled(std::string name);

#endif
