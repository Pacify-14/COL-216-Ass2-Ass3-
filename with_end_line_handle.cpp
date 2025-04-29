#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <deque>
#include <cmath>
#include <bitset>
#include <iomanip>
#include <algorithm>
#include <tuple>
#include <string>
#include <random>
#include <algorithm>

using namespace std;

enum MESIState { M, E, S, I };

struct CacheLine {
	bool valid = false;
	bool dirty = false;
	MESIState state = I;
	unsigned int tag = 0;
	int lru_counter = 0;
};

class Bus; // Forward declaration

class L1Cache {
	public:
		size_t current_access = 0;
		int core_id;
		int s, E, b;
		int S, B;
		vector<deque<CacheLine>> sets;
		vector<string> trace;
		int reads = 0, writes = 0, misses = 0, evictions = 0, writebacks = 0, invalidations = 0;
		int cycles = 0, idle_cycles = 0;
		int lru_counter = 0;
		int data_traffic = 0; // Per-core data traffic
		Bus* bus;

		L1Cache(int id, int s_bits, int E_lines, int b_bits, Bus* bus_ptr)
			: core_id(id), s(s_bits), E(E_lines), b(b_bits), bus(bus_ptr) {
				S = 1 << s;
				B = 1 << b;
				sets.resize(S);
			}

		void load_trace(const string& filename);
		void simulate();
		bool access(char type, unsigned int addr);
		void snoop(unsigned int addr, int src_core, char type, bool& shared, bool& supplied, bool& supplied_dirty);
		pair<unsigned int, unsigned int> get_index_tag(unsigned int addr);
		void print_stats();
};

class Bus {
	public:
		// Add arbitration state
		bool bus_busy = false;

		vector<L1Cache*> caches;
		int total_bus_transactions = 0;
		int total_traffic_bytes = 0;

		// For each bus transaction, track if it is a read or write, and update bus stats
		void broadcast(unsigned int addr, int src_core, char type, int block_size, bool& shared, bool& supplied, bool& supplied_dirty);
};

void L1Cache::load_trace(const string& filename) {
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        // Trim leading and trailing whitespace
        line.erase(line.begin(), find_if(line.begin(), line.end(), [](unsigned char c) { return !isspace(c); }));
        line.erase(find_if(line.rbegin(), line.rend(), [](unsigned char c) { return !isspace(c); }).base(), line.end());
        // Only add non-empty lines
        if (!line.empty()) trace.push_back(line);
    }
}

pair<unsigned int, unsigned int> L1Cache::get_index_tag(unsigned int addr) {
	unsigned int index = (addr >> b) & ((1 << s) - 1);
	unsigned int tag = addr >> (s + b);
	return {index, tag};
}

// MESI protocol access
bool L1Cache::access(char type, unsigned int addr) {
	// Increment counters at start of access
	if (type == 'R') reads++;
	else if (type == 'W') writes++;

	// Always increment 1 cycle for issuing the instruction
	cycles += 1;

	auto [index, tag] = get_index_tag(addr);
	deque<CacheLine>& set = sets[index];

	// Check for hit
	for (auto& line : set) {
		if (line.valid && line.tag == tag) {
			// Hit handling
			line.lru_counter = ++lru_counter;

			if (type == 'W') {
				if (line.state == S) {
					// Acquire bus only for invalidations
					
					bus->bus_busy = true;
					bool shared, supplied, supplied_dirty;
					bus->broadcast(addr, core_id, 'W', B, shared, supplied, supplied_dirty);
					bus->bus_busy = false;
				}
				line.state = M;
				line.dirty = true;
			}
			return true;
		}
	}

	// Miss handling - acquire bus
	
	bus->bus_busy = true;

	// Miss
	misses++;
	idle_cycles += 100;
	cycles += 100;

	// LRU eviction if needed
	bool evict = (set.size() == (size_t)E);
	if (evict) {
		auto lru_it = min_element(set.begin(), set.end(), [](auto& a, auto& b) {
				return a.lru_counter < b.lru_counter;
				});
		if (lru_it->dirty) {
			writebacks++;
			cycles += 100;
			idle_cycles += 100;
			data_traffic += B; // Writeback block to memory
			bus->total_traffic_bytes += B;
		}
		evictions++;
		set.erase(lru_it);
	}

	// MESI protocol: check if other caches have the block
	bool shared = false, supplied = false, supplied_dirty = false;
	bus->broadcast(addr, core_id, type, B, shared, supplied, supplied_dirty);

	// Insert new line
	CacheLine newline;
	newline.valid = true;
	newline.tag = tag;
	newline.lru_counter = ++lru_counter;
	if (type == 'W') {
		newline.state = ::M;
		newline.dirty = true;
	} else {
		if (supplied) {
			newline.state = ::S;
			if (supplied_dirty) {
				// Data supplied by another cache in M state, so it must write back
				// Already accounted in snoop
			}
		} else if (shared) {
			newline.state = ::S;
		} else {
			newline.state = ::E;
		}
		newline.dirty = false;
	}
	set.push_back(newline);

	// Data traffic: fetch from memory or another cache
	if (supplied) {
		// Data supplied by another cache: 2N cycles, N = block words
		int n_words = B / 4;
		cycles += 2 * n_words;
		idle_cycles += 2 * n_words;
		data_traffic += B;
		bus->total_traffic_bytes += B;
	} else {
		// Fetch from memory: already added 100 cycles above
		data_traffic += B;
		bus->total_traffic_bytes += B;
	}
	bus->bus_busy = false;
	return false;
}

