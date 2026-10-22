# Mini Trading System

An ongoing, from-scratch C++ project exploring low-latency trading infrastructure.

The current implementation centers on a **limit order book** and **matching engine**, with ongoing expansion into other aspects of trading infrastructure. The long-term goal is to build an end-to-end simulated trading system.

## Project Status & Roadmap

### 1. Limit Order Book
- [x] Basic price-time priority matching
- [x] Order IDs and cancellation
- [x] Partial fills and timestamps
- [x] Correctness tests
- [x] Memory allocation profiling and optimization

### 2. Matching Engine & Concurrency
- [x] Single-threaded matching engine
- [x] Mutex + condition variable implementation
- [x] Lock-free SPSC ring buffer implementation
- [x] Concurrency benchmarks
- [ ] Queue-only benchmarking and capacity analysis
- [ ] Additional trade-output benchmarking

### 3. Recovery & Market Data Output
- [ ] State persistence and recovery
- [ ] Order and trade event publication

### 4. Market Data Pipeline
- [ ] Market-data simulator
- [ ] Feed handler
- [ ] SPSC event pipeline
- [ ] Market-state reconstruction

### 5. Strategy & Execution
- [ ] Strategy
- [ ] Risk engine
- [ ] Order management system (OMS)
- [ ] Exchange simulator

## Architecture

### Target Architecture

The long-term goal is an end-to-end simulated trading system, covering market-data ingestion, strategy execution, order management, and exchange simulation.

```text
                       MARKET DATA
                            |
                            v
                 +-----------------------+
                 | Market Data Simulator |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |     Feed Handler      |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |    SPSC Ring Buffer   |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |   Market State / Book |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |        Strategy       |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |      Risk Engine      |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |          OMS          |
                 +-----------+-----------+
                             |
                             v
                 +-----------------------+
                 |   Exchange Simulator  |
                 |                       |
                 |    Matching Engine    |
                 |          |            |
                 |          v            |
                 |    Limit Order Book   |
                 +-----------+-----------+
                             |
                             v
                    Execution Reports
```

The target architecture is a conceptual design. Components will be implemented incrementally.

### Current Implementation

The existing implementation focuses on the order-matching subsystem, supporting two alternative concurrency mechanisms.

```text
                   Producer
                      |
                      v
          +-------------------------+
          |     Order Submission    |
          +------------+------------+
                       |
                       v
          +-------------------------+
          |    Transport Variant    |
          |                         |
          |  Mutex + Condition Var. |
          |           OR            |
          |    SPSC Ring Buffer     |
          +------------+------------+
                       |
                       v
          +-------------------------+
          |     Matching Engine     |
          |    (Single Consumer)    |
          +------------+------------+
                       |
                       v
          +-------------------------+
          |     Limit Order Book    |
          |                         |
          |  Match / Rest / Cancel  |
          +-------------------------+
```

Both concurrency variants share the underlying order-book implementation, allowing their performance to be compared under equivalent workloads.

## Build and Run

The project follows a simple **unity-build approach**, compiling each executable from a single entry-point translation unit.

No CMake or Makefiles are required.

### Main Program

```bash
./build.sh
./build/main
```

### Tests

Each test entry point is compiled into a standalone executable.

```bash
./build.sh tests/limit_order_book_test.cpp
./build/limit_order_book_test

./build.sh tests/spsc_queue_test.cpp
./build/spsc_queue_test
```

### Benchmarks

Benchmarks are maintained separately from the main program and correctness tests.

```bash
./build.sh benchmark/<benchmark_name>.cpp
./build/<benchmark_name>
```

Performance measurements should use optimized builds (e.g. `-O3`) rather than debug builds (`-O0`).

## License

MIT License
