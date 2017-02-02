#include <stdio.h>
#include <string>
#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <numeric>
#include "zlib.h"

#define NUM_ARGS 2
#define CL_SIZE 64
#define CL_SHIFT 6
#define MAX_DISTANCE 32768
#define MAX_BRANCHES 100
#define BRANCH_OTHER_DISTANCE 0xFFFFFFFF
#define CACHE_SIZE 32768

using namespace std;

unordered_map<uint64_t, uint64_t> cl_tracker;
unordered_map<uint64_t, uint64_t> iss_tracker;
unordered_map<uint64_t, map<int64_t, int> > branch_predictability;
unordered_map<uint64_t, map<int64_t, int> > jump_predictability;
unordered_map<uint64_t, bool> branch_transition_tracker;
map<int64_t, int> stride_tracker;
map<uint64_t, int> reuse_tracker;

unordered_map<uint64_t, uint64_t> iss_block_tracker;

typedef enum {
	BRANCH,
	JUMP,
	CALL,
	RETURN
} control_type_t;

int main(int argc, char *argv[])
{
	if (argc < NUM_ARGS+1) {
		cout << "Does not include all arguments." << endl;
		return 1;
	}

	cout << argv[1] << endl;
	gzFile infile = gzopen(argv[1], "r");

	char line[500];
	uint64_t i = 0;
	int64_t prev_shifted_ia = 0;
	uint64_t prev_ia = 0;
	uint64_t transitions = 0;
	uint64_t branches = 0;
	uint64_t jumps = 0;
	control_type_t control_type;

	uint64_t total_ins = 0;
	uint64_t read_ins = 0;
	uint64_t write_ins = 0;
	uint64_t call_ins = 0;
	uint64_t ret_ins = 0;
	uint64_t taken_branches = 0;
	uint64_t other_ins = 0;

	uint64_t cl_i = 0;


	for (i = 0; i < 350000000; i++) {
		if (gzgets(infile, line, 500) == NULL) break;
	//while (gzgets(infile, line, 500) != NULL) {
		//cout << line << endl;

		// skip lines with time and instructions
		string s_line(line);
		s_line.erase(remove(s_line.begin(), s_line.end(), '\n'), s_line.end());
		istringstream ss_line(s_line);
		size_t found = s_line.find("time");
		if (found != string::npos) continue;
		found = s_line.find("instructions");
		if (found != string::npos) continue;

		// parse line
		size_t pos;
		string s_ia;
		uint64_t ia;
		ss_line >> hex >> ia;
		//uint64_t ia = stol(s_ia, NULL, 16);
		//string br_type = s_line.substr(pos+1, 1);
		uint64_t da;
		string br_type;
		ss_line >> br_type;
		string qual = br_type;
		do {
			if (qual == "R") {
				ss_line >> da;
				read_ins++;
			} else if (qual == "W") {
				ss_line >> da;
				write_ins++;
			} else if (qual == "") {
				other_ins++;
			}
			ss_line >> qual;
		} while (ss_line >> qual);
		//cout << br_type << endl;


		//cout << (ia >> CL_SHIFT) << endl;

		// update reuse counts
		int64_t shifted_ia = ia >> CL_SHIFT;
		uint64_t prev_ts = cl_tracker[shifted_ia];

		if (prev_shifted_ia != shifted_ia) {

			if (prev_ts != 0) {
				uint64_t distance = pow(2, ceil(log2(cl_i - prev_ts)));
				if (distance > MAX_DISTANCE)
					reuse_tracker[MAX_DISTANCE] += 1;
				else
					reuse_tracker[distance] += 1;
			}
			cl_tracker[shifted_ia] = cl_i;
			iss_tracker[shifted_ia] += 1;
			
				iss_block_tracker[shifted_ia] += 1;


			// update stride counts
			if (prev_shifted_ia != 0) {
				int64_t distance = pow(2, ceil(log2(abs(shifted_ia - prev_shifted_ia))));
				if (prev_shifted_ia > shifted_ia) {
					distance = -distance;
				}
				if (distance > MAX_DISTANCE) {
					stride_tracker[MAX_DISTANCE] += 1;
				} else if (distance < -MAX_DISTANCE) {
					stride_tracker[-MAX_DISTANCE] += 1;
				} else {
					stride_tracker[distance] += 1;
				}
			}
			prev_shifted_ia = shifted_ia;
			cl_i++;

		}

		// update branch predictability
		if (prev_ia != 0 && control_type == BRANCH) {
			//cout << "here1" << endl;
			int64_t distance = 0;
			if (prev_ia > ia) {
				distance = -(prev_ia - ia);
			} else {
				distance = ia - prev_ia;
			}
			if (branch_predictability[prev_ia].size() > MAX_BRANCHES) {
				branch_predictability[prev_ia][BRANCH_OTHER_DISTANCE] += 1;
			} else {
				branch_predictability[prev_ia][distance] += 1;
			}
		} else if (prev_ia != 0 && ((control_type == JUMP) || (control_type == CALL))) {
			//cout << "here1" << endl;
			int64_t distance = 0;
			if (prev_ia > ia) {
				distance = -(prev_ia - ia);
			} else {
				distance = ia - prev_ia;
			}
			if (jump_predictability[prev_ia].size() > MAX_BRANCHES) {
				jump_predictability[prev_ia][BRANCH_OTHER_DISTANCE] += 1;
			} else {
				jump_predictability[prev_ia][distance] += 1;
			}
		}
		if (br_type == "T") {
			//cout << "here" << endl;
			prev_ia = ia;
			taken_branches += 1;
			branches += 1;
			control_type = BRANCH;
		} else if (br_type == "N") {
			branches += 1;
			prev_ia = 0;
			control_type = BRANCH;
		} else if (br_type == "J") {
			jumps += 1;
			prev_ia = ia;
			control_type = JUMP;
		} else if (br_type == "A") {
			call_ins++;
			prev_ia = ia;
			control_type = CALL;
		} else if (br_type == "E") {
			ret_ins++;
			prev_ia = 0;
			control_type = RETURN;
		} else {
			prev_ia = 0;
		}
		if (br_type == "T" && branch_transition_tracker[ia] == false) {
			transitions += 1;
			branch_transition_tracker[ia] = true;
		} else if (br_type == "N" && branch_transition_tracker[ia] == true) {
			transitions += 1;
			branch_transition_tracker[ia] = false;
		}


		total_ins++;
		//i++;
	}

	gzclose(infile);

	fstream outfile;
	outfile.open(argv[2], fstream::app);

	outfile << argv[1] << ",Total Instructions," << total_ins << endl;
	outfile << argv[1] << ",Total Reads," << read_ins << endl;
	outfile << argv[1] << ",Total Writes," << write_ins << endl;
	outfile << argv[1] << ",Total Taken Branches," << taken_branches << endl;
	outfile << argv[1] << ",Total Not Taken Branches," << (branches - taken_branches) << endl;
	outfile << argv[1] << ",Total Jumps," << jumps << endl;
	outfile << argv[1] << ",Total Calls," << call_ins << endl;
	outfile << argv[1] << ",Total Returns," << ret_ins << endl;
	outfile << argv[1] << ",Other," << other_ins << endl;

	//ISS
	outfile << argv[1] << " Instruction Set Size for 64 byte block size:" << endl;
	vector<uint64_t> iss_weights;
	for(unordered_map<uint64_t,uint64_t>::iterator it = iss_tracker.begin(); it != iss_tracker.end(); it++) {
		iss_weights.push_back(it->second);
	}
	
	outfile << "total static blocks " << iss_tracker.size() << endl;
	sort(iss_weights.begin(), iss_weights.end(), greater<uint64_t>());
	
	uint64_t static_blocks = 0;
	uint64_t dynamic_blocks = 0;
	uint64_t total_blocks = accumulate(iss_weights.begin(), iss_weights.end(), 0);
	uint64_t cache_dynamic = total_blocks;
	bool cache_dynamic_found = false;
	vector<uint64_t>::iterator it = iss_weights.begin();
	//cout << "total_blocks: " << total_blocks << endl;
	//cout << "total static blocks: " << iss_weights.size() << endl;
	float points[] {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.91, 0.92, 0.93, 0.94, 0.95, 0.96, 0.97, 0.98, 0.99, 0.991, 0.992, 0.993, 0.994, 0.995, 0.996, 0.997, 0.998, 0.999, 1.0};
	for(float i : points) {
		cout << "proof: " << (it != iss_weights.end()) << endl;
		if (i == 0.999) {
			outfile << "counts at " << i << " " << *it << endl;
		}
		for(; it != iss_weights.end(); it++) {
			if (((float)dynamic_blocks/(float)total_blocks) >= i) {
				break;
			}
			static_blocks += 1;
			dynamic_blocks += *it;
			if ((!cache_dynamic_found) && ((static_blocks<<CL_SHIFT) >= CACHE_SIZE)) {
				cache_dynamic = dynamic_blocks;
				cache_dynamic_found = true;
				outfile << "when cache size reached, counts per block at " << *it << endl;
			}
		}
		outfile << (float)dynamic_blocks/(float)total_blocks*100.0 << "\%," << (static_blocks << CL_SHIFT) << ",Bytes" << endl;
		//cout << "i: " << i << endl;
	}
	if (it != iss_weights.end()) {
		outfile << "did not reach end" << endl;
	} else {
		outfile << "did reach end" << endl;
	}
	outfile << cache_dynamic << "/" << total_blocks << " Cache Line accesses fit in cache." << endl;

	outfile << argv[1] << ",stride histogram" << endl;
	for(map<int64_t,int>::iterator it = stride_tracker.begin(); it != stride_tracker.end(); it++) {
		outfile << it->first << "," << it->second << endl;
	}
	outfile << argv[1] << ",reuse histogram" << endl;
	uint64_t total_reuse = 0;
	for(map<uint64_t,int>::iterator it = reuse_tracker.begin(); it != reuse_tracker.end(); it++) {
		outfile << it->first << "," << it->second << endl;
		total_reuse += it->second;
	}
	outfile << "never," << (cl_i - total_reuse) << endl;
	outfile << argv[1] << ",branch predictability" << endl;
	uint64_t num_multi_target_branches = 0;
	uint64_t num_branch_targets = 0;
	for(unordered_map<uint64_t, map<int64_t, int> >::iterator it = branch_predictability.begin(); it != branch_predictability.end(); it++) {
		// priority_queue<int> values;
		// for(map<int64_t, int>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
		// 	values.push(it2->second);
		// }
		if (it->second.size() > 1) {
			num_multi_target_branches += 1;
		}
		num_branch_targets += it->second.size();
	}
	uint64_t num_multi_target_jumps = 0;
	uint64_t num_jump_targets = 0;
	uint64_t num_dynamic_multi_target_jumps = 0;
	for(unordered_map<uint64_t, map<int64_t, int> >::iterator it = jump_predictability.begin(); it != jump_predictability.end(); it++) {
		if (it->second.size() > 1) {
			num_multi_target_jumps += 1;
			for (map<int64_t, int>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
				num_dynamic_multi_target_jumps += it2->second;
			}
		}
		num_jump_targets += it->second.size();
	}
	outfile << "total branch targets," << num_branch_targets << endl;
	outfile << "total jump targets," << num_jump_targets << endl;
	outfile << "total multi-target jumps," << num_multi_target_jumps << endl;
	outfile << "total static branches," << branch_predictability.size() << endl;
	outfile << "total static jumps," << jump_predictability.size() << endl;
	outfile << "total branch transitions," << transitions << endl;
	outfile << "total dynamic branches," << branches << endl;
	outfile << "total dynamic jumps," << jumps << endl;
	outfile << "total dynamic multi-target jumps," << num_dynamic_multi_target_jumps << endl;

	//print single use
	uint64_t num_single_use = 0;
	vector<uint64_t> single_use_icounts;
	map<uint64_t, uint64_t> cl_hit_histogram;
	for(unordered_map<uint64_t,uint64_t>::iterator it = iss_block_tracker.begin(); it != iss_block_tracker.end(); it++) {
		if (it->second == 1) {
			num_single_use++;
			single_use_icounts.push_back(cl_tracker[it->first]);
		}
		cl_hit_histogram[it->second] += 1;
	}
	outfile << "cache line hit histogram" << endl;
	for (map<uint64_t, uint64_t>::iterator it = cl_hit_histogram.begin(); it != cl_hit_histogram.end(); it++) {
		outfile << it->first << "," << it->second << endl;
	}
	sort(single_use_icounts.begin(), single_use_icounts.end());
	outfile << "single use icounts" << endl;
	for (uint64_t single_use : single_use_icounts) {
		outfile << single_use << endl;
	}
	outfile.close();

	return 0;
}