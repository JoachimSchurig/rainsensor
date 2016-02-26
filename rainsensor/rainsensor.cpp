/*
 
 rainsensor.cpp
 
 using the CppGPIO library: https://github.com/JoachimSchurig/CppGPIO/
 
 compile:
 
 g++ -std=gnu++14 -pthread -o rainsensor rainsensor.cpp -lcppgpio
 
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



void count_rain(const GPIO::Counter& counter, const std::string& filename, bool print_to_console, int interval = 5)
{
    typedef std::vector<unsigned long> bucket_vec_t;
    bucket_vec_t buckets;
    // assign zeroes to all buckets and make them index accessible
    buckets.resize(60 / interval);
    bucket_vec_t::iterator inserter = buckets.begin();
    
    // init with current counter value (probably 0)
    unsigned long last_event_counter = counter.get_count();

    while (true) {
        
        // sleep some minutes
        std::this_thread::sleep_for(std::chrono::seconds(interval*60));

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
        
        double mm_per_hour = events_per_hour * 127.455166 * 5 / 1000;
        
        if (!filename.empty()) {
            
            std::ofstream out;
            out.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);

            if (!out.is_open()) {
                std::cerr << "Cannot open file " << filename << std::endl;
                exit(1);
            }

            out << std::setprecision(2) << std::fixed << mm_per_hour << std::endl;

            out.close();

        }
        
        if (print_to_console) {
            
            std::cout << std::setprecision(2) << std::fixed << mm_per_hour << " mm/m2" << std::endl;
            
        }
        
    }
}


int main(int argc, char *argv[])
{
    // analyze options
    std::string filename;
    bool print_to_console = false;
    int interval = 5;
    int gpio_pin = 0;
    
    {
        int opt;
        
        while ((opt = getopt(argc, argv, "c:f:hi:p")) != -1) {
            switch (opt) {
                case 'c':
                    gpio_pin = atoi(optarg);
                    break;
                case 'f':
                    filename = optarg;
                    break;
                default:
                case 'h':
                    std::cout << argv[0] << " - help:" << std::endl;
                    std::cout << std::endl;
                    std::cout << " -c N     : select gpio pin to use (default 0)" << std::endl;
                    std::cout << " -f file  : file to write hourly rainfall into (default none)" << std::endl;
                    std::cout << " -i N     : interval in minutes between updates (1..60, default 5)" << std::endl;
                    std::cout << " -p       : print updates to stdout too (default off)" << std::endl;
                    std::cout << std::endl;
                    exit(0);
                case 'i':
                    interval = atoi(optarg);
                    if (interval < 1 || interval > 60) {
                        std::cerr << "invalid value for interval (1..60): " << interval << std::endl;
                        exit(1);
                    }
                    break;
                case 'p':
                    print_to_console = true;
                    break;
            }
        }
    }

    // setup the Counter object
    GPIO::Counter counter(gpio_pin, GPIO::GPIO_PULL::UP, std::chrono::milliseconds(500), std::chrono::milliseconds(5));
    counter.start();
    
    // and run the endless loop to capture the rain counter
    count_rain(counter, filename, print_to_console, interval);
    
    return 0;
}
