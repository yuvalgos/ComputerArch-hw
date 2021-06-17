/* 046267 Computer Architecture - Spring 21 - HW #3               */
/* Implementation (skeleton)  for the dataflow statistics calculator */

#include "dflow_calc.h"
#include <vector>
#include <list>
#include <queue>

using namespace std;

/* edge is represented as pair where first is 'target'
  instruction(vertex) and second is weight */
typedef pair<int, int> Edge;


struct InstGraph
{
    /* Graph is represented as adjacency lists where a pair (v,w)
      in list u represents edge from u to v with weight w.
      vertice n is entry and vertice n+1 is exit */
    int n_instructions;
    int v_entry;
    int v_exit;

    vector<list<Edge>> adj_list;

    InstGraph(int n_instructions): n_instructions(n_instructions), 
        v_entry(n_instructions), 
        v_exit(n_instructions+1)
    {
        for(int i = 0; i<=v_exit; i++)
        adj_list.push_back(list<Edge>());
    }

    void insertEdge(int source, int dest, int weight, bool is_front)
    {
        /* inserting minus weight because later we want to 
        find the longest path and not the shortest one */
        if(is_front)
            adj_list[source].push_front(make_pair(dest, -weight));
        else
            adj_list[source].push_back(make_pair(dest, -weight));
    }

    // return the weight of the longest path from source to dest
    int findLongestPath(int source, int dest)
    {
        const int inf = 1000000000;
        /* implements dijkastras algorithm to find the shortest path 
           from source to all, then we will take dest.
           the dijastras implementation is from the internet.
           since wights are negative it finds the longest one */
        priority_queue< Edge, vector<Edge>, greater<Edge> > pq;
        vector<int> distances(n_instructions+2, inf);

        pq.push(make_pair(0,source));
        distances[source] = 0;

        while(!pq.empty())
        {
            int u = pq.top().second;
            pq.pop();

            list< pair<int, int> >::iterator i;
            for (i = adj_list[u].begin(); i != adj_list[u].end(); i++)
            {
                int v = (*i).first;
                int weight = (*i).second;
    
                if (distances[v] > distances[u] + weight)
                {
                    distances[v] = distances[u] + weight;
                    pq.push(make_pair(distances[v], v));
                }
            }
        }

        // distance is negative
        return -distances[dest];
    }
};


ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts)
{
    InstGraph* inst_graph = new InstGraph(numOfInsts);

    // source vertices must have edge from exit to them later.
    vector<bool> is_source(numOfInsts, true); 

    // this vector keeps record of which instruction is the last to change each register:
    vector<int> reg_record(32, -1);

    // build dependency graph. iterate over each program oporation and check
    // if source register is written to in an earlier instrucion. 
    for(int inst = 0; inst < (int)numOfInsts; inst++)
    {
        // dependency of src1 is pushed in the front and of source 2 in the back
        int src1_idx_record = reg_record[progTrace[inst].src1Idx];
        if(src1_idx_record != -1)
        {
            int weight = (int)opsLatency[progTrace[src1_idx_record].opcode];
            inst_graph->insertEdge(inst, src1_idx_record, weight, true);
            is_source[src1_idx_record] = false;
        }
        else
        {
            inst_graph->insertEdge(inst, inst_graph->v_entry, 0, true);
        }

        int src2_idx_record = reg_record[progTrace[inst].src2Idx];
        if(src2_idx_record != -1)
        {
            int weight = (int)opsLatency[progTrace[src2_idx_record].opcode];
            inst_graph->insertEdge(inst, src2_idx_record, weight, false);
            is_source[src2_idx_record] = false;
        }
        else
        {
            inst_graph->insertEdge(inst, inst_graph->v_entry, 0, false);
        }

        reg_record[progTrace[inst].dstIdx] = inst;
    }

    // add edges to entry and from exit for relevant vertices:
    for(int inst = 0; inst<(int)numOfInsts; inst++)
    {
        if(is_source[inst])
        {
            int weight = (int)opsLatency[progTrace[inst].opcode];
            inst_graph->insertEdge(inst_graph->v_exit, inst, weight, false);
        }
        // if(inst_graph->adj_list[inst].empty())
        // {
        //     inst_graph->insertEdge(inst, inst_graph->v_entry, 0, false);
        // }  
    }

    return inst_graph;
}

void freeProgCtx(ProgCtx ctx) 
{
    InstGraph* inst_graph = (InstGraph*)ctx;
    delete inst_graph;
}

int getInstDepth(ProgCtx ctx, unsigned int theInst) 
{
    InstGraph* inst_graph = (InstGraph*)ctx;
    if((int)theInst > inst_graph->n_instructions || theInst < 0)
        return -1;
    return inst_graph->findLongestPath(theInst, inst_graph->v_entry);
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {
    InstGraph* inst_graph = (InstGraph*)ctx;

    if((int)theInst > inst_graph->n_instructions || theInst < 0)
        return -1;

    // handle src 1 dependency (its in the front of the list)
    if(inst_graph->adj_list[theInst].front().first == inst_graph->v_entry)
        *src1DepInst = -1;
    else
        *src1DepInst = inst_graph->adj_list[theInst].front().first;
    
    // handle src 2 dependency (its in the back of the list)
    if(inst_graph->adj_list[theInst].back().first == inst_graph->v_entry)
        *src2DepInst = -1;
    else
        *src2DepInst = inst_graph->adj_list[theInst].back().first;
        
    return 0;
}

int getProgDepth(ProgCtx ctx) {
    InstGraph* inst_graph = (InstGraph*)ctx;
    return inst_graph->findLongestPath(inst_graph->v_exit, inst_graph->v_entry);;
}