// Snooping: called by bus on other caches
void L1Cache::snoop(unsigned int addr, int src_core, char type, bool& shared, bool& supplied, bool& supplied_dirty) {
	auto [index, tag] = get_index_tag(addr);
	for (auto& line : sets[index]) {
		if (line.valid && line.tag == tag) {
			if (type == 'W') {
				// Invalidate
				if (line.state == ::M || line.state == ::E || line.state == ::S) {
					if (line.state == ::M) {
						// Writeback dirty block to memory
						writebacks++;
						data_traffic += B;
						bus->total_traffic_bytes += B;
					}
					line.state = ::I;
					line.dirty = false;
					invalidations++;
				}
			} else if (type == 'R') {
				// If in M or E or S, set shared
				if (line.state == ::M) {
					// Supply data to requester
					supplied = true;
					supplied_dirty = true;
					shared = true;
					line.state = ::S;
					// Writeback to memory (simulate supplying data)
					writebacks++;
					data_traffic += B;
					bus->total_traffic_bytes += B;
				} else if (line.state == ::E) {
					supplied = true;
					shared = true;
					line.state = ::S;
				} else if (line.state == ::S) {
					shared = true;
				}
			}
		}
	}
}

// Bus broadcast: notifies all other caches
void Bus::broadcast(unsigned int addr, int src_core, char type, int block_size, bool& shared, bool& supplied, bool& supplied_dirty) {
	total_bus_transactions++;
	shared = false;
	supplied = false;
	supplied_dirty = false;
	for (auto* cache : caches) {
		if (cache->core_id != src_core) {
			cache->snoop(addr, src_core, type, shared, supplied, supplied_dirty);
		}
	}
	// For write, 4 bytes data traffic (word)
	if (type == 'W') {
		total_traffic_bytes += 4;
	}
}

