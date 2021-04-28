/* 046267 Computer Architecture - Spring 2020 - HW #1 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <iostream>
#include <vector>
#include <math.h>
#include <bitset>


using namespace std;


struct BTBEntry
{
public:
	uint32_t tag;
	uint32_t target;
	uint32_t actual_command; // for debuging only
};


struct BTB
{
public:
	unsigned size;
	unsigned history_size;
	unsigned tag_size;

	bool isGlobalHist;
	bool isGlobalSMTable;

	unsigned fsmState;
	int share_method;

	vector<BTBEntry> entries;

	// a vector of state machine for each entry or one vector if global
	int state_machines_vectors_count;
	vector<vector<int>> state_machines;

	int history_entries;
	vector<int> history; // will be of size 1 if global

};


/* a struct to save values calculated in 
 predict in order to reuse them for update */
struct IntermediateValues
{
	bool isNewPC;
	bool isTake;
	int entry_id;
	unsigned tag;
};


struct BTB btb;
struct IntermediateValues ivals;
int instruction_count;
int flush_count;


int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
	
	// save parameters:
	btb.size = btbSize;
	btb.history_size = historySize;
	btb.tag_size = tagSize;
	btb.fsmState = fsmState;
	btb.isGlobalHist = isGlobalHist;
	btb.isGlobalSMTable = isGlobalTable;
	btb.share_method = Shared;

	// initilaize entries:
	for(unsigned i = 0; i < btbSize; i++)
	{
		BTBEntry e;
		e.tag = e.target = 0;
		btb.entries.push_back(e);
	}

	// initialize state machines:
	btb.state_machines_vectors_count = btbSize;
	if(isGlobalTable)
		btb.state_machines_vectors_count = 1;
	for(int i = 0; i<btb.state_machines_vectors_count; i++)
	{	
		// create vector with state machine for each history:
		vector<int> sms;
		for(int j = 0; j < (1<<btb.history_size); j++)
			sms.push_back(fsmState);
		
		btb.state_machines.push_back(sms);
	}

	// initialize history:
	btb.history_entries = btbSize;
	if(!isGlobalHist) 
		btb.history_entries = 1;
	
	for(int i = 0; i < btb.history_entries; i++)
	{
		btb.history.push_back(0);
	}

	//initialize stats:
	instruction_count = 0;
	flush_count = 0;

	return 0;	
}


