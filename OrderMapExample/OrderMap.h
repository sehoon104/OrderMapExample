#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <random>

static constexpr uint64_t XORSHIFT64STAR_MULT = 2685821657736338717ULL;

struct Order {
    std::uint64_t orderID;
    double price;
    enum class Side : bool { Buy = 0, Sell = 1 } side;
    std::uint32_t qty;
};

//Ordermap implementation for faster performance
class FlatHashMap {
public:
    using Key = std::uint64_t;
    using Val = Order;

    explicit FlatHashMap(std::size_t capacity_pow2)
        : cap_(capacity_pow2), mask_(capacity_pow2 - 1), size_(0) {

		//capacity must be power of two in order to use for bitmasking instead of modulo
        if (cap_ == 0 || (cap_ & (cap_ - 1)) != 0) {
            throw std::invalid_argument("FlatHashMap capacity must be power of two");
		}

		//initialize entries
        entries_ = new Entry[cap_];
        for (std::size_t i = 0; i < cap_; ++i)
            entries_[i].key = EMPTY;
    }

    ~FlatHashMap() { delete[] entries_; }

	//Non-copyable
    FlatHashMap(const FlatHashMap&) = delete;
    FlatHashMap& operator=(const FlatHashMap&) = delete;

	//Non-movable
	FlatHashMap(FlatHashMap&&) = delete;
	FlatHashMap& operator=(FlatHashMap&&) = delete;

    bool upsert(Key k, const Val& v) noexcept {

        std::size_t idx = bucket(k);
        std::size_t probes = 0;
        for (;;) {
            Entry& entry = entries_[idx];
            if (entry.key == EMPTY || entry.key == TOMBSTONE) {
                entry.key = k;
                entry.val = v;
                ++size_;
                return true;
            }
            if (entry.key == k) {
                entry.val = v;
                return false;
            }
            idx = (idx + 1) & mask_;
			//Limit the number of probes to avoid infinite loops
            if (++probes > MAX_PROBES)
                return false;
        }
    }

    Val* find(Key k) noexcept {

        std::size_t idx = bucket(k);
        std::size_t probes = 0;
        for (;;) {
            Entry& entry = entries_[idx];
            if (entry.key == EMPTY)
                return nullptr;
            if (entry.key == k) return
                &entry.val;
            idx = (idx + 1) & mask_;
            //Limit the number of probes to avoid infinite loops
            if (++probes > MAX_PROBES) return nullptr;
        }
    }

    bool erase(Key k) noexcept {
        std::size_t idx = bucket(k);
        std::size_t probes = 0;
        for (;;) {
            Entry& entry = entries_[idx];
            if (entry.key == EMPTY)
                return false;
            if (entry.key == k) {
                entry.key = TOMBSTONE;
                --size_;
                return true;
            }
            idx = (idx + 1) & mask_;
            //Limit the number of probes to avoid infinite loops
            if (++probes > MAX_PROBES)
                return false;
        }
    }

    std::size_t size() const noexcept { return size_; }

private:
    struct Entry {
        Key key;
        Val val;
    };

    static constexpr Key EMPTY = 0ULL;
    static constexpr Key TOMBSTONE = 1ULL;
    static constexpr std::size_t MAX_PROBES = 128;
    static constexpr uint64_t SPLITMIX6430 = 0xbf58476d1ce4e5b9ULL;
    static constexpr uint64_t SPLITMIX6427 = 0x94d049bb133111ebULL;

	//SplitMix64 hash function
    static inline std::uint64_t hash64(std::uint64_t x) noexcept {
        x ^= x >> 30; x *= SPLITMIX6430;
        x ^= x >> 27; x *= SPLITMIX6427;
        x ^= x >> 31;
        return x;
    }

    inline std::size_t bucket(Key k) const noexcept {
        return static_cast<std::size_t>(hash64(k) & mask_);
    }

    Entry* entries_{ nullptr };
    std::size_t cap_{ 0 };
    std::size_t mask_{ 0 };
    std::size_t size_{ 0 };
};

//Util Functions
static inline std::size_t ceil_pow2(std::size_t x) {
    if (x <= 1) return 1;
    --x;
    for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1)
        x |= x >> i;
    return x + 1;
}

template <class Clock = std::chrono::steady_clock>
class Timer {
public:
    using tp = typename Clock::time_point;
    void reset() { t0_ = Clock::now(); }
    std::uint64_t ns() const {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0_).count());
    }
private:
    tp t0_{ Clock::now() };
};

static inline std::uint64_t xorshift64star(std::uint64_t& s) {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * XORSHIFT64STAR_MULT;
}

static volatile std::uint64_t sink64 = 0;

