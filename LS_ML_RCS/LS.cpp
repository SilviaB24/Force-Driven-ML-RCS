#include "LS.h"

using namespace std;

const int numberOfFunctionType = 2;	//the number of Function types in the input FU library. 

struct Sclbld				//S&B solution
{
	int achievedLatency;				//achieved latency
	int res[numberOfFunctionType];		//count the number of FUs of each function type used in the S&B solution
	vector<int> scl;					//save the scheduled cc of each operation in the S&B solution
	vector<vector<vector<int>>> bld;	//[function type of the FU][number of the FU][operations bound to the FU]
};

struct preAllocation		//allocation
{
	int FunctionType;		//the function type
	int preNum, postNum;	//the preallocation and postallocation
	double utilizationRate;	//the average utilization rate of all FUs of the function type
};


struct SortSlack {
	bool operator()(const std::pair<int, G_Node>& a, const std::pair<int, G_Node>& b) {
		return a.second.alap < b.second.alap;
	}
};




// IMPLEMENTED BY SILVIA
struct PrioritySorting {
	bool operator()(const std::pair<int, G_Node>& a, const std::pair<int, G_Node>& b) {
		
		if (a.second.priority1 != b.second.priority1) {
            return a.second.priority1 < b.second.priority1;
        }
		
		// SILVIA'S NEW IMPROVEMENT IDEA:
		if (a.second.priority2 != b.second.priority2) {
			return a.second.priority2 < b.second.priority2;
		}

		return a.second.priority3 < b.second.priority3;
		// END OF SILVIA'S NEW IMPROVEMENT IDEA
	}
};
// END IMPLEMENTED BY SILVIA




//functions to check ASAP, ALAP, get latency constraint.
void ASAP(std::map<int, G_Node>& ops, std::vector<int>& delay);
int checkParent(G_Node* op, std::vector<int>& delay);
void ALAP(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC);
int checkChild(G_Node* op, std::vector<int>& delay, int& LC);
void getLC(int& LC, double& latency_parameter, std::map<int, G_Node>& ops, std::vector<int>& delay);



// IMPLEMENTED BY PLEASE
void calculate_first_priority(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay);
// END IMPLEMENTED BY PLEASE



// IMPLEMENTED BY SILVIA
void LS(std::map<int, int>& schlResult, std::map<int, int>& FUAllocationResult, std::map<int, std::map<int, std::vector<int>>>& bindingResult, int& actualLatency,
	std::map<int, G_Node>& ops, int& latencyConstraint, double& latencyParameter, std::vector<int>& delay, std::vector<int>& res_constr, bool improvedSolution);
void calculate_priorities(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay);
void calculate_second_priority(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay);
void calculate_third_priority(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay);
// END IMPLEMENTED BY SILVIA



// IMPLEMENTED BY SILVIA
void LS_outer_loop(std::map<int, int>& schlResult, std::map<int, int>& FUAllocationResult, std::map<int, std::map<int, std::vector<int>>>& bindingResult, int& actualLatency,
	std::map<int, G_Node>& ops, int& latencyConstraint, double& latencyParameter, std::vector<int>& delay, std::vector<int>& res_constr)
{

	int starting_high_latency = 100000;
	// Get latency constraint upper bound
	LS(schlResult, FUAllocationResult, bindingResult, actualLatency,
			ops, starting_high_latency, latencyParameter, delay, res_constr, false);
	
	int target_latency = actualLatency;
	int iteration_count = 0;

	while (iteration_count < MAX_ITERATIONS){

		iteration_count++;

		cout << "Iteration: " << iteration_count << ". Target latency: " << target_latency << endl;

		// calculate priorities

		LS(schlResult, FUAllocationResult, bindingResult, actualLatency,
			ops, target_latency, latencyParameter, delay, res_constr, true);

		target_latency = actualLatency - 1;

		if(actualLatency >= latencyConstraint){
			cout << "No better solution found. Exiting. " << endl;
			return;
		}
	}
}
// END IMPLEMENTED BY SILVIA





