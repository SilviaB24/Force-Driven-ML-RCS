//The original code is created by Huan Ren. Last modified by Owen and Keivn.
//"lookahead" approach was not discussed in the class but is used in this code, for forces calculation. please refer to the FDS paper section IV: E


// MODIFIED BY SILVIA, PLEASE

#include "LS.h"

#include <queue>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

extern int DFG;
extern int LC;
extern int opn;
extern double latencyParameter;

void output_schedule(string str, std::map<int, G_Node>& ops);
void FDS(std::map<int, G_Node>& ops, std::vector<int>& delay, int& , double& latency_parameter, int tnum, bool debug);
void getLCFDS(int& LC, double& latency_parameter, std::map<int, G_Node>& ops, std::vector<int>& delay);
int checkParentFDS(G_Node* op, std::vector<int>& delay);
void ALAPFDS(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC);
void ASAPFDS(std::map<int, G_Node>& ops, std::vector<int>& delay);
int checkChildFDS(G_Node* op, std::vector<int>& delay, int& LC);
void update_depth(std::map<int, int>& op_depth, vector<int>& topo_order, std::map<int, G_Node>& ops);
void get_pr_su_update_list(std::map<int, G_Node>& ops, std::map<int, std::vector<int>>& ops_update_pr_list,
	std::map<int, std::vector<int>>& ops_update_su_list,
	std::map<int, int>& op_depth,
	int& depth_limit);



void FDS_Outer_Loop(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC,
        double& latencyParameter, std::vector<int>& res_constr, bool debug) {

	int tnum = delay.size();

    bool feasible = false;
    int iteration = 0;

	// Loop until a feasible solution is found or max iterations reached
    while (!feasible && iteration < 200) { 
        iteration++;

        // Run standard FDS scheduling
        FDS(ops, delay, LC, latencyParameter, tnum, debug);

        // 2. Calcola l'uso effettivo di risorse per ogni ciclo di clock
        std::vector<int> max_used(tnum, 0); // Massima risorsa usata per tipo.
        
        // La schedule termina al tempo massimo di fine operazione.
        int actual_max_latency = 0;
        for (auto& [_, node] : ops) {
            int finish_time = node.asap + delay[node.type] - 1;
            if (finish_time > actual_max_latency) {
                actual_max_latency = finish_time;
            }
        }

        // Vettore per tracciare l'uso di risorse per ciclo di clock: usage[Type][CC]
        std::vector<std::vector<int>> usage(tnum, std::vector<int>(actual_max_latency + 1, 0)); 
        
        for (auto& [_, node] : ops) {
            if (node.asap > 0) { // Se l'operazione è stata schedulata (FDS fallisce se non schedula)
                // L'operazione inizia in node.asap e usa 1 risorsa di tipo node.type
                usage[node.type][node.asap]++;
            }
        }

        // 3. Verifica la fattibilità: controlla se l'uso massimo supera il vincolo di risorsa
        feasible = true;
        for (size_t type = 0; type < tnum; ++type) {
            // Controlla l'uso in tutti i cicli di clock
            for (int cc = 1; cc <= actual_max_latency; ++cc) {
                if (usage[type][cc] > res_constr[type]) {
                    feasible = false;
                    break;
                }
            }
            if (!feasible) break; // Non appena un vincolo è violato, interrompi
        }
        
        // 4. Se non fattibile, aumenta LC e riprova
        if (!feasible) {
            // Aumenta LC di un valore ragionevole.
            // Il tuo codice usa LC += 3, che è un incremento aggressivo ma valido.
            LC += 3;

			if (debug)
            	std::cout << "[ML_RCS] Violated constraints, increasing LC to "
                      << LC << " (iteration " << iteration << ")\n";
        }
    }

    if (!feasible)
		if (debug)
        	std::cout << "[ML_RCS] WARNING: did not reach a feasible solution after "
                  << iteration << " iterations.\n";
}



void output_schedule(string str, std::map<int, G_Node>& ops)
{
	//obtain filename to output

	ofstream fout_s(str, ios::out | ios::app);	//output file to save the scheduling results
	fout_s << "LC " << LC << endl;
	for (int i = 0; i < opn; i++) {
		//std::cout << i << " " << ops[i].asap << endl;	//after scheduling, ASAP = ALAP of each node
		fout_s << i << " " << ops[i].asap << endl;
	}
	//fout_s << "*********************************************" << endl;
	fout_s.close();
	fout_s.clear();
}

