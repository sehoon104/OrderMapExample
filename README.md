# FlatHashMap Benchmark (C++20)

This project benchmarks **`std::unordered_map`** versus a custom **open-addressing `FlatHashMap`** optimized for low-latency trading systems and cache efficiency.

The benchmark simulates a simplified **order map** — mapping `uint64_t` order IDs to lightweight `Order` structs — and measures per-operation latency and throughput across insert, find, and erase operations.

---

## Reason for Implementation

`std::unordered_map` provides good average performance, but:
- It performs **dynamic memory allocations** per node.
- It suffers from **poor cache locality** due to pointer chasing.
- **Rehash events** introduce unpredictable latency spikes.

In high-frequency trading (HFT) and other latency-sensitive domains, those spikes are unacceptable.  
This project demonstrates how a **flat, fixed-capacity hash map** can deliver much tighter latency distribution by trading flexibility for predictability.

---

## Implementation Details

### `FlatHashMap`
- **Open addressing** with **linear probing**.
- **Fixed power-of-two capacity** — no rehashing during runtime.
- **Contiguous memory layout** for improved cache locality.
- **O(1)** average insert/find/erase with bounded probe length.
- No dynamic allocation or exceptions in hot paths.

### `Order` Structure
```cpp
struct Order {
    std::uint64_t orderID;
    double price;
    enum class Side : bool { Buy = 0, Sell = 1 } side;
    std::uint32_t qty;
};