void LS(std::map<int, int>& schlResult, std::map<int, int>& FUAllocationResult, std::map<int, std::map<int, std::vector<int>>>& bindingResult, int& actualLatency,
	std::map<int, G_Node>& ops, int& latencyConstraint, double& latencyParameter, std::vector<int>& delay, std::vector<int>& res_constr, bool improvedSolution)
{
	ASAP(ops, delay);
	getLC(latencyConstraint, latencyParameter, ops, delay);
	ALAP(ops, delay, latencyConstraint);

	Sclbld sclbld;

	int opn = ops.size(); //# of operations in this DFG.

	vector<preAllocation> Allocation;

	//initialize the allocation structure: only Function types appearing in the DFG are considered
	for (int anOperation = 0; anOperation < opn; anOperation++)
	{
		auto pt = Allocation.begin();
		for (; pt != Allocation.end(); pt++)
			if (pt->FunctionType == ops[anOperation].type)
				break;

		//a new Function type needs to be considered in the allocation structure
		if (pt == Allocation.end())
		{


			preAllocation instance;
            instance.FunctionType = ops[anOperation].type;
            
            // IMPLEMENTED BY SILVIA
            
			// If the function type index exceeds the size of res_constr vector set preNum and postNum to 1
            if (instance.FunctionType < res_constr.size()) {
                instance.preNum = res_constr[instance.FunctionType];
                instance.postNum = res_constr[instance.FunctionType];
            } else {
                
                instance.preNum = 1; 
                instance.postNum = 1;
            }
            // END IMPLEMENTED BY SILVIA

            Allocation.push_back(instance);

		}
	}

	int currentClockCycle;
	vector<int> availableOperations;					//available non-0 slack operations in current clock cycle
	vector<vector<int>> time(numberOfFunctionType);	//time[Function type][an FU] saves the finishing cc of each allocated FU

	currentClockCycle = 1;
	sclbld.scl.assign(opn, 0);
	sclbld.bld = vector<vector<vector<int>>>(numberOfFunctionType, vector<vector<int>>(1, vector<int>(0)));

	//initialize time[][] and res[] according to pre-allocation
	for (int aFunctionType = 0; aFunctionType < numberOfFunctionType; aFunctionType++)
	{
		time[aFunctionType].clear();
		sclbld.res[aFunctionType] = 0;

		auto tpt = Allocation.begin();
		for (; tpt != Allocation.end(); tpt++)
			if (tpt->FunctionType == aFunctionType)
				break;
		if (tpt == Allocation.end())
			continue;
		else
			for (int anFU = 0; anFU < tpt->preNum; anFU++)
			{
				time[aFunctionType].push_back(0);
				sclbld.res[aFunctionType]++;
			}
	}

	//initialize allocation
	for (auto spt = Allocation.begin(); spt != Allocation.end(); spt++)
	{
		for (int i = 0; i < spt->preNum - 1; i++)	//there is one FU allocated for each Function type at the beginning
			sclbld.bld[spt->FunctionType].push_back(vector<int>(0));
		spt->postNum = spt->preNum;
		spt->utilizationRate = 0;
	}

	int numberOfScheduledOperations = 0;		//number of scheduled operations
	while (numberOfScheduledOperations != opn)	//list scheduling begins
	{
		for (int currentFunctionType = 0; currentFunctionType < numberOfFunctionType; currentFunctionType++)	//for each Function type
		{
			for (int currentOperation = 0; currentOperation < opn; currentOperation++)	//for each operation
			{
				//choose an unscheduled operation of the current Function type
				if ((ops[currentOperation].type == currentFunctionType) && (sclbld.scl[currentOperation] == 0))
				{
					//check whether the chosen operation is available (an operation is considered to be available if it has no unscheduled parents
					auto pt = ops[currentOperation].parent.begin();
					if (ops[currentOperation].parent.size() > 0)
						for (; pt != ops[currentOperation].parent.end(); pt++)
							//the parent operation has not been scheduled or its finish cc (the node has been scheduled) is greater than current cc�� which means the operation is still unavailable
							if ((sclbld.scl[(*pt)->id] == 0) || (sclbld.scl[(*pt)->id] + delay[ops[(*pt)->id].type] > currentClockCycle))
								break;
					bool operationAvailability = false;
					if (ops[currentOperation].parent.size() > 0)	//the current operation has parent(s)
					{
						if (pt == ops[currentOperation].parent.end())
							operationAvailability = true;
					}
					//the current operation has no parent, means the current operation is available for sure
					else
						operationAvailability = true;

					//if the current operation is available
					if (operationAvailability) availableOperations.push_back(currentOperation);
				}//end operation and Function type matching
			}//end each current operation

			//Schedule them to available FUs in increasing slack order
			if (!availableOperations.empty())
			{
				vector<std::pair<int, G_Node>> tempOpSet;
				tempOpSet.clear();

				for (auto it = availableOperations.begin(); it != availableOperations.end(); it++)
					tempOpSet.push_back(std::make_pair((*it), ops[*it]));



				// IMPLEMENTED BY SILVIA

				// Calculate priorities for available operations
				if (improvedSolution) {
					
					calculate_priorities(tempOpSet, ops, delay);

					//sort operations in increasing Priority order
					std::sort(tempOpSet.begin(), tempOpSet.end(), PrioritySorting());
				} else {
					//sort operations in increasing slack order
					std::sort(tempOpSet.begin(), tempOpSet.end(), SortSlack());
				}
				// END IMPLEMENTED BY SILVIA



				//schedule avaialble operations in increasing slack order and bind them to avaialble FUs
				for (auto pt = availableOperations.begin(); pt != availableOperations.end(); pt++)
					for (int k = 0; k < time[currentFunctionType].size(); k++)
						if (time[currentFunctionType][k] < currentClockCycle)
						{
							sclbld.scl[*pt] = currentClockCycle;
							numberOfScheduledOperations++;
							sclbld.bld[currentFunctionType][k].push_back(*pt);
							time[currentFunctionType][k] = currentClockCycle + delay[ops[*pt].type] - 1;
							break;
						}
			}
			availableOperations.clear();
		}//end each Function type
		currentClockCycle++;	//move to the next cc
	}//end list scheduling


	//get achieved latency of the LS iteration
	sclbld.achievedLatency = 0;
	for (int anOperation = 0; anOperation < opn; anOperation++)
		if (sclbld.achievedLatency < sclbld.scl[anOperation] + delay[ops[anOperation].type] - 1)
			sclbld.achievedLatency = sclbld.scl[anOperation] + delay[ops[anOperation].type] - 1;

	//get output results:
	//schl result
	for (auto i = 0; i < opn; i++)
		schlResult[i] = sclbld.scl[i];

	//FUAllocation result
	for (auto t = 0; t < numberOfFunctionType; t++)
		FUAllocationResult[t] = sclbld.res[t];

	//FU binding result
	for (auto t = 0; t < numberOfFunctionType; t++)
		for (auto i = 0; i < FUAllocationResult[t]; i++)
			bindingResult[t][i] = sclbld.bld[t][i];

	actualLatency = sclbld.achievedLatency;
}




