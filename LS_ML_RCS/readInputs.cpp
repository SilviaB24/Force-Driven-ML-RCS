#include "LS.h"
#define _CRT_SECURE_NO_WARNINGS

using namespace std;

/*
*
* GET_LIB: read library: only have 2 types: Add/Mul
* 0: Add
* 1: Mul
*
*/

//this function is used to skip the comment lines.
inline bool isCommentLine(const string& line) {
	string s = line;
	s.erase(0, s.find_first_not_of(" \t"));
	return s.rfind("//", 0) == 0;
}

//read library file
//store metrics into these structures:
//delay
//lp
//dp


void READ_LIB(const string& file_name,
	vector<int>& delay,
	vector<int>& lp,
	vector<int>& dp,
	vector<string>& res_type,
	vector<int>& res_constr)
{
	ifstream fin(file_name);
	if (!fin.is_open()) {
		cerr << "Error: cannot open " << file_name << endl;
		return;
	}

	string line;


	while (getline(fin, line)) {

		if (line.empty() || isCommentLine(line))
			continue;

		string type;
		int count, d, leakage, dynamic_p;

		stringstream ss(line);

		// CHANGED BY SILVIA
		ss >> type >> count >> d >> leakage >> dynamic_p;
		// END CHANGED BY SILVIA

		if (ss.fail()) {
			cerr << "Warning: malformed line ignored: " << line << endl;
			continue;
		}
		delay.push_back(d);
		lp.push_back(leakage);
		dp.push_back(dynamic_p);

		// IMPLEMENTED BY SILVIA

		res_type.push_back(type);
		res_constr.push_back(count);

		// END IMPLEMENTED BY SILVIA
	}

	fin.close();
}

//DO NOT CHANGE FUNCTIONS BELOW//
/*
*
* Read_DFG: Read input DFG files: all DFGs are stored under the "DFG" folder, where DFG folder is in the same directory of your main codes.
* ReadGraphInfo: store DFG nodes/parent/child (arcs) into "G_Node" map
*/

void Read_DFG(int& DFG, string& filename, string& dfg_name)
{
	if (DFG == 0)
		filename = "DFG//example.txt";	//this DFG is not provided in the input DFG files, used for your customized DFG only
	else if (DFG == 1)
		filename = "DFG//hal.txt";
	else if (DFG == 2)
		filename = "DFG//horner_bezier_surf_dfg__12.txt";
	else if (DFG == 3)
		filename = "DFG//arf.txt";
	else if (DFG == 4)
		filename = "DFG//motion_vectors_dfg__7.txt";
	else if (DFG == 5)
		filename = "DFG//ewf.txt";
	else if (DFG == 6)
		filename = "DFG//feedback_points_dfg__7.txt";
	else if (DFG == 7)
		filename = "DFG//write_bmp_header_dfg__7.txt";
	else if (DFG == 8)
		filename = "DFG//interpolate_aux_dfg__12.txt";
	else if (DFG == 9)
		filename = "DFG//matmul_dfg__3.txt";
	else if (DFG == 10)
		filename = "DFG//smooth_color_z_triangle_dfg__31.txt";
	else if (DFG == 11)
		filename = "DFG//invert_matrix_general_dfg__3.txt";
	else if (DFG == 12)
		filename = "DFG//h2v2_smooth_downsample_dfg__6.txt";
	else if (DFG == 13)
		filename = "DFG//collapse_pyr_dfg__113.txt";
	else if (DFG == 14)
		filename = "DFG//idctcol_dfg__3.txt";
	else if (DFG == 15)
		filename = "DFG//jpeg_fdct_islow_dfg__6.txt";
	else if (DFG == 16)
		filename = "DFG//random1.txt";
	else if (DFG == 17)
		filename = "DFG//random2.txt";
	else if (DFG == 18)
		filename = "DFG//random3.txt";
	else if (DFG == 19)
		filename = "DFG//random4.txt";
	else if (DFG == 20)
		filename = "DFG//random5.txt";
	else if (DFG == 21)
		filename = "DFG//random6.txt";
	else if (DFG == 22)
		filename = "DFG//random7.txt";

	// IMPLEMENTED BY SILVIA
	else if (DFG == 23)
		filename = "DFG//custom_test1.txt";
	else if (DFG == 24)
		filename = "DFG//custom_test2.txt";

	// END IMPLEMENTED BY SILVIA

	dfg_name = filename.substr(5);
}

/*
	The function to read the detailed DFG and store nodes.

	nodes are stored in ops structure,
	opn is the # of operations.
	the arc is stored via operation's child (successors) and parent (predecessors), only up to depth = 1 parents/children (like a tree node)

*/

