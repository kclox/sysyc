#ifndef __type_h__
#define __type_h__

#include <map>
#include <string>
#include <vector>

template <typename T> struct List : public std::vector<T> {
    List() {}

    List(T item) { this->push_back(item); }

    List *Append(T item) {
        this->push_back(item);
        return this;
    }

    List *Append(List *list) {
        for (auto x : *list) {
            this->push_back(x);
        }
        return this;
    }
};

template <typename T> struct Scope : std::map<std::string, T> {
    Scope *parent;

    Scope() : parent(nullptr) {}

    Scope(Scope<T> *parent) : parent(parent) {}

    bool Insert(T x) {
        auto [_, r] = this->insert({x->name, x});
        return r;
    }

    bool Insert(List<T> *list) {
        bool res = true;
        for (auto x : *list) {
            auto [_, r] = this->insert({x->name, x});
            res &= r;
        }
        return res;
    }

    T Find(std::string name) {
        for (auto p = this; p != nullptr; p = p->parent) {
            auto it = p->find(name);
            if (it != p->end()) {
                return it->second;
            }
        }
        return nullptr;
    }
};

#endif