void getLC(int& LC, double& latency_parameter, std::map<int, G_Node>& ops, std::vector<int>& delay)
{
	int opn = ops.size();
	LC = 0;
	//obtain ASAP latency first
	for (auto i = 0; i < opn; i++)
		if (ops[i].child.empty())
			if (ops[i].asap + delay[ops[i].type] - 1 > LC)
				LC = ops[i].asap + delay[ops[i].type] - 1;
	LC *= latency_parameter;
}

void ASAP(std::map<int, G_Node>& ops, std::vector<int>& delay)
{
	int opn = ops.size();
	queue <G_Node*> q; //queue to read/update nodes' ASAP
	for (auto i = 0; i < opn; i++)
	{
		ops[i].id = i; //initialize node id
		ops[i].asap = -1; //initialize all node's asap to -1
		ops[i].schl = false; //all nodes are not scheduled.
		if (ops[i].parent.empty()) //push all input nodes into q (no parent)
		{
			ops[i].asap = 1; //input nodes have asap = 1
			q.push(&ops[i]); //push all input nodes into q.
		}
	}
	G_Node* current = new G_Node; //temp node
	int temp = 0;
	while (!q.empty())
	{
		current = q.front(); //read the head of q
		if (current->asap > 0) //if head.asap > 0 (all visited), push all unvisited children into q.
		{
			for (auto it = current->child.begin(); it != current->child.end(); it++) //check all children and see if they can obtain ASAP and push into q
			{
				temp = checkParent(*it, delay);
				if (temp > 0) //all parent are visited (has > 0 T-asap, and then, return my Asap = max Parent Asap + d
				{
					(*it)->asap = temp; //get asap
					q.push((*it)); //push into q
				}
			}
			q.pop(); //pop the current (head node)
		}
	}

	//for (int i = 0; i < opn; i++)
		//cout << "my id: " << i << " , asap time = " << ops[i].asap << endl;


}

int checkParent(G_Node* op, std::vector<int>& delay)
{
	bool test = false;
	int myAsap = -1;

	for (auto it = op->parent.begin(); it != op->parent.end(); it++)
	{
		if ((*it)->asap > 0)
		{
			test = true;
			if ((*it)->asap + delay[(*it)->type] > myAsap)
				myAsap = (*it)->asap + delay[(*it)->type]; //my ASAP = parent.ASAP + delay
			continue;
		}
		else
		{
			test = false;
			myAsap = -1;
			break;
		}
	}
	return myAsap;
}

