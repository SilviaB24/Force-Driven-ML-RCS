/*
*	0-1 ILP for basic MR-LCS
*/

#include "ilp.h"

using std::string;
using std::ofstream;
using namespace std;

const int tnum = 2;	//number of operation type
double latency_parameter = 1.5; //latency constant parameter, change this parameter can affect the latency constraint and hence, the final scheduling result is changed
int DFG = 0; //DFG ID, this is automatically run from range 1 to 11
int LC = 0; //global parameter latency constraint. LC = latency_parameter * ASAP latency.
int opn = 0; //# of operations in current DFG


//0-1ILP functions.
void defineV(std::map<int, G_Node>& ops, int tnum, string filename); //all variables
void objF(std::map<int, G_Node>& ops, int tnum, string filename);	//objective function
void asC(std::map<int, G_Node>& ops, string filename);	//opertaion assignment constraint
void daC(std::map<int, G_Node>& ops, int tnum, string filename, vector<int> delay); //data dependency constraint.
void reC(std::map<int, G_Node>& ops, int tnum, string filename, vector<int> delay, int LC);


void main(int argc, char** argv)
{

	string filename = "lib.txt";
	std::vector<int> delay, lp, dp;
	READ_LIB(filename, delay, lp, dp);

	for (int DFG = 1; DFG <= 22; ++DFG) //For each DFG 1-15 = media, 16-22 = random
	{
		int edge_num = 0;
		std::map<int, G_Node> ops;
		LC = 0, opn = 0, edge_num = 0;
		ops.clear();

		//read DFG
		string filename, dfg_name;
		Read_DFG(DFG, filename, dfg_name);			//read DFG filename
		readGraphInfo(filename, edge_num, opn, ops); //read DFG info


		stringstream str(dfg_name);
		string tok;
		string DFGname; //filename (DFG name) for input/output
		while (getline(str, tok, '.')){
			if (tok != "txt")
				DFGname = tok;
		}

		string outputFileName = DFGname + "_ILP.txt";

		//output to file


		//get ASAP/ALAP/MR
		ASAP(ops, delay); //Obtain ASAP for each operation
		getLC(LC, latency_parameter, ops, delay);
		ALAP(ops, delay, LC); //Obtain ALAP for each operation

		ofstream fout_b(outputFileName, ios::out | ios::app);


		//output basic information:
		std::cout << "DFG ID: " << DFG << ", name : " << filename << endl;
		std::cout << "latency constraint: " << LC << endl;

		/*
			get number of variables :
			all operations, each has |MR| vars.
			all FU-types, each has one var.
		*/

		int numberVariables = tnum; //initialize by # of FU-types 
		int numberDataDependency = 0; //initialize # of data dependency functions
		int numberResourceConstraint = tnum * LC; //initialize # of resource constraint functions

		// LC constraint is skipped, since MR considers LC already.
		// 0/1 variable constraint is considered by variable assignment (bool type)
		//resource constraint: each cc across [1, LC], each type has one. 
		for (auto i : ops)
		{
			/*
				all operations, each has |MR| vars.
			*/
			numberVariables += (i.second.alap - i.second.asap + 1);

			/*
				get number of data dependencies:
				# of arcs:
				here, we only count #s of predecessors (immediate, depth = 1 only) across all operations
			*/

			numberDataDependency += i.second.parent.size();
		}

		fout_b << "// basic information section:" << endl;
		fout_b << "// DFG name: " << DFGname << endl;
		fout_b << "// # of nodes (operations): " << opn << endl;
		fout_b << "// # of arcs (data dependency): " << edge_num << endl;
		fout_b << "// Latency constraint: " << LC << endl;
		fout_b << "// # of variables: " << numberVariables << endl;
		fout_b << "// # of data dependency constraint functions: " << numberDataDependency << endl;
		fout_b << "// # of resource constraint functions: " << numberResourceConstraint << endl;
		fout_b << endl;

		fout_b << "// variable section:" << endl;
		defineV(ops, tnum, outputFileName);

		fout_b << endl;
		fout_b << "// objective function section:" << endl;

		objF(ops, tnum, outputFileName);

		fout_b << "// constraint function section:" << endl;

		//cout << "subject to{" << endl;
		fout_b << "subject to{" << endl;

		fout_b << "// operation scheduling assignment constrain function section:" << endl;
		asC(ops, outputFileName);

		fout_b << endl;
		fout_b << "// data dependency constrain function section:" << endl;

		daC(ops, tnum, outputFileName, delay);

		fout_b << endl;
		fout_b << "// resource constrain function section:" << endl;

		reC(ops, tnum, outputFileName, delay, LC);


		fout_b << "}" << endl;
		fout_b.close();
	}

	std::cout << "Press ENTER to terminate the program." << endl;
	std::cin.get();	//press Enter to terminate the program
}

