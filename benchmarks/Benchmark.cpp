#include <benchmark/benchmark.h>
#include <filesystem>
#include <memory>
#include <cstdio>

#include "SkipList.hpp"
#include "SSTable.hpp"
#include "WAL.hpp"
#include "KVStore.hpp"

// ─── helpers ─────────────────────────────────────────────────────────
// Zero-padded keys keep lexicographic and numeric order identical,
// which matters for SSTable block layout and index correctness.
namespace {

std::string makeKey(int i) {
    char buf[32];
    snprintf(buf, sizeof(buf), "key_%08d", i);
    return buf;
}

std::string makeValue(int i) {
    return std::string(50, 'A' + (i % 26));
}

// Builds N entries via SkipList and flushes to a sorted vector.
// Used for SSTable setup — never measured directly.
// Includes a tombstone at the midpoint (n/2) to mirror real usage.
std::vector<LSM::FlushedEntry> buildEntries(int n) {
    LSM::SkipList sl;
    for (int i = 0; i < n; i++) sl.put(makeKey(i), makeValue(i));
    sl.remove(makeKey(n / 2));
    return sl.flushAll();
}

} // namespace

// ─── SkipList ─────────────────────────────────────────────────────────────────
//
// These measure the memtable layer in isolation.
// BM_SkipList_Put will improve after the RNG-per-insert bug is fixed.

// Time to insert N keys into a fresh SkipList.
// PauseTiming around construction so we measure only the puts.
static void BM_SkipList_Put(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        LSM::SkipList sl;
        state.ResumeTiming();

        for (int i = 0; i < n; i++)
            sl.put(makeKey(i), makeValue(i));
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_SkipList_Put)->Arg(1000)->Arg(5000)->Arg(10000);

// Single get from a pre-populated list, cycling through all keys
// so OS page-cache effects average out across the whole list.
static void BM_SkipList_Get_Hit(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    LSM::SkipList sl;
    for (int i = 0; i < n; i++) sl.put(makeKey(i), makeValue(i));

    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sl.get(makeKey(i % n)));
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SkipList_Get_Hit)->Arg(1000)->Arg(10000);

// Keys beyond the inserted range — every lookup should miss.
static void BM_SkipList_Get_Miss(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    LSM::SkipList sl;
    for (int i = 0; i < n; i++) sl.put(makeKey(i), makeValue(i));

    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sl.get(makeKey(n + i)));
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SkipList_Get_Miss)->Arg(1000)->Arg(10000);

// ─── WAL ─────────────────────────────────────────────────────────────────────
//
// BM_WAL_Append is the key benchmark for the flush() removal fix.
//
// Before fix:  flush() called on every append = one syscall per write
//              expect: hundreds of microseconds per op
// After fix:   OS buffers writes, flushes when it wants
//              expect: single-digit microseconds per op

class WALFixture : public benchmark::Fixture {
protected:
    std::string path_;
    std::unique_ptr<LSM::WAL> wal_;

    void SetUp(const benchmark::State&) override {
        path_ = (std::filesystem::temp_directory_path() / "bench_lsm.wal").string();
        std::filesystem::remove(path_);
        wal_ = std::make_unique<LSM::WAL>(path_);
    }

    void TearDown(const benchmark::State&) override {
        wal_.reset();
        std::filesystem::remove(path_);
    }
};

BENCHMARK_DEFINE_F(WALFixture, Append)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        wal_->append(makeKey(i), makeValue(i), LSM::RecordType::PUT);
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(WALFixture, Append);

// ─── SSTable write ────────────────────────────────────────────────────────────
//
// Measures how fast the flush path serialises N entries to disk.
// The entries vector is built once before the loop (not measured).

static void BM_SSTable_Write(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    auto entries = buildEntries(n);
    const std::string path =
        (std::filesystem::temp_directory_path() / "bench_write.sst").string();

    for (auto _ : state) {
        LSM::SSTable sst(path);
        sst.write(entries);
    }
    std::filesystem::remove(path);
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_SSTable_Write)->Arg(1000)->Arg(5000)->Arg(10000);

// ─── SSTable search ───────────────────────────────────────────────────────────
//
// All search benchmarks share one fixture that:
//   1. Writes the SSTable to disk
//   2. Opens a SEPARATE SSTable object for reading
//      (simulates startup, where the file already exists on disk)
//
// After the bloom filter caching fix, the constructor in step 2
// loads the filter from disk once here — so none of the search
// benchmarks below pay the deserialization cost per call.
//
// Before fix: Search_Miss_BloomFilter ≈ Search_Hit_Average (both ~550 µs)
// After fix:  Search_Miss_BloomFilter drops to ~5 µs, hits stay ~100-200 µs

class SSTableFixture : public benchmark::Fixture {
protected:
    std::string path_;
    std::unique_ptr<LSM::SSTable> sst_;
    int n_;

    void SetUp(const benchmark::State& state) override {
        n_ = static_cast<int>(state.range(0));
        path_ = (std::filesystem::temp_directory_path() / "bench_search.sst").string();

        // Write phase — separate object, not the one we benchmark
        LSM::SSTable writer(path_);
        writer.write(buildEntries(n_));

        // Reader — this is the object whose search() we actually measure
        sst_ = std::make_unique<LSM::SSTable>(path_);
    }