void ALAP(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC)
{
	int opn = ops.size();
	queue <G_Node*> q; //same as obtain ASAP:
	//push all output node into q first
	for (auto i = 0; i < opn; i++)
	{
		ops[i].alap = LC + 1; //intialize > LC
		if (ops[i].child.empty())
		{
			ops[i].alap = LC - delay[ops[i].type] + 1; //LC-Delay+1
			q.push(&ops[i]); //push into q.
		}
	}
	G_Node* current = new G_Node;
	int temp = 0;
	while (!q.empty())
	{
		current = q.front();
		if (current->alap <= LC) //less than LC, the parent ALAP may be computed
		{
			for (auto it = current->parent.begin(); it != current->parent.end(); it++)
			{
				temp = checkChild((*it), delay, LC);
				if (temp <= LC) //my ALAP has been updated
				{
					(*it)->alap = temp;
					q.push(*it);
				}
			}
			q.pop();
		}
	}
}

int checkChild(G_Node* op, std::vector<int>& delay, int& LC)
{
	bool test = false;
	int myAlap = LC + 1;
	int criticalChildId = -1;
	for (auto it = op->child.begin(); it != op->child.end(); it++)
	{
		if ((*it)->alap <= LC)
		{
			test = true;
			if ((*it)->alap - delay[op->type] <= myAlap){
				myAlap = (*it)->alap - delay[op->type];

				// IMPLEMENTED BY SILVIA
				op->criticalSuccessorId = (*it)->id;
				// END IMPLEMENTED BY SILVIA
			}
			continue;
		}
		else
		{
			test = false;
			myAlap = LC + 1;
			break;
		}
	}
	return myAlap;
}


// IMPLEMENTED BY SILVIA
void calculate_fds_graphs(std::map<int, G_Node> ops, std::vector<std::vector<float>>& fds_graphs, int target_latency, std::vector<int> delay)
{ 
	
	// print the whole content of ops for debugging
	for (const auto& [id, node] : ops) {
		std::cout << "Node ID: " << id << ", Type: " << node.type
			<< ", ASAP: " << node.asap << ", ALAP: " << node.alap << "\n";
	}

	
	for (size_t i = 0; i < fds_graphs.size(); i++) {
        std::fill(fds_graphs[i].begin(), fds_graphs[i].end(), 0.0f);
    }


	// Compute probabilities
	for (const auto& [id, node] : ops) {

		// Get node info 
		int func_type = node.type;
		int asap_time = node.asap;
		int alap_time = node.alap;

		// Get operation latency
		int op_latency = delay[func_type];

		// Calculate the available start slots (mobility window)
		int avail_start_slots = alap_time - asap_time + 1;

		// Check if there are available slots
		if (avail_start_slots <= 0) continue;
			
		// Uniform probability distribution over available slots
		float prob_per_slot = 1.0f / avail_start_slots;

		// Update fds_graphs taking into account operation latency
		// Distribution is not uniform anymore over the operation duration
		for (int s = asap_time; s <= alap_time; s++) {
            for (int t = s; t < s + op_latency; t++) {
                
				// Ensure we do not exceed target latency
                if (t <= target_latency) {
                    
                    fds_graphs[func_type][t] += prob_per_slot;
                }
			}
		}
	}
}


void calculate_priorities(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay)
{

	calculate_first_priority(available_ops, ops, delay);
	
	// SILVIA'S NEW IMPROVEMENT IDEA
	calculate_second_priority(available_ops, ops, delay);
	calculate_third_priority(available_ops, ops, delay);
	// END OF SILVIA'S NEW IMPROVEMENT IDEA
}



// SILVIA'S NEW IMPROVEMENT IDEA
void calculate_second_priority(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay)
{
	for (auto& [id, node] : available_ops) {

		// Priority 2: 
		float sum = 0.0f;

		// sum on the critical path successors
		int current_id = id;
		while (ops[current_id].criticalSuccessorId != -1) {
			int succ_id = ops[current_id].criticalSuccessorId;

			// Add square of latency to sum
			sum += pow(delay[ops[succ_id].type],2);
			current_id = succ_id;
		}

		// Store negative sum to have higher priority for higher sum values
		ops[id].priority2 = -sum;
	}

}

