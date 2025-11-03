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
### Example Output
```cpp
=== Benchmark for: std::unordered_map ===
insert (Order):
  Throughput: 1.85 Mops/s
  Latency ns: 50th Percentile=300, 95th Percentile=600, 99th Percentile=900, Avg=414.37
find (Order):
  Throughput: 1.73 Mops/s
  Latency ns: 50th Percentile=400, 95th Percentile=800, 99th Percentile=1000, Avg=470.06
erase (Order, N/2):
  Throughput: 1.31 Mops/s
  Latency ns: 50th Percentile=500, 95th Percentile=1100, 99th Percentile=1700, Avg=618.01

=== Benchmark for: FlatHashMap ===
insert (Order):
  Throughput: 3.57 Mops/s
  Latency ns: 50th Percentile=100, 95th Percentile=300, 99th Percentile=600, Avg=150.60
find (Order):
  Throughput: 3.99 Mops/s
  Latency ns: 50th Percentile=100, 95th Percentile=300, 99th Percentile=600, Avg=143.34
erase (Order, N/2):
  Throughput: 3.54 Mops/s
  Latency ns: 50th Percentile=100, 95th Percentile=400, 99th Percentile=700, Avg=167.21
```
### `Order` Structure
```cpp
struct Order {
    std::uint64_t orderID;
    double price;
    enum class Side : bool { Buy = 0, Sell = 1 } side;
    std::uint32_t qty;
};
