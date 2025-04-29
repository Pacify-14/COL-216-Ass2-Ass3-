#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <sstream>
#include <string>
#include <iomanip>
#include <fstream>

using namespace std;

enum State { INVALID, SHARED, EXCLUSIVE, MODIFIED };

struct CacheLine {
    unsigned int tag;
    State state;
    bool dirty = false;
    int lru_counter = 0;
};

struct Transaction {
    unsigned int addr;
    int src_core;
    char type;
    atomic<bool>* shared;
    atomic<bool>* supplied;
    atomic<bool>* supplied_dirty;
    atomic<int>* snoop_count;
};

class Bus {
public:
    vector<class L1Cache*> caches;
    mutex bus_mutex;
    atomic<int> total_bus_transactions{0};
    atomic<int> total_traffic_bytes{0};

    Bus() {}

    void attach_cache(L1Cache* cache) {
        caches.push_back(cache);
    }

    void broadcast(unsigned int addr, int src_core, char type, int block_size, bool& shared, bool& supplied, bool& supplied_dirty) {
        atomic<bool> atomic_shared(false);
        atomic<bool> atomic_supplied(false);
        atomic<bool> atomic_supplied_dirty(false);
        atomic<int> snoop_count(caches.size() - 1); // Number of other cores

        Transaction tx = {addr, src_core, type, &atomic_shared, &atomic_supplied, &atomic_supplied_dirty, &snoop_count};

        bus_mutex.lock();
        total_bus_transactions.fetch_add(1);
        for (auto* cache : caches) {
            if (cache->core_id != src_core) {
                cache->snoop_mutex.lock();
                cache->snoop_queue.push(tx);
                cache->snoop_mutex.unlock();
            }
        }
        bus_mutex.unlock();

        while (snoop_count > 0) {
            this_thread::yield();
        }

        shared = atomic_shared.load();
        supplied = atomic_supplied.load();
        supplied_dirty = atomic_supplied_dirty.load();
    }
};

class L1Cache {
public:
    int core_id;
    Bus* bus;
    int S, E, B;
    vector<vector<CacheLine>> sets;
    queue<Transaction> snoop_queue;
    mutex snoop_mutex;
    vector<string> trace;
    int reads = 0, writes = 0, misses = 0, evictions = 0, writebacks = 0, invalidations = 0;
    int cycles = 0, idle_cycles = 0;
    int lru_counter = 0;
    int data_traffic = 0;

    L1Cache(int id, int s, int e, int b, Bus* b) : core_id(id), S(1 << s), E(e), B(1 << b), bus(b) {
        sets.resize(S, vector<CacheLine>(E, {0, INVALID}));
        bus->attach_cache(this);
    }

