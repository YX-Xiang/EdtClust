#include <chrono>
#include <string>
#include <iostream>


class Config {
    public:
        static int n_threads;
};


class Timer {
    public:
        static std::chrono::high_resolution_clock::time_point set_start();
        static void time_profile(const std::string&, const std::chrono::high_resolution_clock::time_point&);
};