static void print_stats(const std::string& name,
    const std::vector<std::uint64_t>& samples_ns,
    std::uint64_t total_ns) {

    if (samples_ns.empty())
        return;

    std::vector<std::uint64_t> v = samples_ns;
    const auto n = v.size();

    auto nth = [&](double q) {
        std::size_t idx = static_cast<std::size_t>(q * (n - 1));
        std::nth_element(v.begin(), v.begin() + idx, v.end());
        return v[idx];
        };

    const std::uint64_t p50 = nth(0.50);
    const std::uint64_t p95 = nth(0.95);
    const std::uint64_t p99 = nth(0.99);
    const double avg = std::accumulate(samples_ns.begin(), samples_ns.end(), 0.0) / n;
    const double mops = (n * 1e3) / total_ns;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << name << ":\n";
    std::cout << "  Throughput: " << mops << " Mops/s\n";
    std::cout << "  Latency ns: 50th Percentile=" << p50 << ", 95th Percentile=" << p95
        << ", 99th Percentile=" << p99 << ", Avg=" << avg << "\n";
}

//Adapter for stl unordered_map and FlatHashMap
template <typename MapT>
struct MapAdapter;

// std::unordered_map
template <>
struct MapAdapter<std::unordered_map<std::uint64_t, Order>> {
    using Map = std::unordered_map<std::uint64_t, Order>;
    Map m;
    explicit MapAdapter(std::size_t expected) {
        m.rehash(ceil_pow2(static_cast<std::size_t>(expected * 1.6)));
    }
    bool upsert(std::uint64_t k, const Order& v) { m[k] = v; return true; }
    Order* find(std::uint64_t k) {
        auto it = m.find(k);
        return (it == m.end()) ? nullptr : &it->second;
    }
    bool erase(std::uint64_t k) { return m.erase(k) > 0; }
};

// FlatHashMap
template <>
struct MapAdapter<FlatHashMap> {
    FlatHashMap m;
    explicit MapAdapter(std::size_t expected)
        : m(ceil_pow2(static_cast<std::size_t>(expected * 1.25))) {
    }
    bool upsert(std::uint64_t k, const Order& v) { return m.upsert(k, v); }
    Order* find(std::uint64_t k) { return m.find(k); }
    bool erase(std::uint64_t k) { return m.erase(k); }
};

//Benchmarking function
template <typename Map>
static void run_benchmark(const std::string& label,
    std::size_t N_total,
    std::uint64_t seed_base) {
    std::cout << "\n=== Benchmark for: " << label << " ===\n";

	// Generate random keys
    std::vector<std::uint64_t> keys(N_total);
    std::iota(keys.begin(), keys.end(), 2ULL);
    std::uint64_t seed = seed_base;
    std::shuffle(keys.begin(), keys.end(), std::mt19937_64(seed));

    std::vector<std::uint64_t> find_keys = keys;
    std::shuffle(find_keys.begin(), find_keys.end(), std::mt19937_64(seed ^ 0xABCDEF));

    std::vector<std::uint64_t> erase_keys(find_keys.begin(),
        find_keys.begin() + N_total / 2);

    MapAdapter<Map> map(N_total);
    std::vector<std::uint64_t> lat_insert, lat_find, lat_erase;

	//Insert latencies
    Timer<> timer; timer.reset();
    for (std::size_t i = 0; i < N_total; ++i) {
		// Create an Order
        Order o{
            keys[i],
            100.0 + static_cast<double>(i % 50),
            (i % 2 == 0 ? Order::Side::Buy : Order::Side::Sell),
            static_cast<std::uint32_t>(10 + (i % 100))
        };
		// Measure insert latency
        auto t0 = std::chrono::steady_clock::now();
        map.upsert(o.orderID, o);
        auto t1 = std::chrono::steady_clock::now();
        lat_insert.push_back(
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
    }
    std::uint64_t t_insert = timer.ns();
    print_stats("insert (Order)", lat_insert, t_insert);

	//Finds latencies
    timer.reset();
    for (std::size_t i = 0; i < N_total; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        Order* p = map.find(find_keys[i]);
        auto t1 = std::chrono::steady_clock::now();
        if (p) sink64 ^= static_cast<std::uint64_t>(p->qty);
        lat_find.push_back(
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
    }
    std::uint64_t t_find = timer.ns();
    print_stats("find (Order)", lat_find, t_find);

    //Erase latencies
    timer.reset();
    for (auto k : erase_keys) {
        auto t0 = std::chrono::steady_clock::now();
        map.erase(k);
        auto t1 = std::chrono::steady_clock::now();
        lat_erase.push_back(
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
    }
    std::uint64_t t_erase = timer.ns();
    print_stats("erase (Order, N/2)", lat_erase, t_erase);
}