    void TearDown(const benchmark::State&) override {
        sst_.reset();
        std::filesystem::remove(path_);
    }
};

// First block — best case for the index (matches the very first entry)
BENCHMARK_DEFINE_F(SSTableFixture, Search_Hit_FirstBlock)(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(sst_->search(makeKey(0)));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SSTableFixture, Search_Hit_FirstBlock)->Arg(2000)->Arg(10000);

// Last block — worst case for the index (scans to the final entry)
BENCHMARK_DEFINE_F(SSTableFixture, Search_Hit_LastBlock)(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(sst_->search(makeKey(n_ - 1)));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SSTableFixture, Search_Hit_LastBlock)->Arg(2000)->Arg(10000);

// Tombstone at the midpoint — bloom filter passes it through,
// index finds the block, searchInBlock() detects record_type == 1 and returns nullopt.
// Should be similar cost to a normal hit in the same block.
BENCHMARK_DEFINE_F(SSTableFixture, Search_Tombstone)(benchmark::State& state) {
    const std::string key = makeKey(n_ / 2);
    for (auto _ : state)
        benchmark::DoNotOptimize(sst_->search(key));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SSTableFixture, Search_Tombstone)->Arg(2000)->Arg(10000);

// Key that definitely does not exist.
// Bloom filter should reject it without any index or block I/O.
// This is the benchmark that most directly shows the caching fix working.
BENCHMARK_DEFINE_F(SSTableFixture, Search_Miss_BloomFilter)(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(sst_->search(makeKey(n_ + 99999)));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SSTableFixture, Search_Miss_BloomFilter)->Arg(2000)->Arg(10000);

// Rotates through all existing keys — average cost across all blocks.
// More realistic than always hitting the same key (avoids OS page cache bias).
BENCHMARK_DEFINE_F(SSTableFixture, Search_Hit_Average)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sst_->search(makeKey(i % n_)));
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(SSTableFixture, Search_Hit_Average)->Arg(2000)->Arg(10000);

// ─── KVStore (end-to-end) ─────────────────────────────────────────────────────
//
// These measure the full public API — WAL + memtable together.
// BM_KVStore_Put will improve after the WAL flush() fix.

class KVStoreFixture : public benchmark::Fixture {
protected:
    std::string dir_;
    std::unique_ptr<LSM::KVStore> store_;

    void SetUp(const benchmark::State&) override {
        dir_ = (std::filesystem::temp_directory_path() / "bench_kvstore").string();
        std::filesystem::remove_all(dir_);
        store_ = std::make_unique<LSM::KVStore>(dir_);
    }

    void TearDown(const benchmark::State&) override {
        store_.reset();
        std::filesystem::remove_all(dir_);
    }
};

// Full write path: WAL::append() + SkipList::put().
// Most of the cost is the WAL flush() before the fix.
BENCHMARK_DEFINE_F(KVStoreFixture, Put)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        store_->put(makeKey(i), makeValue(i));
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(KVStoreFixture, Put);

// Read a key that is in the memtable — no SSTable involved.
// Baseline for how fast the in-memory path is.
BENCHMARK_DEFINE_F(KVStoreFixture, Get_Memtable_Hit)(benchmark::State& state) {
    const int n = 2000;
    for (int i = 0; i < n; i++) store_->put(makeKey(i), makeValue(i));

    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(store_->get(makeKey(i % n)));
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(KVStoreFixture, Get_Memtable_Hit);

// Read a key that was never inserted anywhere.
// Currently only checks the memtable (SSTable fallback not wired up yet).
BENCHMARK_DEFINE_F(KVStoreFixture, Get_Miss)(benchmark::State& state) {
    const int n = 2000;
    for (int i = 0; i < n; i++) store_->put(makeKey(i), makeValue(i));

    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(store_->get(makeKey(n + i)));
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(KVStoreFixture, Get_Miss);


// Measure the cost of a full memtable flush.
// This benchmark works by filling the memtable to one item below its flush
// threshold, then timing the single 'put' operation that triggers the flush.
// The cost measured includes:
//   1. wal->sync()
//   2. Writing all memtable data to a new SSTable file.
//   3. Clearing the WAL and resetting the memtable.
BENCHMARK_DEFINE_F(KVStoreFixture, Flush)(benchmark::State& state) {
    // This must match the private member in KVStore.hpp
    const size_t memtable_threshold = 4096;

    for (auto _ : state) {
        state.PauseTiming();
        // Reset the store to get a clean memtable for each run.
        store_.reset();
        std::filesystem::remove_all(dir_);
        store_ = std::make_unique<LSM::KVStore>(dir_);

        // Pre-fill the memtable just below the threshold.
        for (size_t i = 0; i < memtable_threshold - 1; ++i) {
            store_->put(makeKey(i), makeValue(i));
        }
        state.ResumeTiming();

        // This final put triggers the flush.
        store_->put(makeKey(memtable_threshold - 1), makeValue(memtable_threshold - 1));
    }
    // We processed N items to cause one flush.
    state.SetItemsProcessed(state.iterations() * memtable_threshold);
}
BENCHMARK_REGISTER_F(KVStoreFixture, Flush);

BENCHMARK_MAIN();