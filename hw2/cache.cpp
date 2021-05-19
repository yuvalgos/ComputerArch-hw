#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <iterator>

using namespace std;

// #ifndef PRINT_DEBUG
//     #define PRINT_DEBUG
// #endif

class _Cache
{
    int block_size_log2;
    int num_entries;
    int asso_log2;
    int ways;
    int num_lines; // each line contatins asso ways (entries)
    bool is_write_alloc;

    /* each element in the vector is one "line",
     each list element is one way,
     value is "tag", which is block id (first part of the address).
     the list is sorted by access time, front is most recent. 
     list may contain less then ways values */
    vector< list<unsigned long int> > data;

    /* those are dirty bits for the above vector of data, they are maintained the same
        way. they added as a patch due to unclear instruction and i'm not going to spend
        two hours changing everything */
    vector< list<bool> > dirty;

    int hit_count;
    int miss_count;

public:
    _Cache(int block_size_log2, int cache_size_log2, int asso_log2, bool is_write_alloc):
        block_size_log2(block_size_log2),
        num_entries( 1<<(cache_size_log2 - block_size_log2) ),
        asso_log2(asso_log2),
        ways( 1<<asso_log2 ),
        num_lines( num_entries>>asso_log2 ),
        is_write_alloc(is_write_alloc),
        hit_count(0),
        miss_count(0)
    {
        // create cache lines:
        for(int i = 0; i< num_lines; i++)
        {
            //create list of ways
            std::list<unsigned long int> ways_list;
            data.push_back(ways_list);

            std::list<bool> dirty_list;
            dirty.push_back(dirty_list);
        }
    }

    /* AccessTry gets block and returns if it is in the cache. 
        also updates miss/hit count and dirty bit if op == 'w'*/
    bool AccessTry(unsigned long int block, char op, bool count_if_hit=true)
    {
        bool hit = false;
        // check if block exist in cache (return value)
        for(int i =0; i<num_lines; i++)
        {   
            #ifdef PRINT_DEBUG
                cout << "set " << std::to_string(i) << ": ";
                for (auto const &j: data[i]) {
                    cout << j << ", ";
                }
                cout << endl;
            #endif

            // check if in current line:
            auto it = std::find(data[i].begin(), data[i].end(), block);
            if(it !=  data[i].end())
            {
                // hit!
                hit = true;
                if(count_if_hit) hit_count ++;

                // move to the front of the list as it was the most recently used
                data[i].splice(data[i].begin(), data[i], it);

                // do the same to dirty bit and update it if needed:
                int index = 0;
                for(;it!=data[i].begin();it--)
                    index++;

                auto it_dirty = dirty[i].begin(); 
                for(int j=0;j<index;j++) it_dirty++;

                if(op == 'w')
                    *it_dirty = true;
                dirty[i].splice(dirty[i].begin(), dirty[i], it_dirty);

                break;
            }
        }
        
        if(!hit) // it was not found in the loop
        {
            miss_count++;
            #ifdef PRINT_DEBUG
                cout << "miss " << miss_count << endl;
            #endif
        }

        return hit;
    }

     /* Insert inserts given block into the cache and returns true if a block is evacuated.
      evacuated block is returned in the argument evacuated, is_evacuated_dirty is set to true if
      the evacuated block was dirty. evacuation is done in lru policy */
    bool Insert(unsigned long int block, char op, unsigned long int *evacuated, bool *is_evacuated_dirty)
    {
        unsigned long int set = block % num_lines;
        bool is_evacuated = false;

        // if all ways are full, evacuate the last one:
        if(data[set].size() >= (unsigned long int)ways)  // casted ways to avoid warning
        {   
            *evacuated = data[set].back();
            data[set].pop_back();
            is_evacuated = true;

            // same for dirty bit
            *is_evacuated_dirty = dirty[set].back();
            dirty[set].pop_back();
        }

        // insert the new block at the front
        data[set].push_front(block);

        // insert the correct dirty bit
        if(op == 'w')
            dirty[set].push_front(true);
        else
            dirty[set].push_front(false);

        //cout << "inserted to set " << set << endl;

        return is_evacuated;
    }

    /* Evacuate gets block and removes it from the cache.
        it is used when lower level cache evacuates in otder to maintain inclusion */
    void Evacuate(unsigned long int block)
    {
         for(int i =0; i<num_lines; i++)
        {
            data[i].remove(block);
        }
    }

    double getMissRate()
    {
        return double(miss_count) / (double(hit_count + miss_count)); 
    }

};