//---------------------------------------//
//----------------FDS--------------------//
//---------------------------------------//
void FDS(std::map<int, G_Node>& ops, std::vector<int>& delay, int& , double& latency_parameter, int tnum, bool debug) 
{
	//find latency constraint
	//Obtain ASAP latency first
	ASAPFDS(ops,delay); //Obtain ASAP for each operation
	getLCFDS(LC, latency_parameter,ops, delay);
	ALAPFDS(ops, delay, LC); //Obtain ALAP for each operation

	//start FDS
	//initialize DG by tnum X (LC+1) note that, starts from cc = 0 to LC, but we don't do compuation in row cc = 0.
	vector<vector<double>> DG(tnum, vector<double>(LC + 1, 0)); //DG[TYPE][CC]
	double bestForce = 0.0; //best scheduling force value
	int bestNode = -1, bestT = -1, iteration = 0, temp1; // best Node ID and T (cc), # of iteration; 
	double temp = 0.0, force = 0.0, newP = 0.0, oldP = 0.0;	//newP/oldP for the compuataion of force

	std::map<int, int> op_depth;

	vector<int> topo_order;
	topo_order.clear();

	update_depth(op_depth, topo_order, ops);

	//int prev_schl_op = -1;

	std::map<int, vector<int>> depth_ops;
	depth_ops.clear();

	int max_d = 0;
	for (auto it = op_depth.begin(); it != op_depth.end(); it++)
		if (max_d < it->second)
			max_d = it->second;

	for (auto i = 0; i <= max_d; i++) {
		vector<int> temp;
		temp.clear();

		depth_ops[i] = temp;
	}

	for (auto it = op_depth.begin(); it != op_depth.end(); it++)
		depth_ops[it->second].push_back(it->first);

	//get reverse topo-order.
	vector<int> reverse_topo;
	reverse_topo.clear();

	for (auto i = max_d; i >= 0; i--)
		if (depth_ops[i].size() > 0)
			for (auto it = depth_ops[i].begin(); it != depth_ops[i].end(); it++)
				reverse_topo.push_back(*it);


	std::map<int, std::vector<int>> ops_update_pr_list, ops_update_su_list;

	//get depth of operations will be udated
	//using max_d or other values //max_d = update all.
	int depth_limit = 1;

	get_pr_su_update_list(ops, ops_update_pr_list, ops_update_su_list,
		op_depth, depth_limit);


	//example of initial ASAP update

	for (auto it = topo_order.begin(); it != topo_order.end(); it++) {

		int w = *it;

		if (ops[w].schl)
			continue;

		if (ops[w].parent.empty())
			ops[w].asap = 1;
		else {

			int max_asap = 1;

			for (auto pr = ops[w].parent.begin(); pr != ops[w].parent.end(); pr++) {

				int pr_type = (*pr)->type;
				int pr_delay = delay[pr_type];
				int pr_asap = (*pr)->asap;

				if (max_asap <= pr_asap + pr_delay)
					max_asap = pr_asap + pr_delay;
			}

			ops[w].asap = max_asap;
		}
	}

	//initial ALAP update

	for (auto it = reverse_topo.begin(); it != reverse_topo.end(); it++) {
		int w = *it;

		if (ops[w].schl)
			continue;

		if (ops[w].child.empty())
			ops[w].alap = LC - delay[ops[w].type] + 1;
		else {
			int min_alap = LC - delay[ops[w].type] + 1;

			for (auto su = ops[w].child.begin(); su != ops[w].child.end(); su++) {

				int su_type = (*su)->type;
				int su_alap = (*su)->alap;

				if (min_alap >= su_alap - delay[ops[w].type])
					min_alap = su_alap - delay[ops[w].type];
			}

			ops[w].alap = min_alap;
		}
	}


	while (1) //outer loop
	{
		if (iteration != 0) //starting from second iteration, update node's ASAP/ALAP first.
		{
			for (auto i = 0; i < opn; i++)
				if (ops[i].asap == ops[i].alap && !ops[i].schl) {
					// ops[i].prev_alap = ops[i].alap;
					// ops[i].prev_asap = ops[i].asap;
				}

			//intermediate ASAP update.
			for (auto it = topo_order.begin(); it != topo_order.end(); it++) {

				int w = *it;

				if (ops[w].schl)
					continue;

				if (ops[w].parent.empty())
					ops[w].asap = 1;
				else {

					int max_asap = 1;

					for (auto pr = ops[w].parent.begin(); pr != ops[w].parent.end(); pr++) {

						int pr_type = (*pr)->type;
						int pr_delay = delay[pr_type];
						int pr_asap = (*pr)->asap;

						if (max_asap <= pr_asap + pr_delay)
							max_asap = pr_asap + pr_delay;
					}

					ops[w].asap = max_asap;
				}
			}


			//intermediate ALAP update.
			for (auto it = reverse_topo.begin(); it != reverse_topo.end(); it++) {
				int w = *it;

				if (ops[w].schl)
					continue;

				if (ops[w].child.empty())
					ops[w].alap = LC - delay[ops[w].type] + 1;
				else {
					int min_alap = LC - delay[ops[w].type] + 1;

					for (auto su = ops[w].child.begin(); su != ops[w].child.end(); su++) {

						int su_type = (*su)->type;
						int su_alap = (*su)->alap;

						if (min_alap >= su_alap - delay[ops[w].type])
							min_alap = su_alap - delay[ops[w].type];
					}

					ops[w].alap = min_alap;
				}
			}
		}

		//generate DG
		for (auto i = 0; i < opn; i++) //for each node
		{   //if node has asap = alap and not be scheduled, schedule it directly (only 1 available cc)
			if (ops[i].asap == ops[i].alap && !ops[i].schl)
				ops[i].schl = true;
			temp = 1.0 / double(ops[i].alap - ops[i].asap + 1); //set temp = scheduling probability = 1/(# of event), to be fast computed.
			for (auto t = ops[i].asap; t <= ops[i].alap; t++) //asap to alap cc range,
				for (auto d = 0; d < delay[ops[i].type]; d++) //delay
					DG[ops[i].type][t + d] += temp; //compute DG
		}//end DG generation


		/*for (auto i = 0; i < tnum; i++)
			for (auto j = 1; j <= LC; j++)
			{
				if (DG[i][j] != 0)
					cout << "DG type = " << i << " , on cc " << j << " : " << DG[i][j] << endl;
			}*/

			//start inner loop:
			//initialize best node parameter;
		bestForce = 0.0;
		bestNode = bestT = -1;

		for (auto n = 0; n < opn; n++) //check all unscheduled node
		{
			if (ops[n].schl)
				continue;
			for (auto t = ops[n].asap; t <= ops[n].alap; t++) //check all cc (all event) in MR of n [asap, alap]
			{

				//Note HERE: You may need to use the intermediate ASAP/ALAP update for accurate tentative MR update for all unscheduled predecessors and successors.
				// 
				//Below is the force computation. Note that, for predecessors and successors below, this version (for highest efficiency) is computed based on the "self" operation u's tentative scheduling (cc t) change.

				force = 0.0; //initialize temp force value	
				temp = 1.0 / double(ops[n].alap - ops[n].asap + 1); // old event probability = 1/temp1
				temp1 = ops[n].alap - ops[n].asap + 1; // # of old events

				//self force: self = sum across MR { -(deltaP) * (DG + 1/3 * deltaP) };				
				for (auto cc = ops[n].asap; cc <= ops[n].alap; cc++)
					if (cc == t) // @temp scheduling cc t
						for (auto d = 0; d < delay[ops[n].type]; d++) //across multi-delay
							force += -(1.0 - temp) * (DG[ops[n].type][cc + d] + 1.0 / 3.0 * (1.0 - temp));
					else
						for (auto d = 0; d < delay[ops[n].type]; d++) //across multi-delay
							force += temp * (DG[ops[n].type][cc + d] - 1.0 / 3.0 * temp);
				//p-s force:
				//Predecessors: only affect the P(n) alap: 
				newP = 0.0;
				oldP = 0.0;
				for (auto it = ops[n].parent.begin(); it != ops[n].parent.end(); it++)
				{
					if ((*it)->schl)
						continue;
					oldP = double((*it)->alap - (*it)->asap + 1); //temp is the oldP
					newP = double(oldP - (temp1 - (t - ops[n].asap + 1))); //newP = oldP - [(n's oldP) - (t - n's ASAP + 1)]
					temp = 1.0 / newP - 1.0 / oldP;
					for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++)
						if (cc <= t - delay[(*it)->type])
							for (auto d = 0; d < delay[(*it)->type]; d++)
								force += -(DG[(*it)->type][cc + d] + temp / 3.0) * temp;
						else
							for (auto d = 0; d < delay[(*it)->type]; d++)
								force += (DG[(*it)->type][cc + d] - 1.0 / 3.0 / oldP) / oldP;
				}
				//Successors: only affect the S(n) asap:
				newP = 0.0;
				oldP = 0.0;
				for (auto it = ops[n].child.begin(); it != ops[n].child.end(); it++)
				{
					if ((*it)->schl)
						continue;
					oldP = double((*it)->alap - (*it)->asap + 1); //temp is the oldP
					newP = double(oldP - (temp1 - (ops[n].alap - t + 1))); //newP = oldP - [(n's oldP) - (n's ALAP -t + 1)]
					temp = 1.0 / newP - 1.0 / oldP;
					for (auto cc = (*it)->asap; cc <= (*it)->alap; cc++)
						if (cc >= t - delay[(*it)->type])
							for (auto d = 0; d < delay[(*it)->type]; d++)
								force += -(DG[(*it)->type][cc + d] + temp / 3.0) * temp;
						else
							for (auto d = 0; d < delay[(*it)->type]; d++)
								force += (DG[(*it)->type][cc + d] - 1.0 / 3.0 / oldP) / oldP;
				}

				//Note, this version does not have a good tie-breaking consideration when multiple (operation, scheduling) options have the same cost;
				//The default one is based on the operation ID (smallest) and clock cycles (smallest)

				if (bestT < 0) //update best node, cc, force value
				{
					bestForce = force;
					bestNode = n;
					bestT = t;
				}
				else if (force > bestT)
				{
					bestForce = force;
					bestNode = n;
					bestT = t;
				}
			} // end MR
		}// end one operation inner loop
		//schedule the best node
		if (bestT < 0) //when all nodes has been scheduled, bestT = -1 (not changed) and break the while to stop the process
			break;
		ops[bestNode].asap = ops[bestNode].alap = bestT;
		ops[bestNode].schl = true;
		iteration++;
	}// end FDS-outer loop

	DG.clear();
}//end FDS function




