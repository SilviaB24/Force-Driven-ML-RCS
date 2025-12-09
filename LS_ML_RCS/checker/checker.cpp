//The original code is created by Huan Ren. Last modified by Owen and Keivn.
//"lookahead" approach was not discussed in the class but is used in this code, for forces calculation. please refer to the FDS paper section IV: E
#include "checker.h"
#include <climits>
#include <bitset>
#include <queue>

using namespace std;

const int tnum = 2;	//number of operation type

int DFG = 0; //DFG ID, this is automatically run from range 1 to 11
int LC = 0; //global parameter latency constraint. LC = latency_parameter * ASAP latency.
string DFGname; //filename (DFG name) for input/output
string outputfile; //output filename

int opn = 0; //# of operations in current DFG
//G_Node* ops; //operations list

std::map<int, G_Node> ops;

ofstream output_sb_results;

bool parse_filename_by_algo(const std::string& filename,
	std::string& dfg_name,
	std::string& algo_name);

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::cerr << "Usage: fds.exe <input_filename>\n";
		return 1;
	}

	std::string input_filename = argv[1];
	//std::cout << "Input file: " << input_filename << endl;

	std::string check_dfg_name, algo_name;

	if (!parse_filename_by_algo(input_filename, check_dfg_name, algo_name)) {
		std::cerr << "Invalid filename format: " << input_filename << std::endl;
		return 1;
	}

	std::cout << "DFG name = " << check_dfg_name << "\n";
	std::cout << "HLS algo = " << algo_name << "\n";

	string filename, dfg_name;
	filename = "DFG//" + check_dfg_name + ".txt";
	//Read_DFG(DFG, filename, dfg_name); //read DFG filename

	//get delay structure;
	string lib_filename = "lib.txt";
	Lib lib;

	vector<int> delay, lp, dp;
	delay.clear();
	READ_LIB(lib_filename, delay, lp, dp);

	int edge_num = 0;
	readGraphInfo(filename, edge_num, opn); //read DFG info

	//initialize resource constraint function.
	int rc[2];
	rc[0] = 0;
	rc[1] = 0;

	int all_error = 0;
	ops.clear();

	//read s&b results:
	S sb_res;
	//	New Change for ML-RCS, add "rc".
	get_S_structure(input_filename, sb_res, LC, rc);

	int dependency_error = 0;
	std::map<int, int> error_pair;
	error_pair.clear();

	int actual_latency = 0;

	for (auto u = 0; u < opn; u++) {

		int u_type = ops[u].type;
		int u_delay = delay[u_type];
		int u_start_time = sb_res.schedule[u];
		int u_end_time = u_start_time + u_delay - 1;

		// New Change for ML-RCS: get actual latency.
		if (actual_latency < u_end_time)
			actual_latency = u_end_time;

		if (ops[u].parent.size() > 0) {
			for (auto prsu = ops[u].parent.begin(); prsu != ops[u].parent.end(); prsu++) {

				int prsu_type = (*prsu)->type;
				int prsu_delay = delay[prsu_type];
				int prsu_start_time = sb_res.schedule[(*prsu)->id];
				int prsu_end_time = prsu_start_time + prsu_delay - 1;

				if (u_start_time > prsu_end_time) {

				}
				else {
					//cout << "error: u = " << u << " time = " << u_time << " pr = " << (*prsu)->id << " pr_time = " << prsu_time << " pr_delay = " << prsu_delay << endl;

					if (u < (*prsu)->id)
						error_pair[u] = (*prsu)->id;
					else
						error_pair[(*prsu)->id] = u;
				}
			}
		}

		if (ops[u].child.size() > 0) {
			for (auto prsu = ops[u].child.begin(); prsu != ops[u].child.end(); prsu++) {

				int prsu_type = (*prsu)->type;
				int prsu_delay = delay[prsu_type];
				int prsu_start_time = sb_res.schedule[(*prsu)->id];
				int prsu_end_time = prsu_start_time + prsu_delay - 1;

				if (u_end_time < prsu_start_time) {

				}
				else {

					//	cout << "error: u = " << u << " time = " << u_time << " su = " << (*prsu)->id << " su_time = " << prsu_time << " u_delay = " << u_delay << endl;

					if (u < (*prsu)->id)
						error_pair[u] = (*prsu)->id;
					else
						error_pair[(*prsu)->id] = u;
				}
			}
		}

	}

	std::cout << "LC = " << LC << endl;
	std::cout << "# of dependency error = " << error_pair.size() << endl;

	//check FU-overlapping:

	std::map<int, int> FUs;
	FUs.clear();

	for (auto u = 0; u < opn; u++)
		FUs[sb_res.bind[u]] = 1;

	std::map<int, std::map<int, int>> FU_bind;
	FU_bind.clear();

	std::map<int, std::map<int, int>> FU_usage;
	FU_usage.clear();

	for (auto fu = FUs.begin(); fu != FUs.end(); fu++)
		for (auto cc = 1; cc <= actual_latency; cc++)
			FU_bind[fu->first][cc] = 0;

	for (auto t = 0; t < tnum; t++)
		for (auto cc = 1; cc <= actual_latency; cc++)
			FU_usage[t][cc] = 0;

	for (auto u = 0; u < opn; u++)
	{
		int my_fu = sb_res.bind[u];

		int my_delay = delay[ops[u].type];

		int my_cc = sb_res.schedule[u];

		for (auto c = my_cc; c < my_cc + my_delay; c++)
		{
			FU_bind[my_fu][c]++;
			FU_usage[ops[u].type][c]++;
		}

	}

	int fu_overlapped_error = 0;
	for (auto fu = FU_bind.begin(); fu != FU_bind.end(); fu++)
		for (auto cc = fu->second.begin(); cc != fu->second.end(); cc++)
			if (cc->second > 1) {
				std::cout << "for FU " << fu->first << " cc " << cc->first << " has 2 op executed on it, error exists." << endl;
				fu_overlapped_error++;
			}

	std::cout << "# of FU usage error (mutliple ops executes on the same cc and same FU) = " << fu_overlapped_error << endl;

	/*
		New Change for ML-RCS:
		Check resource constraint:
	*/
	//initialize max_resouce usage for each type.
	int max_resource[2];
	max_resource[0] = 0;
	max_resource[1] = 0;

	for (auto type = 0; type < tnum; type++)
		for (auto cc = 1; cc <= actual_latency; cc++)
			if (max_resource[type] < FU_usage[type][cc])
				max_resource[type] = FU_usage[type][cc];

	std::cout << "# of ADD used = " << max_resource[0] << " , ADD resource constraint = " << rc[0] << endl;
	std::cout << "# of MUL used = " << max_resource[1] << " , MUL resource constraint = " << rc[1] << endl;

	if (max_resource[0] > rc[0])
	{
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Resource constraint for ADD is not satisfied. RC of ADD = " << rc[0] << " Max usage of ADD = " << max_resource[0] << endl;
	}

	if (max_resource[1] > rc[1]) {
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Resource constraint for MUL is not satisfied. RC of MUL = " << rc[1] << " Max usage of MUL = " << max_resource[1] << endl;
	}

	std::cout << "Actual Latency = " << actual_latency << " Reported latency = " << LC << endl;
	if (actual_latency != LC) {
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Actual latency and reported latency are not the same. " << endl;
	}

	//End new change.

	std::cout << "********** For DFG : " << DFG << " , Reported LC = " << LC << " , Total # of Errors = " << (fu_overlapped_error + error_pair.size()) << " * *********************" << endl;

	all_error += (fu_overlapped_error + error_pair.size());

	std::cout << "Press ENTER to terminate the program." << endl;
	std::cin.get();	//press Enter to terminate the program
}

