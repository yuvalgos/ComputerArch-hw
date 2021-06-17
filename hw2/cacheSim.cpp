/* 046267 Computer Architecture - Spring 2021 - HW #2 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include "cache.cpp"

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;


// #define PRINT_DEBUG


int main(int argc, char **argv) {

	if (argc < 19) {
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;
	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}

	// initialize variables:
	_Cache l1 = _Cache(BSize, L1Size, L1Assoc, WrAlloc);
	_Cache l2 = _Cache(BSize, L2Size, L2Assoc, WrAlloc);
	int cycle_count = 0;
	int inst_count = 0;

	while (getline(file, line)) {

		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		#ifdef PRINT_DEBUG
			cout << "operation: " << operation;
		#endif

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		#ifdef PRINT_DEBUG
			cout << ", address (hex)" << cutAddress;
		#endif

		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);

		#ifdef PRINT_DEBUG
			cout << " (dec) " << num << endl;
		#endif

		unsigned long int bsize_log2 = BSize;
		unsigned long int full_address = num;

		unsigned long mask = 0xffffffffffffffff;
		mask = (mask << bsize_log2);
		unsigned long int block = ((full_address & mask) >> bsize_log2);


		// cout << "full address " << bitset<64>(full_address) << endl;
   		// cout << "block " << bitset<64>(block) << endl;
		#ifdef PRINT_DEBUG
			cout << "(block dec) " << block << endl;
		#endif
		// access l1 time:
		cycle_count += L1Cyc;
		// access l1, if hit lru is updated there
		if(!l1.AccessTry(block, operation))
		{
			// l1 miss
			cycle_count += L2Cyc;

			if(!l2.AccessTry(block, operation))
			{
				// l2 miss
				cycle_count += MemCyc;

				// if read or write allocate, insert to l2 
				//and evacuate sames block from l1 if needed:
				if(operation == 'r' || WrAlloc == 1)
				{	
					unsigned long int evacuated_block;
					bool not_used;
					if(l2.Insert(block, operation, &evacuated_block, &not_used))
						l1.Evacuate(evacuated_block);
				}
			}

			/* insert to l1 if needed,
			if evacuated block is dirty it has to be promoted in the lru
			mechanism in l2 so we use AccessTry on l2 to make it happen*/ 
			if(operation == 'r' || WrAlloc == 1)
			{
				unsigned long int evacuated_block;
				bool is_evacuated_dirty;
				if(l1.Insert(block, operation, &evacuated_block, &is_evacuated_dirty))
					l2.AccessTry(evacuated_block, 'w', false);
				
			}

		}

		inst_count ++;
	}

	double L1MissRate = l1.getMissRate();
	double L2MissRate = l2.getMissRate();
	double avgAccTime = double(cycle_count) / double(inst_count);

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}
