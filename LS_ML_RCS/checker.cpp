//The original code is created by Huan Ren. Last modified by Owen and Keivn.
//"lookahead" approach was not discussed in the class but is used in this code, for forces calculation. please refer to the FDS paper section IV: E
#include "checker.h"
#include <climits>
#include <bitset>
#include <queue>
#include <regex>

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

// ===================== Helper functions =====================
std::string trim(const std::string& str);
std::string to_lower(const std::string& str);

std::string trim(const std::string& str) {
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) {
		return "";
	}
	size_t end = str.find_last_not_of(" \t\r\n");
	return str.substr(start, end - start + 1);
}

std::string to_lower(const std::string& str) {
	std::string res = str;
	std::transform(res.begin(), res.end(), res.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return res;
}

// ===================== func1, func2, func3, func4 =====================
// f1 for reading DFG name (line 0)
// f2 for reading FU informations <type> <rc> <usage> <delay>
// f3 for reading actual latency
// f4 for reading detailed s or s&b solution.

void func1(const std::string& line, string& DFG_name);
void func2(const std::string& line, std::map<int, string>& FU_type, std::map<int, int>& FU_constraint, std::map<int, int>& reported_FUs, std::map<int, int>& FU_delay, std::map<string, int>& FU_type_name_to_id);
void func3(const std::string& line, int& actual_latency_reported);
void func4(const std::string& line, S& s);

// func1: first non-comment, non-empty line: <DFG Name>
void func1(const std::string& line, string& DFG_name) {
	DFG_name = trim(line);
	// You can also log if needed:
	// std::cout << "DFG name = " << DFG_name << std::endl;
}

// func2: FU-type lines (between line 2 and the line before "latency")
// Format: <fu-type-string> <FU resource constraint> <reported FU usage> <FU delay>
// FU-type ID rule:
//   - If fuName is in predefined_FU_type_id, use that fixed ID.
//   - Otherwise, assign an ID starting from 8 in the order of appearance.

void func2(const std::string& line, std::map<int, string>& FU_type, std::map<int, int>& FU_constraint, std::map<int, int>& reported_FUs, std::map<int, int>& FU_delay, std::map<string, int>& FU_type_name_to_id) {
	std::stringstream ss(line);
	std::string fuName;
	int rc = 0;
	int reported = 0;
	int delay = 0;

	std::map<std::string, int> predefined_FU_type_id = {
	{"ADD",0},{"MUL",1},{"AND",2},{"ASR",3},
	{"LSR",4},{"STR",5},{"SUB",6},{"DIV",7}
	};

	// Next dynamic FU-type ID for non-predefined types (starts from 8)
	int next_dynamic_fu_type_id = 8;

	if (!(ss >> fuName >> rc >> reported >> delay)) {
		std::cerr << "Error: invalid FU-type line format: " << line << std::endl;
		std::exit(1);
	}


	int fuTypeID = -1;

	// First, check if this FU-type is in the predefined mapping
	auto it_pre = predefined_FU_type_id.find(fuName);
	if (it_pre != predefined_FU_type_id.end()) {
		// Use predefined ID
		fuTypeID = it_pre->second;
	}
	else {
		// Not in predefined list: use dynamic ID starting from 8
		auto it_dyn = FU_type_name_to_id.find(fuName);
		if (it_dyn == FU_type_name_to_id.end()) {
			// First time seeing this new FU-type -> assign a new ID
			fuTypeID = next_dynamic_fu_type_id++;
			FU_type_name_to_id[fuName] = fuTypeID;
		}
		else {
			// Already assigned a dynamic ID before
			fuTypeID = it_dyn->second;
		}
	}

	// Store information in the global maps
	FU_type[fuTypeID] = fuName;
	FU_constraint[fuTypeID] = rc;
	reported_FUs[fuTypeID] = reported;
	FU_delay[fuTypeID] = delay;

	// Also record the name-to-ID mapping for both predefined and dynamic types
	FU_type_name_to_id[fuName] = fuTypeID;
}

// func3: "latency" keyword line
// Format: "actual latency" <actual latency value>
// Note: the keyword "latency" should be matched case-insensitively.
void func3(const std::string& line, int& actual_latency_reported) {
	std::string lower_line = to_lower(line);
	if (lower_line.find("latency") == std::string::npos) {
		std::cerr << "Error: func3 called on a line without keyword 'latency': " << line << std::endl;
		std::exit(1);
	}

	// Extract the (last) integer value from the line as the actual latency
	std::stringstream ss(line);
	std::string token;
	int lastInt = -1;
	while (ss >> token) {
		std::stringstream ts(token);
		int val;
		if (ts >> val) {
			lastInt = val;
		}
	}

	if (lastInt < 0) {
		std::cerr << "Error: unable to parse actual latency value from line: " << line << std::endl;
		std::exit(1);
	}

	actual_latency_reported = lastInt;
	// std::cout << "Actual latency (reported) = " << actual_latency_reported << std::endl;
}

// func4: all lines after the "actual latency" line
// Format:
//   scheduling-only checker: <operation ID> <scheduling time>
//   S&B checker:             <operation ID> <scheduling time> <FU-ID>
void func4(const std::string& line, S& s) {
	std::stringstream ss(line);
	int op_id = 0;
	int sched_cc = 0;
	int fu_id = -1;

	if (!(ss >> op_id >> sched_cc)) {
		std::cerr << "Error: invalid operation line format (missing op_id or scheduling time): "
			<< line << std::endl;
		std::exit(1);
	}

	// Try to read FU-ID (for S&B checker)
	if (ss >> fu_id) {
		// S&B case: both schedule and bind
		s.schedule[op_id] = sched_cc;
		s.bind[op_id] = fu_id;
	}
	else {
		// Scheduling-only case: no FU-ID present
		s.schedule[op_id] = sched_cc;
		// bind map remains untouched for this op_id
	}
}


int main(int argc, char** argv)
{
	if (argc < 2) {
		std::cerr << "Usage: fds.exe <input_filename>\n";
		return 1;
	}

	int edge_num = 0;
	int all_error = 0;
	ops.clear();

	std::string input_filename = argv[1];
	//std::cout << "Input file: " << input_filename << endl;

	std::string check_dfg_name, algo_name;

	// CHANGED BY SILVIA

    // Parse input filename to extract clean DFG name
    std::string temp_dfg_name = input_filename;
    
    // Remove algorithm prefix if present
    size_t pos_prefix = temp_dfg_name.find("Improved_"); 
    if (pos_prefix != std::string::npos) {
        temp_dfg_name = temp_dfg_name.substr(pos_prefix + 9);
    }

    // Remove features suffixes
	std::string clean_dfg_name = std::regex_replace(temp_dfg_name, std::regex("(_S[01]_P[01]\\.txt)$"), "");

    
    check_dfg_name = clean_dfg_name;
    algo_name = "LS_Improved";

	std::cout << "DFG name = " << check_dfg_name << endl;
	std::cout << "HLS algo = " << algo_name << endl;

	string filename, dfg_name;
	filename = "DFG//" + check_dfg_name + ".txt";
	//Read_DFG(DFG, filename, dfg_name); //read DFG filename


    readGraphInfo(filename, edge_num, opn, ops);




	// END CHANGED BY SILVIA


	std::string DFG_name;
	std::map<int, string> FU_type;
	FU_type.clear();

	std::map<string, int> FU_type_name_to_id;
	FU_type_name_to_id.clear();

	//read s&b results:
	S sb_res;
	sb_res.bind.clear();
	sb_res.fu_info.clear();
	sb_res.schedule.clear();

	std::map<int, int> delay, reported_FUs, rc;
	delay.clear();
	reported_FUs.clear();
	rc.clear();


	get_S_structure(input_filename, DFG_name, FU_type, rc, reported_FUs, delay, FU_type_name_to_id,
		LC, sb_res);

	cout << "reported FUs ADD = " << reported_FUs[0] << " MUL = " << reported_FUs[1] << endl;
	cout << "delay of ADD = " << delay[0] << " MUL = " << delay[1] << endl;

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

	std::cout << "# of ADD used (derived from your schl solution) = " << max_resource[0] << " , reported # of ADD used = " << reported_FUs[0] << " , ADD resource constraint = " << rc[0] << endl;
	std::cout << "# of MUL used (derived from your schl solution) = " << max_resource[1] << " , reported # of MUL used = " << reported_FUs[1] << " , MUL resource constraint = " << rc[1] << endl;

	if (max_resource[0] > rc[0])
	{
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Resource constraint for ADD is not satisfied. RC of ADD = " << rc[0] << " Max usage of ADD = " << max_resource[0] << endl;
	}

	if (max_resource[1] > rc[1]) {
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Resource constraint for MUL is not satisfied. RC of MUL = " << rc[1] << " Max usage of MUL = " << max_resource[1] << endl;
	}

	if (reported_FUs[0] != max_resource[0])
	{
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Actual and reported ADD used are not the same." << endl;
	}

	if (reported_FUs[1] != max_resource[1]) {
		fu_overlapped_error++; //increment # of total errors.
		std::cout << "Actual and reported MUL used are not the same." << endl;
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
int get_S_structure(string& filename, string& DFG_name, std::map<int, string>& FU_type, std::map<int, int>& FU_constraint, std::map<int, int>& reported_FUs, std::map<int, int>& FU_delay, std::map<string, int>& FU_type_name_to_id,
	int& actual_latency_reported, S& s)
{
	std::ifstream fin(filename);
	if (!fin) {
		std::cerr << "Error: cannot open file: " << filename << std::endl;
		return 1;
	}

	std::string rawLine;
	std::string firstLine;
	std::vector<std::string> fuLines;
	std::string latencyLine;
	std::vector<std::string> restLines;

	//using an idea of finite state machine (FSM): 3 different states.
	enum State {
		WAIT_FIRST_LINE = 0,
		FU_SECTION_BEFORE_LATENCY,
		AFTER_LATENCY
	};

	State state = WAIT_FIRST_LINE;

	while (std::getline(fin, rawLine)) {
		std::string line = trim(rawLine);

		// Skip empty (blank) lines
		if (line.empty()) {
			continue;
		}

		// Skip comment lines starting with "//" (after trimming)
		if (line.rfind("//", 0) == 0) {
			continue;
		}

		if (state == WAIT_FIRST_LINE) {
			// First valid line: DFG name
			firstLine = line;
			state = FU_SECTION_BEFORE_LATENCY;
			continue;
		}

		if (state == FU_SECTION_BEFORE_LATENCY) {
			// Check if this line contains "latency" (case-insensitive)
			std::string lower_line = to_lower(line);
			if (lower_line.find("latency") != std::string::npos) {
				latencyLine = line;
				state = AFTER_LATENCY;
			}
			else {
				// FU-type line
				fuLines.push_back(line);


			}
		}
		else if (state == AFTER_LATENCY) {
			// All lines after the latency line
			restLines.push_back(line);
		}
	}

	// Check if we have found the latency line
	if (latencyLine.empty()) {
		std::cerr << "Error: no \"latency\" line found." << std::endl;
		return 1;
	}

	// Check FU-type count limit (max 8)
	if (fuLines.size() > 8) {
		std::cerr << "Error: number of FU types exceeds the maximum limit of 8. " << std::endl;
		return 1;
	}

	// ===================== Call func1, func2, func3, func4 =====================

   // 1) First line: DFG name
	func1(firstLine, DFG_name);

	// 2) FU-type lines (2nd to at most 9th valid line)
	for (int i = 0; i < static_cast<int>(fuLines.size()); ++i) {
		func2(fuLines[i], FU_type, FU_constraint, reported_FUs, FU_delay, FU_type_name_to_id);  // fuTypeID = i
	}

	// 3) "actual latency" line
	func3(latencyLine, actual_latency_reported);

	// 4) All lines after the "actual latency" line: ops
	for (const auto& l : restLines) {
		func4(l, s);
	}

	return 0;

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

// CHANGED BY SILVIA
void Read_DFG(int& DFG, string& filename, string& dfg_name, string suffix)
{
	//0: default original DFG, no "_4type_xxxx", just <DFG_name>.txt
	//1: uniform distributed 4-type DFG, "_4type_uniform.txt"
	//2: inversely prop. delay distributed 4-type DFG, "_4type_invdelay.txt"

	string base_name = "";

	if (DFG == 0)
		base_name = "example";	//this DFG is not provided in the input DFG files, used for your customized DFG only
	else if (DFG == 1)
		base_name = "hal";
	else if (DFG == 2)
		base_name = "horner_bezier_surf_dfg__12";
	else if (DFG == 3)
		base_name = "arf";
	else if (DFG == 4)
		base_name = "motion_vectors_dfg__7";
	else if (DFG == 5)
		base_name = "ewf";
	else if (DFG == 6)
		base_name = "feedback_points_dfg__7";
	else if (DFG == 7)
		base_name = "write_bmp_header_dfg__7";
	else if (DFG == 8)
		base_name = "interpolate_aux_dfg__12";
	else if (DFG == 9)
		base_name = "matmul_dfg__3";
	else if (DFG == 10)
		base_name = "smooth_color_z_triangle_dfg__31";
	else if (DFG == 11)
		base_name = "invert_matrix_general_dfg__3";
	else if (DFG == 12)
		base_name = "h2v2_smooth_downsample_dfg__6";
	else if (DFG == 13)
		base_name = "collapse_pyr_dfg__113";
	else if (DFG == 14)
		base_name = "idctcol_dfg__3";
	else if (DFG == 15)
		base_name = "jpeg_fdct_islow_dfg__6";
	else if (DFG == 16)
		base_name = "random1";
	else if (DFG == 17)
		base_name = "random2";
	else if (DFG == 18)
		base_name = "random3";
	else if (DFG == 19)
		base_name = "random4";
	else if (DFG == 20)
		base_name = "random5";
	else if (DFG == 21)
		base_name = "random6";
	else if (DFG == 22)
		base_name = "random7";
	
	filename = "DFG//" + base_name + "_4type" + suffix + ".txt";

	dfg_name = filename.substr(5);
}
// END CHANGED BY SILVIA

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
	
	while (fgets(line, 100, bench))	//read a line from the DFG file, store it into "line[100]
	{
		if ((label = strstr(line, "label")) != NULL)	//if a keyword "label" is incurred, that means a operation node is found in the input DFG file
		{
			tok = strtok(line, seps);	//break up the line by using the tokens in "seps". search the c/c++ function "strtok" for detail
			name.assign(tok);	//obtain the node name
			oplist.insert(make_pair(name, node_id));	//match the name of the node to its number flag
			tok = strtok(label + 7, seps);	//obtain the name of the operation type
			if (strcmp(tok, "ADD") == 0)	  ops[node_id].type = 0;			//match the operation type to the nod. search for c/c++ function "strcmp" for detail
			else if (strcmp(tok, "MUL") == 0) ops[node_id].type = 1;
			else if (strcmp(tok, "DIV") == 0) ops[node_id].type = 2;
			else if (strcmp(tok, "SQRT") == 0) ops[node_id].type = 3;
			else
				ops[node_id].type = 0;

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
	
	//end of reading DFG

	fclose(bench);
	delete[] line;
}