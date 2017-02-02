#include <stdio.h>
#include <string>
#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <vector>
#include <list>
#include <fstream>
#include <sstream>
#include <cmath>
#include <numeric>
#include "zlib.h"

using namespace std;

#define NUM_ARGS 2

typedef enum res {
	SUCCESS,
	FAIL
} res_t;

typedef enum mesi {
	MODIFIED,
	EXCLUSIVE,
	SHARED,
	INVALID
} mesi_t;

typedef enum replacement_policy {
	LRU
} replacement_policy_t;
string replacement_policy_s[] = {"LRU"};

typedef enum prefetch_type {
	NONE,
	N1L,
	BR_TARGET,
} prefetch_type_t;
string prefetch_type_s[] = {"NONE", "N1L"};

struct cache_index {
	std::list<uint64_t> replacement_order;
	std::unordered_map<uint64_t, struct cache_way> ways;
};

struct cache_way {
	bool dirty_bit;
	mesi_t mesi_bits;
};

struct cache_config {
	int sets;
	int ways;
	int cl_size;
	replacement_policy_t replacement_policy;
	int hit_latency;
	int addr_bits;
	int in_request_buffer_size;
	int in_response_buffer_size;
	int read_request_buffer_size;
	int write_request_buffer_size;
};

struct cache_config cache_config = {
		.sets = 64,
		.ways = 8,
		.cl_size = 64,
		.replacement_policy = LRU,
		.hit_latency = 1,
		.addr_bits = 48,
		.in_request_buffer_size = 16,
		.in_response_buffer_size = 16,
		.read_request_buffer_size = 16,
		.write_request_buffer_size = 16
};


prefetch_type_t prefetch_type = NONE;

//globals
uint32_t set_shift;
uint64_t set_mask;
uint64_t tag_mask;
uint64_t tag_shift;
uint64_t total_misses;
uint64_t total_requests;
uint64_t total_replacements;
uint64_t steps;
unordered_map<uint64_t, struct cache_index>tag_store;

// debug level
int level = 3;

void log_insanity(string str)
{
	if (level > 2)
		cout << steps << ": " << str << endl;
}

uint64_t get_set_idx(uint64_t addr)
{
	return ((addr >> set_shift) & set_mask);
}

uint64_t get_tag(uint64_t addr)
{
	return ((addr >> tag_shift) & tag_mask);
}

res_t fill(uint64_t addr, mesi_t mesi_bits)
{
	uint64_t set_idx = get_set_idx(addr);
	struct cache_index &set = tag_store[set_idx];
	int evict = false;
	if (set.replacement_order.size() >= cache_config.ways) {
		total_replacements++;
		uint64_t wb_tag = set.replacement_order.front();
		if (set.ways[wb_tag].dirty_bit == true) {
			//TODO: deal with writeback
		}
		// Request *request = new Request(config, addr);
		// request->request_type = WRITE;
		// request->tag = 0;
		// write_request_buffer.push_back(request);
		set.replacement_order.pop_front();
		set.ways.erase(wb_tag);
		if (level > 0) {
			cout << dec << steps << hex << ": EVICT set - " << set_idx << "; tag - " << wb_tag << endl;
		}
		evict = true;
	}
	uint64_t tag = get_tag(addr);
	if ((level > 1) || ((level > 0) && (evict))) {
		cout << dec << steps << hex << ": FILL ia - " << addr << "; set - " << set_idx << "; tag - " << tag << endl;
	}
	struct cache_way new_way = {
			.dirty_bit = false,
			.mesi_bits = mesi_bits
		};
	set.ways[tag] = new_way;
	set.replacement_order.push_back(tag);
	return SUCCESS;
}

res_t can_read(uint64_t addr)
{
	uint64_t set_idx = get_set_idx(addr);
	if (tag_store.find(set_idx) == tag_store.end()) {
		if (level > 1) {
			cout << dec << steps << hex << ": MISS ia - " << addr << "; set - " << set_idx << endl;
		}
		return FAIL;
	}
	uint64_t tag = get_tag(addr);
	struct cache_index &set = tag_store[set_idx];
	if (set.ways.find(tag) == set.ways.end()) {
		if (level > 1) {
			cout << dec << steps << hex << ": MISS ia - " << addr << "; set - " << set_idx << "; tag - " << tag << endl;
		}
		return FAIL;
	}
	if (level > 1) {
		cout << dec << steps << hex << ": HIT ia - " << addr << "; set - " << set_idx << "; tag - " << tag << endl;
	}
	return SUCCESS;
}

