#include <cstdlib>
#include <random>

using namespace std;

static mt19937 random_engine;

void srand(unsigned seed)
{
    random_engine = mt19937(seed);
}

int rand()
{
    return random_engine();
}
