#include "zeeset.h"
#include <string>
#include <iostream>
#include <string.h>

int main() {
    ZeeSet<std::string, unsigned long, 32, 30> rank;
    std::mt19937 rng;
    rng.seed(time(NULL));

    unsigned max_id = 30;
    unsigned max_value = 100;

    for(unsigned i = 0; i < max_id; ++i) {
        static char buf[1024];
        snprintf(buf, sizeof(buf), "K%u", i);
        rank.Update(std::string(buf), rng() % max_value);
    }

    for(unsigned j = 0; j < max_id / 2; ++j) {
        unsigned rd = (unsigned)rng() % (unsigned)max_id;
        static char buf[1024];
        snprintf(buf, sizeof(buf), "K%u", rd);
        rank.Delete(std::string(buf));
    }

    std::cout << rank.DumpLevels() << "\n";

    for(int j = 0; j < 10; ++j) {
        unsigned rd = (unsigned)rng() % (unsigned)max_id;
        static char buf[1024];
        snprintf(buf, sizeof(buf), "K%u", rd);

        unsigned long r = rank.GetRankOfElement(std::string(buf));

        std::cout << "rank of " << std::string(buf) << "=" << r << "\n";
    }

    for(int j = 0; j < 10; ++j) {
        std::string key;
        unsigned long value;

        unsigned rd = (unsigned)rng() % (rank.Count() * 2);

        if( rank.GetElementByRank(rd, key, value) ) {
            std::cout << "rank " << rd << ": " << "[" << key << "]=" << value << "\n";
        } else {
            std::cout << "rank " << rd << ": " << "NONE" << "\n";
        }
    }

    rank.GetElementsByRangedRank(5, 10, [](unsigned long rank, const std::string &key, const unsigned long &value){
            std::cout << "ranged rank " << rank << ": " << "[" << key << "]=" << value << "\n";
            });

    rank.ForeachElements([](unsigned long rank, const std::string &key, const unsigned long &value){
            std::cout << "foreach rank " << rank << ": " << "[" << key << "]=" << value << "\n";
            });

    /*
    rank.DeleteByRangedRank(5, 6, [](unsigned long rank, const std::string &key, const unsigned long &value){
            std::cout << "delete_range rank " << rank << ": " << "[" << key << "]=" << value << "\n";
            });
    */

    std::cout << rank.DumpLevels() << "\n";
    std::cout << "rank count: " << rank.Count() << "\n";

    for(int k = 0; k < 10; ++k) {
        unsigned long rd_value = rng() % max_value;

        std::string key;
        unsigned long value;
        unsigned long rank_value;

        if( rank.GetElementOfFirstGreaterValue(rd_value, key, value, &rank_value) ) {
            std::cout << "first > " << rd_value << ": " << "[" << key << "]=" << value << " (rank:" << rank_value << ")";
        } else {
            std::cout << "first > " << rd_value << ": " << "NONE";
        }

        std::cout << " | ";

        if( rank.GetElementOfFirstGreaterEqualValue(rd_value, key, value, &rank_value) ) {
            std::cout << "first >= " << rd_value << ": " << "[" << key << "]=" << value << " (rank:" << rank_value << ")";
        } else {
            std::cout << "first >= " << rd_value << ": " << "NONE";
        }
        
        std::cout << " | ";

        if( rank.GetElementOfLastLessValue(rd_value, key, value, &rank_value) ) {
            std::cout << "last < " << rd_value << ": " << "[" << key << "]=" << value << " (rank:" << rank_value << ")";
        } else {
            std::cout << "last < " << rd_value << ": " << "NONE";
        }
        
        std::cout << " | ";

        if( rank.GetElementOfLastLessEqualValue(rd_value, key, value, &rank_value) ) {
            std::cout << "last <= " << rd_value << ": " << "[" << key << "]=" << value << " (rank:" << rank_value << ")";
        } else {
            std::cout << "last <= " << rd_value << ": " << "NONE";
        }

        std::cout << "\n";
    }

    {
        unsigned long rd_value_min = rng() % max_value;
        unsigned long rd_value_max = rng() % max_value;

        std::cout << "rd_value_min=" << rd_value_min << " " << "rd_value_max=" << rd_value_max << "\n";

        rank.GetElementsByRangedValue(rd_value_min, false, rd_value_max, false, [=](unsigned long rank, const std::string &key, const unsigned long &value) {
                std::cout << rd_value_min << "<v<" << rd_value_max << ": " << "(" << rank << ":" << key << ")" << " | ";
                });
        std::cout << "\n";

        rank.GetElementsByRangedValue(rd_value_min, true, rd_value_max, false, [=](unsigned long rank, const std::string &key, const unsigned long &value) {
                std::cout << rd_value_min << "<=v<" << rd_value_max << ": " << "(" << rank << ":" << key << ")" << " | ";
                });
        std::cout << "\n";

        rank.GetElementsByRangedValue(rd_value_min, false, rd_value_max, true, [=](unsigned long rank, const std::string &key, const unsigned long &value) {
                std::cout << rd_value_min << "<v<=" << rd_value_max << ": " << "(" << rank << ":" << key << ")" << " | ";
                });
        std::cout << "\n";

        rank.GetElementsByRangedValue(rd_value_min, true, rd_value_max, true, [=](unsigned long rank, const std::string &key, const unsigned long &value) {
                std::cout << rd_value_min << "<=v<=" << rd_value_max << ": " << "(" << rank << ":" << key << ")" << " | ";
                });
        std::cout << "\n";
    }

    return 0;
}

