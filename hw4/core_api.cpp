/* 046267 Computer Architecture - Spring 2021 - HW #4 */

#include "core_api.h"
#include "sim_api.h"
#include <vector>
#include <algorithm>

#include <iostream>


using std::vector;


// predicates
bool isFalse(bool var){return !var;}
bool isNotZero(int var){return var != 0;}

struct simData{
	/* next instruction id for each thread. the value in place i in the vector
		corresponds to the instruction id in thread i */
	vector<int> inst_id;

	/* array of thread contexes each vector element is a vector of thread registers */
	vector< vector<int> > regs;

	/* true for threads that are done */
	vector<bool> is_done_threads;

	/* the number of cycles remaining to sleep for each thread.
	 0 if the thread should not sleep, -1 if thread is done */
	vector<int> sleep_cycles;

	int cycle_count;

	int inst_count;

	simData(int thread_count):
		inst_id(vector<int>(thread_count,0)),
		regs(vector< vector<int> >(thread_count, vector<int>(REGS_COUNT, 0))),
		is_done_threads(vector<bool>(thread_count, false)),
		sleep_cycles(vector<int>(thread_count,0)),
		cycle_count(0),
		inst_count(0)
	{

	};

	// return false as long as there are threads that didn't finish
	bool isDone()
	{
		for(int t_done :is_done_threads)
		{
			if(t_done == false) return false;
		}
		return true;
	}

	bool areThereThreadsNotSleeeping()
	{
		for(int s : sleep_cycles)
		{
			if(s == 0) return true;
		}
		return false;
	}

	/* one cycle has passed (no command comited)
	 (dec sleep time to sleeping threads by 1 and add cycle to count) */
	void runCycle()
	{
		cycle_count ++;
		for(int i = 0; i<SIM_GetThreadsNum(); i++)
		{
			if(sleep_cycles[i] > 0)
				sleep_cycles[i]--;		
		}
	}

	void committedInst()
	{
		runCycle();
		inst_count++;
	}
};


simData* sim_data;


void CORE_BlockedMT() 
{
	sim_data = new simData(SIM_GetThreadsNum());

	int curr_thread = 0;
	while(!sim_data->isDone())
	{
		//std::cout<<"while start";
		// check if there are threads to run if not run idle instruction
		if(!sim_data->areThereThreadsNotSleeeping())
		{
			
			sim_data->runCycle();
			continue;
		}

		// check if current thread asleep, if it is, advance to the next one available
		//  (make sure it's not done) and pay for switch
		if(sim_data->sleep_cycles[curr_thread] != 0)
		{
			// find next thread:
			while(sim_data->sleep_cycles[curr_thread] != 0)
			{
				curr_thread ++;
				curr_thread = curr_thread % SIM_GetThreadsNum();
			}

			// pay for context switch
			for(int i = 0; i<SIM_GetSwitchCycles(); i++)
			{
				sim_data->runCycle();
			}

			continue;
		}

		// commit next instruction in this thread
		Instruction inst;
		SIM_MemInstRead(sim_data->inst_id[curr_thread], &inst, curr_thread);

		sim_data->committedInst();
		sim_data->inst_id[curr_thread] ++; // we already have the instruction

		switch(inst.opcode) 
		{
			case CMD_HALT: 
			{
				sim_data->is_done_threads[curr_thread] = true;
				sim_data->sleep_cycles[curr_thread] = -1;
				break;
			} 
			case CMD_ADD:
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] + sim_data->regs[curr_thread][inst.src2_index_imm];
				break;
			}
			case CMD_SUB: 
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] - sim_data->regs[curr_thread][inst.src2_index_imm];
				break;
			} 
			case CMD_ADDI:
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] + inst.src2_index_imm;
				break;
			}
			case CMD_SUBI: 
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] - inst.src2_index_imm;
				break;
			} 
			case CMD_LOAD: 
			{
				// resolve src2:
				int src2 = sim_data->regs[curr_thread][inst.src2_index_imm];
				if(inst.isSrc2Imm)
					src2 = inst.src2_index_imm;
				
				// load:
				SIM_MemDataRead(src2 + inst.src1_index, &(sim_data->regs[curr_thread][inst.dst_index]));

				// go to sleep:
				sim_data->sleep_cycles[curr_thread] += SIM_GetLoadLat();

				break;
			} 
			case CMD_STORE: 
			{
				// resolve src2:
				int src2 = sim_data->regs[curr_thread][inst.src2_index_imm];
				if(inst.isSrc2Imm)
					src2 = inst.src2_index_imm;
				
				// store:
				SIM_MemDataWrite(src2 + sim_data->regs[curr_thread][inst.dst_index], sim_data->regs[curr_thread][inst.src1_index]);

				// go to sleep:
				sim_data->sleep_cycles[curr_thread] += SIM_GetStoreLat();

				break;
			} 
			default: break; // including CMD_NOP
		}
	}// end while

}

