#ifndef __utils_h__
#define __utils_h__

#include <functional>
#include <map>
#include <string>
#include <vector>

template <typename... Args>
std::string StrFormat(const std::string &format, Args... args) {
    size_t size =
        1 + snprintf(nullptr, 0, format.c_str(), args...); // Extra space for \0
    // unique_ptr<char[]> buf(new char[size]);
    char bytes[size];
    snprintf(bytes, size, format.c_str(), args...);
    return std::string(bytes);
}

template <typename T>
void foreach (T &iterable, std::function<void(typename T::iterator)> each,
              std::function<void(typename T::iterator)> separation) {
    auto it = iterable.begin();
    auto itend = iterable.end();
    if (it != itend) {
        auto last = --itend;
        for (; it != last; it++) {
            each(it);
            separation(it);
        }
        each(it);
    }
}

#define FOREACH_LAMBDA() [&](auto it)

template <typename T>
void foreach (T &iterable, std::function<void(typename T::iterator, int)> each,
              std::function<void(typename T::iterator, int)> separation) {
    auto itbegin = iterable.begin(), it = itbegin;
    auto itend = iterable.end();
    if (it != itend) {
        auto last = --itend;
        for (; it != last; it++) {
            each(it, it - itbegin);
            separation(it, it - itbegin);
        }
        each(it, it - itbegin);
    }
}

#define FOREACH_IDX_LAMBDA() [&](auto it, int i)

template <typename KT, typename VT>
struct mapped_vector : public std::vector<VT> {
  private:
    std::map<KT, VT> q;

  public:
    mapped_vector() {}

    mapped_vector(std::initializer_list<std::pair<KT, VT>> init) {
        for (auto [k, v] : init) {
            insert(k, v);
        }
    }

    std::pair<typename std::map<KT, VT>::iterator, bool> insert(const KT k,
                                                                const VT v) {
        auto [it, result] = q.insert({k, v});
        if (result)
            std::vector<VT>::push_back(v);
        return {it, result};
    }

    VT *find(const KT k) {
        auto it = q.find(k);
        if (it == q.end())
            return nullptr;
        else
            return &it->second;
    }

    inline std::map<KT, VT> &getmap() { return q; }

    // VT &operator[](const KT k) { return q[k]; }
};

#endif