    void load_trace(const string& filename) {
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty()) trace.push_back(line);
        }
    }

    pair<unsigned int, unsigned int> get_index_tag(unsigned int addr) {
        unsigned int index = (addr >> B) & ((1 << S) - 1);
        unsigned int tag = addr >> (S + B);
        return {index, tag};
    }

    void snoop(unsigned int addr, int src_core, char type, bool& shared, bool& supplied, bool& supplied_dirty) {
        auto [index, tag] = get_index_tag(addr);
        for (auto& line : sets[index]) {
            if (line.tag == tag && line.state != INVALID) {
                if (type == 'W') {
                    if (line.state == MODIFIED || line.state == EXCLUSIVE || line.state == SHARED) {
                        if (line.state == MODIFIED) {
                            writebacks++;
                            data_traffic += B;
                            bus->total_traffic_bytes.fetch_add(B);
                        }
                        line.state = INVALID;
                        line.dirty = false;
                        invalidations++;
                    }
                } else if (type == 'R') {
                    if (line.state == MODIFIED) {
                        supplied = true;
                        supplied_dirty = true;
                        shared = true;
                        line.state = SHARED;
                        writebacks++;
                        data_traffic += B;
                        bus->total_traffic_bytes.fetch_add(B);
                    } else if (line.state == EXCLUSIVE) {
                        supplied = true;
                        shared = true;
                        line.state = SHARED;
                    } else if (line.state == SHARED) {
                        shared = true;
                    }
                }
            }
        }
    }

    bool access(char type, unsigned int addr) {
        if (type == 'R') reads++;
        else if (type == 'W') writes++;

        auto [index, tag] = get_index_tag(addr);
        vector<CacheLine>& set = sets[index];

        for (auto& line : set) {
            if (line.tag == tag && line.state != INVALID) {
                line.lru_counter = ++lru_counter;
                if (type == 'W') {
                    if (line.state == SHARED) {
                        bool shared = false, supplied = false, supplied_dirty = false;
                        bus->bus_mutex.lock();
                        bus->broadcast(addr, core_id, 'W', B, shared, supplied, supplied_dirty);
                        bus->bus_mutex.unlock();
                    }
                    line.state = MODIFIED;
                    line.dirty = true;
                }
                return true;
            }
        }

        misses++;
        idle_cycles += 100;
        cycles += 100;

        if (set.size() == (size_t)E) {
            auto lru_it = min_element(set.begin(), set.end(), [](auto& a, auto& b) {
                return a.lru_counter < b.lru_counter;
            });
            if (lru_it->dirty) {
                writebacks++;
                cycles += 100;
                idle_cycles += 100;
                data_traffic += B;
                bus->total_traffic_bytes.fetch_add(B);
            }
            evictions++;
            *lru_it = {0, INVALID};
        }

        bool shared = false, supplied = false, supplied_dirty = false;
        bus->bus_mutex.lock();
        bus->broadcast(addr, core_id, type, B, shared, supplied, supplied_dirty);
        bus->bus_mutex.unlock();

        CacheLine newline = {tag, type == 'W' ? MODIFIED : (shared ? SHARED : EXCLUSIVE), type == 'W'};
        newline.lru_counter = ++lru_counter;
        if (set.size() < (size_t)E) set.push_back(newline);

        if (supplied) {
            int n_words = B / 4;
            cycles += 2 * n_words;
            idle_cycles += 2 * n_words;
            data_traffic += B;
        } else {
            data_traffic += B;
        }
        return false;
    }

    void process_snoops() {
        snoop_mutex.lock();
        while (!snoop_queue.empty()) {
            Transaction tx = snoop_queue.front();
            snoop_queue.pop();
            snoop_mutex.unlock();

            bool local_shared = false, local_supplied = false, local_supplied_dirty = false;
            snoop(tx.addr, tx.src_core, tx.type, local_shared, local_supplied, local_supplied_dirty);

            if (local_shared) tx.shared->store(true);
            if (local_supplied) tx.supplied->store(true);
            if (local_supplied_dirty) tx.supplied_dirty->store(true);
            tx.snoop_count->fetch_sub(1);

            snoop_mutex.lock();
        }
        snoop_mutex.unlock();
    }
};

void core_thread(L1Cache& core) {
    for (size_t j = 0; j < core.trace.size(); j++) {
        core.process_snoops();
        string line = core.trace[j];
        char type;
        string addr_str;
        stringstream ss(line);
        ss >> type >> addr_str;
        unsigned int addr = stoul(addr_str, nullptr, 16);
        core.access(type, addr);
    }
    core.process_snoops();
}

int main(int argc, char* argv[]) {
    string trace_prefix, outfile;
    int s = 0, E = 0, b = 0;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-t") trace_prefix = argv[++i];
        else if (arg == "-s") s = stoi(argv[++i]);
        else if (arg == "-E") E = stoi(argv[++i]);
        else if (arg == "-b") b = stoi(argv[++i]);
        else if (arg == "-o") outfile = argv[++i];
        else if (arg == "-h") {
            cout << "$ ./L1simulate -t <tracefile> -s <s> -E <E> -b <b> -o <outfile>\n";
            return 0;
        }
    }

    Bus bus;
    vector<L1Cache> cores;
    for (int i = 0; i < 4; i++) {
        cores.emplace_back(i, s, E, b, &bus);
        cores[i].load_trace(trace_prefix + "_proc" + to_string(i) + ".trace");
    }

    vector<thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(core_thread, ref(cores[i]));
    }
    for (auto& t : threads) {
        t.join();
    }

    for (int i = 0; i < 4; i++) {
        L1Cache& core = cores[i];
        int accesses = core.reads + core.writes;
        double miss_rate = accesses ? (double)core.misses / accesses * 100 : 0.0;
        cout << "Core " << i << " Statistics:\n";
        cout << "Total Instructions: " << accesses << "\n";
        cout << "Total Reads: " << core.reads << "\n";
        cout << "Total Writes: " << core.writes << "\n";
        cout << "Total Execution Cycles: " << core.cycles << "\n";
        cout << "Idle Cycles: " << core.idle_cycles << "\n";
        cout << "Cache Misses: " << core.misses << "\n";
        cout << fixed << setprecision(2) << "Cache Miss Rate: " << miss_rate << "%\n";
        cout << "Cache Evictions: " << core.evictions << "\n";
        cout << "Writebacks: " << core.writebacks << "\n";
        cout << "Bus Invalidations: " << core.invalidations << "\n";
        cout << "Data Traffic (Bytes): " << core.data_traffic << "\n\n";
    }
    cout << "Overall Bus Summary:\n";
    cout << "Total Bus Transactions: " << bus.total_bus_transactions << "\n";
    cout << "Total Bus Traffic (Bytes): " << bus.total_traffic_bytes << "\n";

    return 0;
}
