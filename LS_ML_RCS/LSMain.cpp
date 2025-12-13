#include "LS.h"

// IMPLEMENTED BY SILVIA
#ifdef _WIN32
    #include <direct.h>   // Windows-specific header
    #define MAKE_DIR(name) _mkdir(name)
#else
    #include <sys/stat.h> // macOS/Linux-specific header
    #include <sys/types.h>
    #define MAKE_DIR(name) mkdir(name, 0777)
#endif


#include <chrono>
#include <sstream>
#include <iomanip>

// END IMPLEMENTED BY SILVIA


using namespace std;

double latencyParameter = 1.5;	 //latency constant parameter, change this parameter can affect the latency constraint and hence, the final scheduling result is changed
int DFG = 0;					//DFG ID, this is automatically run from range 1 to 11
int LC = 0;						//global parameter latency constraint. LC = latency_parameter * ASAP latency.
int opn = 0;					 //# of operations in current DFG
int edge_num = 0;
//G_Node* ops; //operations list

void LS_outer_loop(std::map<int, int>& schlResult, std::map<int, int>& FUAllocationResult, std::map<int, std::map<int, std::vector<int>>>& bindingResult, int& actualLatency,
	std::map<int, G_Node>& ops, int& latencyConstraint, double& latencyParameter, std::vector<int>& delay, std::vector<int>& res_constr, bool debug, bool featS, bool featP);
void LS(std::map<int, int>& schlResult, std::map<int, int>& FUAllocationResult, std::map<int, std::map<int, std::vector<int>>>& bindingResult, int& actualLatency,
	std::map<int, G_Node>& ops, int& latencyConstraint, double& latencyParameter, std::vector<int>& delay, std::vector<int>& res_constr, bool improvedSolution, bool debug, bool featS, bool featP);


void FDS_Outer_Loop(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC, double& latencyParameter, std::vector<int>& res_constr, bool debug);

// IMPLEMENTED BY SILVIA

void WriteResultToCSV(string algName, string dfgName, string data_type, bool featS, bool featP, int targetLat, int actualLat, int totalFUs, double runtimeMs, double res_scaling_factor, bool ls_base);
void LoadConstraints(const string& filename, std::map<string, ConstraintData>& db);  
std::string to_string_with_precision(float value, int n_decimals); 

// END IMPLEMENTED BY SILVIA

ofstream output_sb_result;