res_t read(uint64_t addr)
{
	// Can Read
	uint64_t set_idx = get_set_idx(addr);
	if (tag_store.find(set_idx) == tag_store.end()) {
		return FAIL;
	}
	uint64_t tag = get_tag(addr);
	struct cache_index &set = tag_store[set_idx];
	if (set.ways.find(tag) == set.ways.end()) {
		return FAIL;
	}

	//do the read
	set.replacement_order.remove(tag);
	set.replacement_order.push_back(tag);
	if (level > 2) {
		cout << dec << steps << ": LRU - ";
		for (uint64_t replacement : set.replacement_order) {
			cout << hex << replacement << " ";
		}
		cout << endl;
	}
	return SUCCESS;
}

int main(int argc, char *argv[])
{
	if (argc < NUM_ARGS+1) {
		cout << "Does not include all arguments." << endl;
		return 1;
	}

	gzFile infile = gzopen(argv[1], "r");
	char line[500];

	cout << hex;

	//statistics
	total_misses = 0;
	total_requests = 0;

	set_shift = (uint32_t) log2(cache_config.cl_size);
	set_mask = (uint64_t) (cache_config.sets - 1);
	tag_shift = (uint32_t) (log2(cache_config.sets) + set_shift);
	tag_mask = (uint64_t) (pow(2, cache_config.addr_bits - tag_shift) - 1);

	cout << dec << "sets: " << cache_config.sets << endl;
	cout << "ways: " << cache_config.ways << endl;
	cout << "cache line size: " << cache_config.cl_size << endl;
	cout << "cache size: " << (cache_config.cl_size * cache_config.ways * cache_config.sets) << " bytes" << endl;
	cout << "set shift: " << set_shift << "; set mask: " << hex << set_mask << endl;
	cout << "tag shift: " << dec << tag_shift << "; tag_mask: " << hex << tag_mask << endl;
	cout << "replacement policy: " << replacement_policy_s[cache_config.replacement_policy] << endl;
	cout << "prefetch_type: " << prefetch_type_s[prefetch_type] << endl;

	uint64_t prev_load_block;

	for (steps = 0; steps < 1000; steps++) {
		if (gzgets(infile, line, 500) == NULL) break;

		// skip lines with time and instructions
		string s_line(line);
		s_line.erase(remove(s_line.begin(), s_line.end(), '\n'), s_line.end());
		istringstream ss_line(s_line);
		size_t found = s_line.find("time");
		if (found != string::npos) continue;
		found = s_line.find("instructions");
		if (found != string::npos) continue;

		uint64_t ia;
		ss_line >> hex >> ia;

		if (level > 2) {
			cout << s_line << endl;
		}

		if (prev_load_block == (ia >> 4)) continue;

		total_requests++;
		if (can_read(ia) == FAIL) {
			total_misses++;
			fill(ia, EXCLUSIVE);
		}
		if (prefetch_type == N1L) {
			if (can_read(ia+cache_config.cl_size) == FAIL) {
				fill(ia+cache_config.cl_size, EXCLUSIVE);
			}
		}
		read(ia);

		prev_load_block = (ia >> 4);

	}

	//print results
	cout << dec;
	cout << "Total Instructions: " << steps << endl;
	cout << "Total Requests: " << total_requests << endl;
	cout << "Total Misses: " << total_misses << endl;
	cout << "Total Replacements: " << total_replacements << endl;

	//output to file
	fstream outfile;
	outfile.open(argv[2], fstream::app);

	outfile << dec << "sets: " << cache_config.sets << endl;
	outfile << "ways: " << cache_config.ways << endl;
	outfile << "cache line size: " << cache_config.cl_size << endl;
	outfile << "cache size: " << (cache_config.cl_size * cache_config.ways * cache_config.sets) << " bytes" << endl;
	outfile << "set shift: " << set_shift << "; set mask: " << hex << set_mask << endl;
	outfile << "tag shift: " << dec << tag_shift << "; tag_mask: " << hex << tag_mask << endl;
	outfile << "replacement policy: " << replacement_policy_s[cache_config.replacement_policy] << endl;
	outfile << "prefetch_type: " << prefetch_type_s[prefetch_type] << endl;

	outfile << dec;
	outfile << "Total Instructions: " << steps << endl;
	outfile << "Total Requests: " << total_requests << endl;
	outfile << "Total Misses: " << total_misses << endl;
	outfile << "Total Replacements: " << total_replacements << endl;
	outfile << endl;

	outfile.close();

}
