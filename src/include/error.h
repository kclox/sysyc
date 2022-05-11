#ifndef __error_h__
#define __error_h__

#include <exception>
#include <iostream>
#include <string>

struct SymbolNotDefine : public std::exception {
    std::string sym;
    char msg[0x100];
    SymbolNotDefine(std::string sym) {
        this->sym = sym;
        snprintf(msg, 0x100 - 1, "symbol ‘%s’ is not defined", sym.c_str());
    }

    virtual const char *what() const throw() { return msg; }
};

struct BreakContiException : public std::exception {
    std::string type;
    char msg[0x100];
    BreakContiException(std::string type) : type(type) {
        snprintf(msg, 0x100 - 1, "%s can only be used in loop", type.c_str());
    }

    virtual const char *what() const throw() { return msg; }
};

struct SyntaxError : public std::exception {
    char msg[0x100];
    SyntaxError(std::string msg) {
        snprintf(this->msg, 0x100 - 1, "[Syntex Error] %s", msg.c_str());
    }

    virtual const char *what() const throw() { return msg; }
};

#endif