void CORE_FinegrainedMT() 
{	// 95% of it is a copy of BlockedMT but i'm not going to make it neat just for this exercise
	// the only difference is that we advance to the next thread in the end of the loop and we
	// don't pay for context switch.

	sim_data = new simData(SIM_GetThreadsNum());
	
	int curr_thread = 0;
	while(!sim_data->isDone())
	{
		// check if there are threads to run, if not run idle instruction
		if(!sim_data->areThereThreadsNotSleeeping())
		{
			sim_data->runCycle();
			continue;
		}

		// check if current thread is  asleep, if it is, advance to the next one available
		// (make sure it's not done)
		if(sim_data->sleep_cycles[curr_thread] != 0)
		{
			// find next thread:
			while(sim_data->sleep_cycles[curr_thread] != 0)
			{
				curr_thread ++;
				curr_thread = curr_thread % SIM_GetThreadsNum();
			}
			continue;
		}

		// commit next instruction in this thread
		Instruction inst;
		SIM_MemInstRead(sim_data->inst_id[curr_thread], &inst, curr_thread);

		sim_data->committedInst();
		sim_data->inst_id[curr_thread] ++; // we already have the instruction

		switch(inst.opcode) 
		{
			case CMD_HALT: 
			{
				sim_data->is_done_threads[curr_thread] = true;
				sim_data->sleep_cycles[curr_thread] = -1;
				break;
			} 
			case CMD_ADD:
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] + sim_data->regs[curr_thread][inst.src2_index_imm];
				break;
			}
			case CMD_SUB: 
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] - sim_data->regs[curr_thread][inst.src2_index_imm];
				break;
			} 
			case CMD_ADDI:
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] + inst.src2_index_imm;
				break;
			}
			case CMD_SUBI: 
			{
				sim_data->regs[curr_thread][inst.dst_index] = 
					sim_data->regs[curr_thread][inst.src1_index] - inst.src2_index_imm;
				break;
			} 
			case CMD_LOAD: 
			{
				// resolve src2:
				int src2 = sim_data->regs[curr_thread][inst.src2_index_imm];
				if(inst.isSrc2Imm)
					src2 = inst.src2_index_imm;
				
				// load:
				SIM_MemDataRead(src2 + inst.src1_index, &(sim_data->regs[curr_thread][inst.dst_index]));

				// go to sleep:
				sim_data->sleep_cycles[curr_thread] += SIM_GetLoadLat();

				break;
			} 
			case CMD_STORE: 
			{
				// resolve src2:
				int src2 = sim_data->regs[curr_thread][inst.src2_index_imm];
				if(inst.isSrc2Imm)
					src2 = inst.src2_index_imm;
				
				// store:
				SIM_MemDataWrite(src2 + sim_data->regs[curr_thread][inst.dst_index], sim_data->regs[curr_thread][inst.src1_index]);

				// go to sleep:
				sim_data->sleep_cycles[curr_thread] += SIM_GetStoreLat();

				break;
			} 
			default: break; // including CMD_NOP
		}
	
		// now in oppose to BlockedMT, we advance to the next thread.
		// if this thread is not available yet it is going to be fixed 
		// in the beggining of the next iteration.
		curr_thread ++;
		curr_thread = curr_thread % SIM_GetThreadsNum();

	}// end while
}

double CORE_BlockedMT_CPI(){
	double cpi = (double)sim_data->cycle_count / (double)sim_data->inst_count;
	delete sim_data;
	return cpi;
}

double CORE_FinegrainedMT_CPI()
{
	double cpi = (double)sim_data->cycle_count / (double)sim_data->inst_count;
	delete sim_data;
	return cpi;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) 
{
	for(int i = 0; i < REGS_COUNT; i++)
	{
		context[threadid].reg[i] = sim_data->regs[threadid][i];
	}
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) 
{
	for(int i = 0; i < REGS_COUNT; i++)
		context[threadid].reg[i] = sim_data->regs[threadid][i];
}
