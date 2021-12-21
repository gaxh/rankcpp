#include "zeeset.h"
#include <iostream>

struct SortData {
    int x;
    int y;

    bool operator==(const SortData &rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    bool operator<(const SortData &rhs) const {
        if( x < rhs.x ) {return true;}
        else if(x > rhs.x) {return false;}
        else {return y < rhs.y;}
    }
};

std::ostream &operator<<(std::ostream &os, const SortData &ins) {
    os << "(" << ins.x << "," << ins.y << ")";
    return os;
}

int main() {
    ZeeSet<unsigned, SortData, 32, 30> rank;
    std::mt19937 rng;
    rng.seed(time(NULL));

    unsigned max_id = 100000;
    unsigned max_op = 1000000;

    for(unsigned i = 0; i < max_op; ++i) {
        unsigned op = (unsigned)rng() % 10;
        unsigned id = (unsigned)rng() % max_id;

        switch(op) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
                {
                    SortData s;
                    rank.Update(id, s);
                }
                break;
            default:
                {
                    rank.Delete(id);
                }
                break;
        }
    }

    rank.Clear();

    //std::cout << rank.DumpLevels() << "\n";
    std::cout << "TestSelf=" << rank.TestSelf() << "\n";

    for(unsigned i = 0; i < max_op; ++i) {
        unsigned op = (unsigned)rng() % 10;
        unsigned id = (unsigned)rng() % max_id;

        switch(op) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 6:
                {
                    SortData s;
                    rank.Update(id, s);
                }
                break;
            default:
                {
                    rank.Delete(id);
                }
                break;
        }
    }

    //std::cout << rank.DumpLevels() << "\n";
    std::cout << "TestSelf=" << rank.TestSelf() << "\n";
    
    return 0;
}

