#ifndef __VectorUnorderedMap_H__
#define __VectorUnorderedMap_H__


#include <functional>
#include <vector>


template <typename T>

inline void hash_combine(std::size_t &seed, const T &val) {
    seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// auxiliary generic functions to create a hash value using a seed
template <typename T> inline void hash_val(std::size_t &seed, const T &val) {
    hash_combine(seed, val);
}


template <typename T, typename... Types>

inline void hash_val(std::size_t &seed, const T &val, const Types &... args) {
    hash_combine(seed, val);
    hash_val(seed, args...);
}


template <typename... Types>

inline std::size_t hash_val(const Types &... args) {
    std::size_t seed = 0;
    hash_val(seed, args...);
    return seed;
}


struct vector_hash {
    template <typename T>
    std::size_t operator()(const std::vector<T>& v) const {
        std::size_t seed = v.size();
        for (const auto& element : v) {
            seed ^= std::hash<T>()(element) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};


#endif