void getLCFDS(int& LC, double& latency_parameter, std::map<int, G_Node>& ops, std::vector<int>& delay)
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

void ASAPFDS(std::map<int, G_Node>& ops, std::vector<int>& delay)
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
				temp = checkParentFDS(*it, delay);
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

int checkParentFDS(G_Node* op, std::vector<int>& delay)
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

void ALAPFDS(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC)
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
				temp = checkChildFDS((*it), delay, LC);
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

int checkChildFDS(G_Node* op, std::vector<int>& delay, int& LC)
{
	bool test = false;
	int myAlap = LC + 1;
	for (auto it = op->child.begin(); it != op->child.end(); it++)
	{
		if ((*it)->alap <= LC)
		{
			test = true;
			if ((*it)->alap - delay[op->type] <= myAlap)
				myAlap = (*it)->alap - delay[op->type];
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

void update_depth(std::map<int, int>& op_depth, vector<int>& topo_order, std::map<int, G_Node>& ops)
{

	vector<vector<int>> G;
	G.clear();

	vector<int> inDegree;
	inDegree.clear();

	for (auto i = 0; i < opn; i++) {

		inDegree.push_back(ops[i].parent.size());

		vector<int> temp;
		for (auto su = ops[i].child.begin(); su != ops[i].child.end(); su++) {

			temp.push_back((*su)->id);
		}

		G.push_back(temp);
	}

	int num = 0;

	queue<int> q;

	topo_order.clear();

	for (int i = 0; i < opn; i++)
		if (inDegree[i] == 0) {
			q.push(i);
			topo_order.push_back(i);
		}


	while (!q.empty()) {

		int u = q.front();

		q.pop();

		for (int i = 0; i < G[u].size(); i++) {
			int v = G[u][i];
			inDegree[v]--;
			if (inDegree[v] == 0) {
				q.push(v);
				topo_order.push_back(v);
			}

		}

		G[u].clear();
		num++;
	}

	//cout << " Topo order = ";

	std::map<int, int> curr_depth;
	curr_depth.clear();

	for (auto i = 0; i < opn; i++) {
		curr_depth[i] = -1;
	}

	for (auto i = topo_order.begin(); i != topo_order.end(); i++) {

		//cout << *i << " ";

		if (ops[*i].parent.size() == 0)
			curr_depth[*i] = 1;

		else {

			int temp_max_depth = 0;

			for (auto pr = ops[*i].parent.begin(); pr != ops[*i].parent.end(); pr++) {

				int id = (*pr)->id;

				if (temp_max_depth < curr_depth[id])
					temp_max_depth = curr_depth[id];
			}

			curr_depth[*i] = temp_max_depth + 1;
		}

		//	cout << "i = " << *i << " depth = " << curr_depth[*i] << endl;
	}
	//cout << endl;


	op_depth = curr_depth;
}

void get_pr_su_update_list(std::map<int, G_Node>& ops, std::map<int, std::vector<int>>& ops_update_pr_list,
	std::map<int, std::vector<int>>& ops_update_su_list,
	std::map<int, int>& op_depth,
	int& depth_limit)
{
	ops_update_pr_list.clear();
	ops_update_su_list.clear();

	//depth_limit = max_depth means no limit.
	//depth_limit > 0 means only update to certain depths.

	//obtaining update list:

	int curr_depth = 0;

	if (depth_limit > 0)
	{
		//for each oper:
		for (auto i = 0; i < opn; i++)
		{
			//get my depth first.
			int i_depth = op_depth[i];

			//init current evaluated depth;
			curr_depth = 0;

			vector<int> pr_list, su_list;
			pr_list.clear();
			su_list.clear();

			//obtain list for fanin
			//do a bottom-up scan (BFS), only adding pr's into list by decrementing depth = 1 each time.

			//initialize pr_q:
			std::queue<int> pr_q;
			while (!pr_q.empty())
				pr_q.pop();
			//adding i into the q as the header.
			pr_q.push(i);

			//start processing
			/*
				recursively scan pr's of the header, only adding ops w/ depth = current_depth+1 from i
			*/
			while (!pr_q.empty())
			{
				//get header:
				int header = pr_q.front();
				int header_depth = op_depth[header];

				//header != i, do the header checking process.
				//first, check the header arrival the depth. if yes, skip this header, otherwise, checking and adding pr of header into the list.
				if (header != i)
					if ((i_depth - header_depth) == depth_limit)
					{
						pr_q.pop();
						continue;
					}

				//otherwise, process the header:
				//scan header's pr list.
				//adding pr into q and vector only if pr's depth = header's depth - 1.
				for (auto pr = ops[header].parent.begin(); pr != ops[header].parent.end(); pr++)
				{
					//get pr_id;
					int pr_id = (*pr)->id;

					//if header_depth - pr_depth = 1, add pr to both pr_q and vector.
					if (header_depth - op_depth[pr_id] == 1)
					{
						pr_list.push_back(pr_id);
						pr_q.push(pr_id);
					}
				}

				//after process header, pop it.
				pr_q.pop();
			}

			//obtain list for fanout

			//do su direction:

			//first adding i into q.
			pr_q.push(i);

			//start process:
			while (!pr_q.empty())
			{
				//get header:
				int header = pr_q.front();
				int header_depth = op_depth[header];

				//header != i, do the header checking process.
				//first, check the header arrival the depth. if yes, skip this header, otherwise, checking and adding su of header into the list.
				if (header != i)
					if (header_depth - i_depth == depth_limit)
					{
						pr_q.pop();
						continue;
					}

				//otherwise, process the header:
				//scan header's su list.
				//adding su into q and vector only if su's depth = header's depth + 1. //abs-difference = 1.
				for (auto su = ops[header].child.begin(); su != ops[header].child.end(); su++)
				{
					//get su_id;
					int su_id = (*su)->id;

					//if su_depth - header_depth = 1, add su to both pr_q and vector.
					if ((op_depth[su_id] - header_depth) == 1)
					{
						su_list.push_back(su_id);
						pr_q.push(su_id);
					}
				}

				//after process header, pop it.
				pr_q.pop();
			}

			ops_update_su_list[i] = su_list;
			ops_update_pr_list[i] = pr_list;
		}
	}




}