void readGraphInfo(string& filename, int& edge_num, int& opn, std::map<int, G_Node>& ops)
{
	FILE* bench;		//the input DFG file

	char* char_arr = &filename[0];

	if (!(bench = fopen(char_arr, "r")))	//open the input DFG file to count the number of operation nodes in the input DFG
	{
		std::cerr << "Error: Reading input DFG file " << char_arr << " failed." << endl;
		cin.get();	//waiting for user to press enter to terminate the program, so that the text can be read
		exit(EXIT_FAILURE);
	}
	char* line = new char[100];
	opn = 0;			//initialize the number of operation node

	while (fgets(line, 100, bench))		//count the number of operation nodes in the input DFG
		if (strstr(line, "label") != NULL)	//if the line contains the keyword "label", the equation returns true, otherwise it returns false. search for c/c++ function "strstr" for detail
			opn++;

	fclose(bench);	//close the input DFG file

	for (auto i = 0; i < opn; i++)
	{
		G_Node curr;

		// IMPLEMENTED BY SILVIA
		curr.criticalSuccessorId = -1; // Initialize criticalSuccessorId
		curr.priority1 = 0.0f; // Initialize priority1
		curr.priority2 = 0.0f; // Initialize priority2
		curr.priority3 = 0; // Initialize priority3
		// END IMPLEMENTED BY SILVIA
		
		ops[i] = curr;
	}

	//close the input DFG file
	//based on the number of operation node in the DFG, dynamically set the size
	std::map<string, int> oplist;
	string name, cname;
	char* tok, * label;	//label: the pointer point to "label" or "->" in each line
	char seps[] = " \t\b\n:";		//used with strtok() to extract DFG info from the input DFG file
	int node_id = 0;	//count the number of edges in the input DFG file
	if (!(bench = fopen(char_arr, "r")))	//open the input DFG file agian to read the DFG, so that the cursor returns back to the beginning of the file
	{
		std::cerr << "Error: Failed to load the input DFG file." << endl;
		cin.get();	//waiting for user to press enter to terminate the program, so that the text can be read
		exit(EXIT_FAILURE);
	}
	if (opn < 600)
		while (fgets(line, 100, bench))	//read a line from the DFG file, store it into "line[100]
		{
			if ((label = strstr(line, "label")) != NULL)	//if a keyword "label" is incurred, that means a operation node is found in the input DFG file
			{
				tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
				name.assign(tok);	//obtain the node name
				oplist.insert(make_pair(name, node_id));	//match the name of the node to its number flag
				tok = strtok(label + 7, seps);	//obtain the name of the operation type
				if (strcmp(tok, "ADD") == 0)	  ops[node_id].type = 0;			//match the operation type to the nod. search for c/c++ function "strcmp" for detail
				else if (strcmp(tok, "AND") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "MUL") == 0) ops[node_id].type = 1;
				else if (strcmp(tok, "ASR") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "LSR") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "LOD") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "STR") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "SUB") == 0) ops[node_id].type = 0;


				// CHANGED BY SILVIA

				// CHANGE THIS TO SET DIVISION AS A DIFFERENT TYPE
				else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 2;

				// USE DIV AS MUL TYPE
				//else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 1;

				// END CHANGED BY SILVIA
				
				ops[node_id].id = node_id;
				node_id++;
			}
			else if ((label = strstr(line, "->")) != NULL)	//if a keyword "->" is incurred, that means an edge is found in the input DFG file
			{
				tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
				name.assign(tok);	//obtain node name u from edge (u, v)
				cname.assign(strtok(label + 3, seps));	////obtain node name v from edge (u, v)
				(ops[oplist[name]].child).push_back(&(ops[oplist[cname]]));	//use double linked list to hold the children
				(ops[oplist[cname]].parent).push_back(&(ops[oplist[name]]));//use double linked list to hold the parents
				edge_num++;
			}
		}
	else
	{
		while (fgets(line, 100, bench))	//read a line from the DFG file, store it into "line[100]
		{
			if ((label = strstr(line, "label")) != NULL)	//if a keyword "label" is incurred, that means a operation node is found in the input DFG file
			{
				tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
				name.assign(tok);	//obtain the node name
				oplist.insert(make_pair(name, node_id));	//match the name of the node to its number flag
				tok = strtok(label + 7, seps);	//obtain the name of the operation type
				if (strcmp(tok, "ADD") == 0)	  ops[node_id].type = 0;			//match the operation type to the nod. search for c/c++ function "strcmp" for detail
				else if (strcmp(tok, "AND") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "MUL") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "ASR") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "LSR") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "LOD") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "STR") == 0) ops[node_id].type = 0;
				else if (strcmp(tok, "SUB") == 0) ops[node_id].type = 1;

				
				// CHANGED BY SILVIA

				// CHANGE THIS TO SET DIVISION AS A DIFFERENT TYPE
				else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 2;

				// USE DIV AS MUL TYPE
				//else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 1;

				// END CHANGED BY SILVIA

				ops[node_id].id = node_id;
				node_id++;
			}
			else if ((label = strstr(line, "->")) != NULL)	//if a keyword "->" is incurred, that means an edge is found in the input DFG file
			{
				tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
				name.assign(tok);	//obtain node name u from edge (u, v)
				cname.assign(strtok(label + 3, seps));	////obtain node name v from edge (u, v)
				(ops[oplist[name]].child).push_back(&(ops[oplist[cname]]));	//use double linked list to hold the children
				(ops[oplist[cname]].parent).push_back(&(ops[oplist[name]]));//use double linked list to hold the parents
				edge_num++;
			}
		}

	}//end of reading DFG

	fclose(bench);
	delete[] line;
}