bool parse_filename_by_algo(const std::string& filename,
	std::string& dfg_name,
	std::string& algo_name)
{
	// FDS
	std::string marker;
	size_t pos = std::string::npos;

	// "_FDS_"
	marker = "_FDS_";
	pos = filename.find(marker);
	if (pos != std::string::npos) {
		algo_name = "FDS";
	}
	else {
		// "_LS_"
		marker = "_LS_";
		pos = filename.find(marker);
		if (pos != std::string::npos) {
			algo_name = "LS";
		}
		else {
			// 
			return false;
		}
	}

	// 
	if (pos == 0) {
		// 
		return false;
	}

	dfg_name = filename.substr(0, pos);
	return true;
}


//----------------//

// replace the line below in "checker.h" line-81.
// void get_S_structure(string& filename, S& s, int& LC, int rc[2]);

//replace the function below in "checker.cpp" (starting from line-196).
void get_S_structure(string& filename, S& s, int& LC, int rc[2])
{
	std::string lineStr;
	std::ifstream ifs(filename);
	std::string line;

	std::cout << "filename = " << filename << endl;

	s.schedule.clear();
	s.bind.clear();
	s.fu_info.clear();

	int counter = 0;

	while (std::getline(ifs, line))
	{
		/*
			New Change for ML-RCS:
		*/
		//line-0 here is the resource constraint info for ADD
		if (counter == 0) {
			std::istringstream iss(line);
			string type;
			int num;

			if (!(iss >> type >> num))
				break;
			else
				rc[0] = num;
		}
		// line-1 here is the resouce constraint info for MUL
		else if (counter == 1) {
			std::istringstream iss(line);
			string type;
			int num;

			if (!(iss >> type >> num))
				break;
			else
				rc[1] = num;
		}
		else if (counter == 3)
		{
			//line-3 here is used to read single actual latency info and you should modify it to your actual latency instead of using LC for MR-LCS.
			std::istringstream iss(line);
			string a;
			int lc;
			if (!(iss >> a >> lc))
			{
				break;
			}
			else {
				LC = lc;
			}
		}
		else if (counter > 3)
		{
			//other lines are op S&B info.
			std::istringstream iss(line);
			string type;
			int op_id, fu_id, cc;
			if (!(iss >> op_id >> cc >> fu_id))
			{
				break;
			}
			else
			{
				//obtain op_id, fu_id, cc, fu-type;
				s.schedule[op_id] = cc;
				s.bind[op_id] = fu_id;
			}


		}

		counter++;
	}
}



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
	vector<int>& dp)
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
		int d, leakage, dynamic_p;

		stringstream ss(line);
		ss >> type >> d >> leakage >> dynamic_p;

		if (ss.fail()) {
			cerr << "Warning: malformed line ignored: " << line << endl;
			continue;
		}
		delay.push_back(d);
		lp.push_back(leakage);
		dp.push_back(dynamic_p);
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

	dfg_name = filename.substr(5);
}

/*
	The function to read the detailed DFG and store nodes.

	nodes are stored in ops structure,
	opn is the # of operations.
	the arc is stored via operation's child (successors) and parent (predecessors), only up to depth = 1 parents/children (like a tree node)

*/

void readGraphInfo(string& filename, int& edge_num, int& opn)
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
				else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 1;
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
				else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 1;
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