int main(int argc, char** argv)
{

	//read "lib"
	//get delay structure: only ADD/MUL are needed. you can change delay of ADD/MUL (# of cc's) in "lib.txt" file.
    std::string filename = "lib_4type.txt"; 
	std::vector<int> delay, lp, dp;
	
	// IMPLEMENTED BY SILVIA
	string algName = "LS";

	// Command-line arguments for debugging and features activation
    bool debug = false;
    bool featS = false;
    bool featP = false;
	string data_type = "invdelay";
    bool ls_base = false;
	double res_scaling_factor = 1.0;

    if (argc >= 7) {
        debug = (std::stoi(argv[1]) != 0);
        featS = (std::stoi(argv[2]) != 0);
        featP = (std::stoi(argv[3]) != 0);
        data_type = "_" + std::string(argv[4]);
		ls_base = (std::stoi(argv[5]) != 0);
		
		// scaling factor for resource constraints from FALLS
		res_scaling_factor = std::stod(argv[6]);
    }

	std::vector<int> res_constr;
	std::vector<string> res_type;

	READ_LIB(filename, delay, lp, dp, res_type);


	// Load resource constraints from a file
	string constraints_filename = "Constraints/constraints" + data_type + ".txt";
	std::map<string, ConstraintData> constraints_db;

	LoadConstraints(constraints_filename, constraints_db);




	// END IMPLEMENTED BY SILVIA



	// print LIB info for debugging
	for (size_t i = 0; i < delay.size(); i++) {

		if (debug)
			std::cout << "Function ID: " << i << ", Delay: " << delay[i] << ", LP: " << lp[i] << ", DP: " << dp[i] << ", ResType: " << res_type[i] << std::endl;
	}

	//iterate all DFGs from 0 to 22 (the 16-22 are random DFGs)
	for (DFG = 0; DFG <= 22; DFG++) {

		std::map<int, G_Node> ops;
		LC = 0, opn = 0, edge_num = 0;
		ops.clear();

		string filename, dfg_name, clean_dfg_name;
		Read_DFG(DFG, filename, dfg_name, data_type);			//read DFG filename
		readGraphInfo(filename, edge_num, opn, ops); //read DFG info


		// IMPLEMENTED BY SILVIA

		// Clean dfg_name
		if (dfg_name.length() >= 4 && dfg_name.substr(dfg_name.length() - 4) == ".txt") {
            clean_dfg_name = dfg_name.substr(0, dfg_name.length() - 4);
        }

        if (clean_dfg_name.find("_4type") != string::npos) {
            clean_dfg_name = clean_dfg_name.substr(0, clean_dfg_name.find("_4type"));
        }

		// Use the resource constraints for the current DFG read from the constraints database
		res_constr = constraints_db[clean_dfg_name].resources;

		// Scale the resource constraints
		for (auto& rc : res_constr) {
			rc = static_cast<int>(rc * res_scaling_factor);

			// Ensure at least 1 resource
			if (rc < 1) rc = 1;
		}

		if (debug) {
			cout << "Resource Constraints for DFG " << DFG << " (" << clean_dfg_name << "): " << endl;
			for (size_t i = 0; i < res_constr.size(); i++) {
				cout << "Type " << res_type[i] << ": " << res_constr[i] << "  ";
			}
			cout << endl << endl;
		}

		// If no resource constraint found for this DFG skip it
		if (res_constr.empty()) {
			cout << "Warning: No resource constraints found for DFG " << DFG << " (" << clean_dfg_name << "). Skipping this DFG." << endl;
			continue;
		}

		// END IMPLEMENTED BY SILVIA


		std::map<int, int> ops_schl_cc, ops_schl_FU, FU_type;
		ops_schl_cc.clear();
		ops_schl_FU.clear();
		FU_type.clear();

		//this is used to store ops in different types.
		std::map<int, int> types;

		// CHANGED BY SILVIA: flexible num of resourse types
		for (auto i = 0; i < delay.size(); i++)

		// END CHANGED BY SILVIA
			types[i] = 0;
		for (auto i = 0; i < opn; i++)
			types[ops[i].type]++;

		std::map<int, int> schlResult;
		schlResult.clear();

		std::map<int, int> FUAllocationResult;
		FUAllocationResult.clear();

		std::map<int, std::map<int, std::vector<int>>> bindingResult;
		bindingResult.clear();

		int actualLatency = 0;
		int latencyConstraint = 0;


		// CHANGED BY SILVIA

		// Start the timer
		auto start_time = std::chrono::high_resolution_clock::now();

		if (ls_base) {
			
			// STANDARD LS IMPLEMENTATION
			LS(schlResult, FUAllocationResult, bindingResult, actualLatency,
			ops, latencyConstraint, latencyParameter, delay, res_constr, false, debug, false, false);
		} else {
			
			// OUR IMPLEMENTATION
			LS_outer_loop(schlResult, FUAllocationResult, bindingResult, actualLatency,
			ops, latencyConstraint, latencyParameter, delay, res_constr, debug, featS, featP);
		}

		// Stop the timer
		auto end_time = std::chrono::high_resolution_clock::now();

		
        std::chrono::duration<double, std::milli> duration = end_time - start_time;
        double runtime_ms = duration.count();

		// END CHANGED BY SILVIA

		for (auto type = 0; type < FUAllocationResult.size(); type++){
			int reallyUsedFUs = 0;
			int numOfFUs = FUAllocationResult[type];
			
			for (auto fu = 0; fu < numOfFUs; fu++) {
				if (bindingResult.count(type) && 
					bindingResult[type].count(fu) && 
					!bindingResult[type][fu].empty()) {
					reallyUsedFUs++;
				}
			}
			FUAllocationResult[type] = reallyUsedFUs;
		}

		int total_FUs = 0;

		//the following part until the end is the output-file function which is used to generate output-files for checker.

		std::map<int, int> opBindingResult;
		opBindingResult.clear();

		int curr_idx = 0;
		int totalFUs = 0;

		for (auto type = 0; type < FUAllocationResult.size(); type++){
			int numOfFUs = FUAllocationResult[type];
			totalFUs += numOfFUs;
			for (auto fu = 0; fu < numOfFUs; fu++) 
				for (auto op = bindingResult[type][fu].begin(); op != bindingResult[type][fu].end(); op++) 
					opBindingResult[*op] = fu + curr_idx;
			curr_idx += numOfFUs;
		}

		cout << endl;
		cout << endl;

		std::cout << "For the current DFG " << DFG << ", the actual Latency is " << actualLatency << " (target latency is " << constraints_db[clean_dfg_name].targetLatency << ")," << " # of total FUs used = " << totalFUs << endl;

		//get output s&b result file.
		string DFGname;
		stringstream str(dfg_name);
		string tok;
		while (getline(str, tok, '.')){
			if (tok != "txt")
				DFGname = tok;
			//cout << tok << endl;
		}

		// IMPLEMENTED BY SILVIA
		// Dynamically create filename based on features and mode 

		string output_dir = "Results/" + to_string_with_precision(res_scaling_factor, 2) + "/";
		MAKE_DIR(output_dir.c_str());

		stringstream ssFileName;

		if (ls_base) {
		ssFileName << output_dir
				<< "Results_" << algName
				<< "_" << DFGname
				<< ".txt";
		} else {
			
		ssFileName << output_dir
				<< "Results_" << algName
				<< "_" << DFGname
				<< "_S" << featS 
				<< "_P" << featP 
				<< ".txt";
		}

		string output_sb_res = ssFileName.str();
		
		// END IMPLEMENTED BY SILVIA

		output_sb_result.open(output_sb_res, ios::out);


		// IMPLEMENTED BY SILVIA, UPDATED BY PLEASE

		// Comment: DFG name
		output_sb_result << "// The next line is the dfg name" << endl;
		output_sb_result << DFGname << endl;

		// Comment: FU parameters description
		output_sb_result << "// The next lines are FU parameters given as:" << endl;
		output_sb_result << "// <FU type>  <resource constraint>  <# of FUs used>  <FU delay>" << endl;

		// One line per FU type (non-comment lines 1..k)
		int numTypes = static_cast<int>(delay.size());
		for (int t = 0; t < numTypes; ++t) {
			std::string typeName = res_type[t];      // e.g., "ADD", "MUL", "DIV"
			std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::toupper);
			int rc   = res_constr[t];               // resource constraint
			int used = FUAllocationResult[t];       // # of FUs used for this type
			int d    = delay[t];                    // FU delay

			output_sb_result << typeName << " " << rc << " " << used << " " << d << endl;
		}

		// Line after FU params: actual latency
		output_sb_result << "actual latency " << actualLatency << endl;

		// Remaining lines: detailed S&B result
		// Format: "<oper-ID> <schl-time> <FU-binding ID>"
		for (int i = 0; i < opn; ++i) {
			output_sb_result << i << " " << schlResult[i] << " " << opBindingResult[i] << endl;
		}

		// Write results to a CSV file
        WriteResultToCSV(algName, DFGname, data_type, featS, featP, constraints_db[clean_dfg_name].targetLatency, actualLatency, totalFUs, runtime_ms, res_scaling_factor, ls_base);
        

		// END IMPLEMENTED BY SILVIA, UPDATED BY PLEASE


		output_sb_result.close();

		//end of output-file function part.
	}

	std::cout << "All DFGs are done." << endl;

	return 0;
}