bool BP_predict(uint32_t pc, uint32_t *dst)
{

	// resolve entry id and tag:
	int entry_id = (pc >> 2) % btb.size;
	
	unsigned tag_start = log2(btb.size) + 2;
	unsigned mask = ((1 << btb.tag_size) - 1) << tag_start;
	unsigned tag = (pc & mask) >> tag_start;

	if(tag != btb.entries[entry_id].tag)
	{	// not in table
		// cout<<"command in table " << btb.entries[entry_id].actual_command << endl;
		// save values in order to use them in update:
		ivals.isNewPC = true;
		ivals.entry_id = entry_id;
		ivals.isTake = false;
		ivals.tag = tag;
		
		*dst = pc + 4;
		return false;
	}

	// resolve the history for this pc
	int pc_history;
	if (btb.isGlobalHist)
		pc_history = btb.history[0];
	else
		pc_history = btb.history[entry_id];
	
	// get taken/not taken predcition from state machine:
	int state;
	if (btb.isGlobalSMTable)
	{
		//resolve shared id in case of Gshare or Lshare:
		int share_id = -1;
		if(btb.share_method == 1)
		{
			unsigned mask = ((1 << btb.history_size) - 1) << 2;
			unsigned pc_share_bits = mask&pc >> 2;
			share_id = pc_share_bits ^ pc_history;
		}
		else if (btb.share_method == 2)
		{
			unsigned mask = ((1 << btb.history_size) - 1) << 16;
			unsigned pc_share_bits = mask&pc >> 16;
			share_id = pc_share_bits ^ pc_history;
		}

		if(btb.share_method == 0)
		{
			state = btb.state_machines[0][pc_history];
			// cout<<"state "<< state<<endl;
			
		}
		else
		{
			state = btb.state_machines[0][share_id];
		}
	}
	else
	{
		state = btb.state_machines[entry_id][pc_history];
	}

	bool isTake = (state > 1) ? true : false;

	// resolve destenation:
	if(isTake)
	{
		*dst = btb.entries[entry_id].target;
	}
	else
	{
		*dst = pc + 4;
	}

	// save values in order to use them in update:
	ivals.isNewPC = false;
	ivals.entry_id = entry_id;
	ivals.isTake = isTake;
	ivals.tag = tag;

	return isTake;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst)
{ 
	instruction_count ++;
	if(ivals.isTake != taken || (ivals.isTake && taken && pred_dst != targetPc))
		flush_count ++;

	if(ivals.isNewPC)
	{
		// reset relevant fields in btb
		btb.entries[ivals.entry_id].tag = ivals.tag;
		btb.entries[ivals.entry_id].target = targetPc;
		btb.entries[ivals.entry_id].actual_command = pc;

		if(!btb.isGlobalHist)
		{
			btb.history[ivals.entry_id] = 0;
		}

		if(!btb.isGlobalSMTable)
		{
			for(int j = 0; j < (1<<btb.history_size) ; j++)
				btb.state_machines[ivals.entry_id][j] = btb.fsmState;
		}
	}

	// update target destination in case it has changed
	if(taken && targetPc != pred_dst)
		btb.entries[ivals.entry_id].target = targetPc;
	
	// update history:
	int old_history;
	if(btb.isGlobalHist)
	{
		// save old history to update state machine:
		old_history = btb.history[0];

		btb.history[0] = (btb.history[0] << 1) + (int)taken;
		int mask = ((1 << btb.history_size) - 1);
		btb.history[0] = btb.history[0] & mask;
		
	}
	else
	{
		// save old history to update state machine:
		old_history = btb.history[ivals.entry_id];

		btb.history[ivals.entry_id] = (btb.history[ivals.entry_id] << 1) + (int)taken;
		int mask = ((1 << btb.history_size) - 1);
		btb.history[ivals.entry_id] = btb.history[ivals.entry_id] & mask;
	}

	// update relevant sm:
	// (yes, sm state has to be updated after initialization)
	if (btb.isGlobalSMTable)
	{
		// i know i copied from predict and i dont care
		// resolve shared id in case of Gshare or Lshare:
		int share_id = -1;
		if(btb.share_method == 1)
		{
			unsigned mask = ((1 << btb.history_size) - 1) << 2;
			unsigned pc_share_bits = mask&pc >> 2;
			share_id = pc_share_bits ^ old_history;
		}
		else if (btb.share_method == 2)
		{
			unsigned mask = ((1 << btb.history_size) - 1) << 16;
			unsigned pc_share_bits = mask&pc >> 16;
			share_id = pc_share_bits ^ old_history;
		}

		if(btb.share_method == 0)
		{
			if(taken && btb.state_machines[0][old_history] < 3)
			{
				// cout<<"sm inc"<<endl;
				btb.state_machines[0][old_history]++;
			}
			else if(!taken && btb.state_machines[0][old_history] > 0)
			{
				// cout<<"sm dec"<<endl;
				btb.state_machines[0][old_history]--;
			}		
		}
		else
		{
			if(taken && btb.state_machines[0][share_id] < 3)
				btb.state_machines[0][share_id]++;
			else if(!taken && btb.state_machines[0][share_id] > 0)
				btb.state_machines[0][share_id]--;
		}
	}
	else // local state machine
	{
		if(taken && btb.state_machines[ivals.entry_id][old_history] < 3)
		{
			btb.state_machines[ivals.entry_id][old_history] ++;
		}
		else if(!taken && btb.state_machines[ivals.entry_id][old_history] > 0)
		{
			btb.state_machines[ivals.entry_id][old_history] --;
		}
	}

	// cout << "old hist " << old_history << endl;

	// for(int v : btb.state_machines[0])
	// {
	// 	cout<<v<<endl;
	// }
	//cout << bitset<32>(btb.history[0]) << endl;
}

void BP_GetStats(SIM_stats *curStats){
	curStats->br_num = instruction_count;
	curStats->flush_num = flush_count;
	curStats->size = 5000;
}