void calculate_third_priority(std::vector<std::pair<int, G_Node>>& available_ops, std::map<int, G_Node>& ops, std::vector<int>& delay)
{
	for (auto& [id, node] : available_ops) {

		// Priority 3: Number of immediate children (more children -> higher priority)
		// Store negative number to have higher priority for more children
		ops[id].priority3 = - ops[id].child.size();
	}

}
// END OF SILVIA'S NEW IMPROVEMENT IDEA


// END IMPLEMENTED BY SILVIA







// IMPLEMENTED BY PLEASE

// ORIGINAL PRIORITY CALCULATION FUNCTION FROM THE MIDTERM
// please's idea Probabilistic Priority Weight

void calculate_first_priority(std::vector<std::pair<int, G_Node>>& available_ops,
                              std::map<int, G_Node>& ops,
                              std::vector<int>& delay)
{
    if (available_ops.empty()) return;

    // Derive a target latency horizon from ALAP values:
    //    L_target = max_u (ALAP(u) + latency(u) - 1)
    int target_latency = 0;
    for (const auto &kv : ops) {
        const G_Node &node = kv.second;
        int func_type = node.type;
        if (func_type < 0 || func_type >= numberOfFunctionType) continue;

        int finish_latest = node.alap + delay[func_type] - 1;
        if (finish_latest > target_latency) {
            target_latency = finish_latest;
        }
    }
    if (target_latency <= 0) target_latency = 1;

    // Build FDS graphs q_k(m) using Silvia's function.
    //    fds_graphs[func_type][cycle] = expected usage of that resource in that cycle.
    std::vector<std::vector<float>> fds_graphs(
        numberOfFunctionType,
        std::vector<float>(target_latency + 2, 0.0f)
    );

    calculate_fds_graphs(ops, fds_graphs, target_latency, delay);

    // Helper: compute local congestion C_local(u) from FDS
    auto compute_C_local = [&](int opId) -> float {
        const G_Node &node = ops.at(opId);
        int func_type = node.type;

        // Ignore SOURCE/SINK or invalid types
        if (func_type < 0 || func_type >= numberOfFunctionType) return 0.0f;

        int asap = node.asap;
        int alap = std::max(node.asap, node.alap);
        int latency = delay[func_type];

        float q_max = 0.0f;

        // We consider all possible (start, active) cycles in the mobility window
        for (int s = asap; s <= alap; ++s) {
            for (int t = s; t < s + latency; ++t) {
                if (t <= 0 || t > target_latency) continue;
                q_max = std::max(q_max, fds_graphs[func_type][t]);
            }
        }
        return q_max; // not divided by resources; normalization happens later
    };

    // Compute raw S(u) and C(u) for all available operations
    std::map<int, double> rawS;
    std::map<int, double> rawC;

    double s_max = 1e-6;
    double c_max = 1e-6;

    for (auto &entry : available_ops) {
        int id = entry.first;
        const G_Node &node = ops.at(id);

        // Slack term S(u) = (mobility + 1)
        int mobility = node.alap - node.asap; // >= 0 ideally
        if (mobility < 0) mobility = 0;
        double S = static_cast<double>(mobility + 1);
        rawS[id] = S;
        if (S > s_max) s_max = S;

        // Congestion term C(u): average along critical successor chain 
        double sumC = 0.0;
        int len = 0;

        int current = id;
        std::set<int> visited; // avoid accidental loops

        while (current != -1 && !visited.count(current)) {
            visited.insert(current);

            float C_local = compute_C_local(current);
            sumC += static_cast<double>(C_local);
            ++len;

            int next = ops.at(current).criticalSuccessorId;
            if (next == current) break; // safety
            current = next;
        }

        double C = (len > 0) ? (sumC / static_cast<double>(len)) : 0.0;
        rawC[id] = C;
        if (C > c_max) c_max = C;
    }

    // Normalize and compute final priority F(u)
    const double EPS = 1e-4;
    const double ALPHA = 1.0; // exponent for S_norm
    const double BETA  = 1.0; // exponent for C_norm

    for (auto &entry : available_ops) {
        int id = entry.first;

        double s_norm = rawS[id] / s_max;
        double c_norm = (c_max > 0.0) ? (rawC[id] / c_max) : 0.0;

        // Probabilistic weighting:
        // F(u) = S_norm^ALPHA * (C_norm + EPS)^BETA
        double F = std::pow(s_norm, ALPHA) * std::pow(c_norm + EPS, BETA);

        // Write into the global ops map 
        ops[id].priority1 = F;

        // also update the local copy stored in available_ops
        // so that PrioritySorting, which compares pair.second, sees it.
        entry.second.priority1 = F;
    }
}



// END IMPLEMENTED BY PLEASE
