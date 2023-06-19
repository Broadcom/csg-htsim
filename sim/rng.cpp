#include <cstdlib>
#include <climits>
#include <random>

using namespace std;

static mt19937 random_engine;

void srand(unsigned seed)
{
    random_engine = mt19937(seed);
}

int rand()
{
    return random_engine() & INT_MAX;
}

void srandom(unsigned seed)
{
    srand(seed);
}

long random()
{
    return rand();
}
