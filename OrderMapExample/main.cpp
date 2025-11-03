#include "OrderMap.h"

static constexpr uint64_t benchmarkseed1 = 0x12345678abcdef01ULL;
static constexpr uint64_t benchmarkseed2 = 0x0fedcba987654321ULL;

// ===============================================================
//                           main()
// ===============================================================
int main() {

	//This size will be rounded to closest power of two
    constexpr std::size_t mapSize = 200'000;

    run_benchmark<std::unordered_map<std::uint64_t, Order>>(
        "std::unordered_map", mapSize, benchmarkseed1);
    run_benchmark<FlatHashMap>(
        "FlatHashMap", mapSize, benchmarkseed2);

    return 0;
}