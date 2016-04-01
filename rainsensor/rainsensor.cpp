/*
 
 rainsensor.cpp
 
 using the CppGPIO library: https://github.com/JoachimSchurig/CppGPIO/
 
 compile:
 
 g++ -std=gnu++11 -pthread -o rainsensor rainsensor.cpp -lcppgpio
 
 run:
 
 sudo ./rainsensor
 
 */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>

#include <cppgpio.hpp>


// keep the startup options in a struct

struct option_t {
    std::string filename;
    bool print_to_console = false;
    int interval = 5;
    int gpio_pin = 0;
    int milliliter = 5;
    int sqcm = 127; // exact value of default device is 127.455166;
};


// the main loop for the rain sensor runs forever

void count_rain(const option_t& options)
{
    // setup the Counter object with very conservative values for the debouncing
    GPIO::Counter counter(options.gpio_pin, GPIO::GPIO_PULL::UP, std::chrono::milliseconds(500), std::chrono::milliseconds(5));
    // and start counting
    counter.start();

    typedef std::vector<unsigned long> bucket_vec_t;
    bucket_vec_t buckets;
    // assign zeroes to all buckets and make them index accessible
    buckets.resize(60 / options.interval);
    bucket_vec_t::iterator inserter = buckets.begin();
    
    // init with current counter value (probably 0)
    unsigned long last_event_counter = counter.get_count();

    while (true) {
        
        // sleep some minutes
        std::this_thread::sleep_for(std::chrono::seconds(options.interval * 60));

        // get new counter value
        unsigned long new_event_counter = counter.get_count();
        
        // did we have an overflow? then start counting again at 0
        // (this is a simplified solution, we could also add the amount of the max
        // data type minus the last counter to the new counter value, which would get us
        // the true event count. But this happens every some years of uninterrupted
        // runtime, so why bother)
        if (new_event_counter < last_event_counter) last_event_counter = 0;
        
        // calculate number of new events during this interval
        unsigned long events = new_event_counter - last_event_counter;
        
        // and store the new counter value for the next round
        last_event_counter = new_event_counter;
        
        // store it in the bucket vector
        *inserter++ = events;
        
        // check if the bucket vector is full and we have to start at the begin
        if (inserter == buckets.end()) inserter = buckets.begin();
        
        unsigned long events_per_hour = 0;
        for (bucket_vec_t::iterator it = buckets.begin(); it != buckets.end(); ++it) {
            events_per_hour += *it;
        }
        
        double mm_per_hour = events_per_hour * options.sqcm * options.milliliter / 1000;
        
        if (!options.filename.empty()) {
            
            std::ofstream out;
            out.open(options.filename.c_str(), std::ofstream::out | std::ofstream::trunc);

            if (!out.is_open()) {
                std::cerr << "Cannot open file " << options.filename << std::endl;
                exit(1);
            }

            out << std::setprecision(2) << std::fixed << mm_per_hour << std::endl;

            out.close();

        }
        
        if (options.print_to_console) {
            
            std::cout << std::setprecision(2) << std::fixed << mm_per_hour << " mm/m2" << std::endl;
            
        }
        
    }
}


// read options and start main loop

int main(int argc, char *argv[])
{
    // analyze options
    option_t options;

    {
        int opt;
        
        while ((opt = getopt(argc, argv, "b:c:f:hi:p:s:")) != -1) {
            switch (opt) {
                case 'b':
                    options.milliliter = atoi(optarg);
                    if (options.milliliter < 1 || options.milliliter > 1000) {
                        std::cerr << "invalid value for milliliter (1..1000): " << options.milliliter << std::endl;
                        exit(1);
                    }
                    break;
                case 'c':
                    options.gpio_pin = atoi(optarg);
                    if (options.gpio_pin < 0 || options.gpio_pin > 63) {
                        std::cerr << "invalid value for gpio (0..63): " << options.gpio_pin << std::endl;
                        exit(1);
                    }
                    break;
                case 'f':
                    options.filename = optarg;
                    break;
                default:
                case 'h':
                    std::cout << argv[0] << " - help:" << std::endl;
                    std::cout << std::endl;
                    std::cout << " -b N     : milliliter per bucket count (default 5)" << std::endl;
                    std::cout << " -c N     : select gpio to use (default 0)" << std::endl;
                    std::cout << " -f file  : file to write hourly rainfall into (default none)" << std::endl;
                    std::cout << " -i N     : interval in minutes between updates (1..60, default 5)" << std::endl;
                    std::cout << " -p       : print updates to stdout too (default off)" << std::endl;
                    std::cout << " -s N     : collector extension in square centimeters (default 127)" << std::endl;
                    std::cout << std::endl;
                    exit(0);
                case 'i':
                    options.interval = atoi(optarg);
                    if (options.interval < 1 || options.interval > 60) {
                        std::cerr << "invalid value for interval (1..60): " << options.interval << std::endl;
                        exit(1);
                    }
                    break;
                case 'p':
                    options.print_to_console = true;
                    break;
                case 's':
                    options.sqcm = atoi(optarg);
                    if (options.sqcm < 1 || options.sqcm > 10000) {
                        std::cerr << "invalid value for square centimeters (1..10000): " << options.sqcm << std::endl;
                        exit(1);
                    }
                    break;
            }
        }
    }

    // run the endless loop to capture the rain counter
    count_rain(options);
    
    return 0;
}
