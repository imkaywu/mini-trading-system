
# Mini Trading System

An ongoing, from-scratch C++ project exploring low-latency trading infrastructure, with a focus on order matching, concurrency, memory efficiency, and performance benchmarking.

The current implementation centers on a **limit order book** and **matching engine**, with ongoing expansion into other aspects of trading infrastructure. The long-term goal is to build an end-to-end simulated trading system.

## Project Status & Roadmap

### Phase 1: Order Matching
- [x] Basic single-threaded limit order book
- [x] Order IDs and cancellation
- [x] Partial fills and timestamps
- [x] Correctness tests

### Phase 2: Performance & Concurrency
- [x] Memory allocation profiling
- [x] Reserved-capacity comparison
- [x] Separate correctness tests and benchmarks
- [x] Mutex + condition variable matching engine
- [x] Bounded SPSC matching engine
- [x] Initial concurrency benchmarks
- [ ] Queue-only benchmarking and capacity analysis
- [ ] Additional concurrency and trade-output benchmarking

### Phase 3: Trading Infrastructure
- [ ] Recovery and market-data output
- [ ] Market-data simulator
- [ ] Feed handler
- [ ] Strategy and risk engine
- [ ] Order management system
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

## Technical Highlights

### Limit Order Book

- Price-time priority matching.
- Order identification and cancellation.
- Full and partial order execution.
- Order timestamps.
- Resting-order management.

### Matching Engine & Concurrency

Two execution variants have been implemented:

| Variant | Mechanism | Purpose |
| --- | --- | --- |
| Mutex + Condition Variable | Lock-based producer-consumer synchronization | Baseline implementation |
| SPSC Ring Buffer | Bounded, lock-free producer-consumer queue | Explore reduced synchronization overhead |

The SPSC implementation assumes exactly one producer and one consumer.

The consumer exclusively owns order-book mutation, avoiding concurrent modifications to the underlying book.

### Performance Benchmarking

Benchmarks examine:

- Order-processing throughput and latency.
- Dynamic memory allocation behavior.
- Reserved versus unreserved container capacity.
- Mutex/condition-variable versus SPSC concurrency.
- Producer submission time versus total processing time.

Measurements distinguish submission throughput from end-to-end throughput, since faster submission does not necessarily imply faster overall processing.

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