//0-1ILP functions.

/*
*	Same functions used in FDS to get basic MR/ASAP/ALAP information
*/

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



//0-1ILP functions.

//declare variables
void defineV(std::map<int, G_Node>& ops, int tnum, string filename)
{
	ofstream fout_b(filename, ios::out | ios::app);

	//declare 0/1 variables for operations
	// dvar boolean x_<operID>_<cc>

	for (auto i : ops) {
		//get ID
		int operID = i.second.id;

		//for each cc in MR:
		for (auto cc = i.second.asap; cc <= i.second.alap; cc++)
			fout_b << "dvar boolean x_" << operID << "_" << cc << ";" << endl;

	}

	//declare function usage variable.
	//dvar int+ N_<FUtype_ID> 0: ADD; 1: MUL
	for (auto t = 0; t < tnum; t++)
		fout_b << "dvar int+ N_" << t << ";" << endl;

	fout_b.close();
}

//objective function
void objF(std::map<int, G_Node>& ops, int tnum, string filename)
{

	ofstream fout_b(filename, ios::out | ios::app);

	fout_b << "dexpr float N_FU =(";

	for (int t = 0; t < tnum; t++) {

		if (t == 0) {
			fout_b << "N_" << t;
		}
		else
		{
			fout_b << "+";
			fout_b << "N_" << t;
		}
	}

	fout_b << ");" << endl;
	fout_b << "minimize N_FU;" << endl;
	fout_b << endl;

	fout_b.close();
}

//operation scheduling assignment constraint.
void asC(std::map<int, G_Node>& ops, string filename)
{
	ofstream fout_b(filename, ios::out | ios::app);

	for (auto i : ops) {

		int opID = i.second.id;
		for (auto cc = i.second.asap; cc <= i.second.alap; cc++) {

			if (cc == i.second.asap) {

				//x_<oper_ID>_<cc> + ....
				fout_b << "x_" << opID << "_" << cc;
			}
			else {
				fout_b << "+" << "x_" << opID << "_" << cc;
			}

		}
		fout_b << "==1;" << endl;
	}

	fout_b.close();
}

//data dependency cosntraint, for each oper u, for each predecessor v (if exists)
void daC(std::map<int, G_Node>& ops, int tnum, string filename, vector<int> delay)
{
	ofstream fout_b(filename, ios::out | ios::app);

	//original nodes
	for (auto u : ops) {

		//skip oper with 0 predecessor
		if (u.second.parent.empty())
			continue;

		int uID = u.second.id;

		//otherwise: for each u,
		// for each predecessor of u,
		// get the sum of start time weighted by bool var, 
		// arc p-->u
		for (auto p : u.second.parent) {

			//get pID, do the start time summation for u

			for (auto u_start = u.second.asap; u_start <= u.second.alap; u_start++) {
				//for 1st var.
				if (u_start == u.second.asap)
					fout_b << u_start << "*x_" << uID << "_" << u_start;
				else
					fout_b << "+" << u_start << "*x_" << uID << "_" << u_start;
			}

			//get pID, do the start time summation for u
			int pID = p->id;
			//do the start time summation for u, but need "-" sign for all.

			for (auto p_start = p->asap; p_start <= p->alap; p_start++)
				fout_b << "-" << p_start << "*x_" << pID << "_" << p_start;

			//finally, get delay of p, and set the constraint.
			int ptype = p->type;
			int d = delay[ptype];

			fout_b << ">=" << d << ";" << endl;
		}
	}

	fout_b.close();
}

//resouce constraint: for each type, each cc.
void reC(std::map<int, G_Node>& ops, int tnum, string filename, vector<int> delay, int LC)
{
	ofstream fout_b(filename, ios::out | ios::app);

	int count = 0;
	int first = 0;
	for (int t = 0; t < tnum; t++)
		for (int l = 1; l <= LC; l++)
		{
			//re-initialize the # of vars and if the first var appears (for outputting format).
			count = 0;
			first = 0;

			//for each type, ecah cc, scan opers:
			for (auto u : ops) {

				int uID = u.second.id;

				//for same type t ops:
				if (u.second.type == t) {
					//scan u from asap to alap.
					for (auto u_start = u.second.asap; u_start <= u.second.alap; u_start++)
						// for each delay cc.
						for (int d = 0; d < delay[u.second.type]; d++)
							//if a starting time s and its execution period (s, s+d-1) hit the current cc l
							//this start time is considered in this equation.
							if (u_start + d == l) {

								count += 1;
								first += 1;

								if (first == 1)
									fout_b << "x_" << uID << "_" << u_start;
								else
									fout_b << "+x_" << uID << "_" << u_start;
							}
				}
			}

			if (count > 0)
				fout_b << "<=N_" << t << ";" << endl;
		}

	fout_b.close();
}