void L1Cache::print_stats() {
	int accesses = reads + writes;
	double miss_rate = (accesses > 0) ? (double)misses / accesses * 100 : 0.0;
	int cache_size = S * E * B / 1024;

	cout << "Core " << core_id << " Statistics:\n";
	cout << "Total Instructions: " << accesses << "\n";
	cout << "Total Reads: " << reads << "\n";
	cout << "Total Writes: " << writes << "\n";
	cout << "Total Execution Cycles: " << cycles << "\n";
	cout << "Idle Cycles: " << idle_cycles << "\n";
	cout << "Cache Misses: " << misses << "\n";
	cout << fixed << setprecision(2) << "Cache Miss Rate: " << miss_rate << "%\n";
	cout << "Cache Evictions: " << evictions << "\n";
	cout << "Writebacks: " << writebacks << "\n";
	cout << "Bus Invalidations: " << invalidations << "\n";
	cout << "Data Traffic (Bytes): " << data_traffic << "\n\n";
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
	cores.reserve(4);

	for (int i = 0; i < 4; i++) {
		cores.emplace_back(i, s, E, b, &bus);
		cores[i].load_trace(trace_prefix + "_proc" + to_string(i) + ".trace");
		bus.caches.push_back(&cores[i]);
	}

	// New arbitration-based simulation loop
	vector<size_t> current_access(4, 0);  // Track progress per core
	bool active = true;

	while (active) {
		active = false;
		vector<int> requesting_cores;

		// Collect cores with pending accesses
		for (int i = 0; i < 4; i++) {
			if (current_access[i] < cores[i].trace.size()) {
				requesting_cores.push_back(i);
				active = true;
			}
		}

		if (!requesting_cores.empty()) {
			for (int selected : requesting_cores) {  // Go through cores in increasing ID
				L1Cache& core = cores[selected];
				if (current_access[selected] < core.trace.size()) {
					string line = core.trace[current_access[selected]];
					char type;
					string addr_str;
					stringstream ss(line);
					ss >> type >> addr_str;
					unsigned int addr = stoul(addr_str, nullptr, 16);

					core.access(type, addr);
					current_access[selected]++;
				}
			}
		}

	}


	cout << "Simulation Parameters:\n";
	cout << "Trace Prefix: " << trace_prefix << "\n";
	cout << "Set Index Bits: " << s << "\n";
	cout << "Associativity: " << E << "\n";
	cout << "Block Bits: " << b << "\n";
	cout << "Block Size (Bytes): " << (1 << b) << "\n";
	cout << "Number of Sets: " << (1 << s) << "\n";
	cout << "Cache Size (KB per core): " << ((1 << s) * E * (1 << b)) / 1024 << "\n";
	cout << "MESI Protocol: Enabled\n";
	cout << "Write Policy: Write-back, Write-allocate\n";
	cout << "Replacement Policy: LRU\n";
	cout << "Bus: Central snooping bus\n\n";

	for (auto& core : cores) core.print_stats();

	cout << "Overall Bus Summary:\n";
	cout << "Total Bus Transactions: " << bus.total_bus_transactions << "\n";
	cout << "Total Bus Traffic (Bytes): " << bus.total_traffic_bytes << "\n";

	// Optionally write to output file
	if (!outfile.empty()) {
		ofstream out(outfile);
		if (out) {
			out << "Simulation Parameters:\n";
			out << "Trace Prefix: " << trace_prefix << "\n";
			out << "Set Index Bits: " << s << "\n";
			out << "Associativity: " << E << "\n";
			out << "Block Bits: " << b << "\n";
			out << "Block Size (Bytes): " << (1 << b) << "\n";
			out << "Number of Sets: " << (1 << s) << "\n";
			out << "Cache Size (KB per core): " << ((1 << s) * E * (1 << b)) / 1024 << "\n";
			out << "MESI Protocol: Enabled\n";
			out << "Write Policy: Write-back, Write-allocate\n";
			out << "Replacement Policy: LRU\n";
			out << "Bus: Central snooping bus\n\n";
			for (auto& core : cores) {
				out << "Core " << core.core_id << " Statistics:\n";
				int accesses = core.reads + core.writes;
				double miss_rate = accesses ? (double)core.misses / accesses * 100 : 0.0;
				out << "Total Instructions: " << accesses << "\n";
				out << "Total Reads: " << core.reads << "\n";
				out << "Total Writes: " << core.writes << "\n";
				out << "Total Execution Cycles: " << core.cycles << "\n";
				out << "Idle Cycles: " << core.idle_cycles << "\n";
				out << "Cache Misses: " << core.misses << "\n";
				out << fixed << setprecision(2) << "Cache Miss Rate: " << miss_rate << "%\n";
				out << "Cache Evictions: " << core.evictions << "\n";
				out << "Writebacks: " << core.writebacks << "\n";
				out << "Bus Invalidations: " << core.invalidations << "\n";
				out << "Data Traffic (Bytes): " << core.data_traffic << "\n\n";
			}
			out << "Overall Bus Summary:\n";
			out << "Total Bus Transactions: " << bus.total_bus_transactions << "\n";
			out << "Total Bus Traffic (Bytes): " << bus.total_traffic_bytes << "\n";
		}
	}

	return 0;
}