// IMPLEMENTED BY SILVIA


// Helper function to format float to string with precision
std::string to_string_with_precision(float value, int n_decimals) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(n_decimals) << value;
    return out.str();
}



// Function to load resource constraints from a file 
void LoadConstraints(const string& filename, std::map<string, ConstraintData>& db) {

	// Open the file
    ifstream fin(filename);

    if (!fin.is_open()) {
        cerr << "Error: cannot open constraint file " << filename << endl;
        return;
    }

    string line;

    while (getline(fin, line)) {
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        stringstream ss(line);
        string name;
        int target, a, m, d, s;

		// Read target delay and constraints for each FU type
        ss >> name >> target >> a >> m >> d >> s; 

		// If reading was successful, store in db
        if (!ss.fail()) {
            db[name] = { target, {a, m, d, s} };
        }
    }

	// Close the file
    fin.close();
}



// Function to write results to a CSV file
void WriteResultToCSV(string algName, string dfgName, string data_type, bool featS, bool featP, int targetLat, int actualLat, int totalFUs, double runtimeMs, double res_scaling_factor, bool ls_base)
{    
    // Dynamically create filename based on features and mode 
    string clean_data_type = data_type.substr(1);
    
	string output_dir = "CSV/" + to_string_with_precision(res_scaling_factor, 2) + "/";
	MAKE_DIR(output_dir.c_str());

    stringstream ssFileName;

	if (ls_base) {
    ssFileName << output_dir
			   << "Results_" << algName
			   << "_" << clean_data_type 
               << ".csv";
	} else {
		
    ssFileName << output_dir
			   << "Results_" << algName
			   << "_" << clean_data_type 
               << "_S" << featS 
               << "_P" << featP 
               << ".csv";
	}

	printf("Writing results to CSV file: %s\n", ssFileName.str().c_str());
    
    string fileName = ssFileName.str();
    
    // Open the CSV file in append mode
    ofstream csvFile;
    csvFile.open(fileName, std::ios_base::app);
    
    // If new/empty file write the header
    csvFile.seekp(0, ios::end);
    if (csvFile.tellp() == 0) {
        csvFile << "DFG_Name,Target_Latency(FALLS),Actual_Latency(Project),Delta,Status,FUs_Used,Runtime_ms\n";
    }
    
    // Calculate status: if achieved better or equal latency than target it's a PASS
    string status = (actualLat <= targetLat) ? "PASS" : "FAIL";
    if (targetLat == -1) status = "NO_DATA";
    
    // Write data line
    csvFile << dfgName << "," 
            << targetLat << "," 
            << actualLat << "," 
            << (actualLat - targetLat) << ","
            << status << ","
            << totalFUs << ","
			<< runtimeMs << "\n";
            
    csvFile.close();
}

// END IMPLEMENTED BY SILVIA