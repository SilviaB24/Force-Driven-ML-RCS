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
// END IMPLEMENTED BY SILVIA


using namespace std;

double latencyParameter = 1;	 //latency constant parameter, change this parameter can affect the latency constraint and hence, the final scheduling result is changed
int DFG = 0;					//DFG ID, this is automatically run from range 1 to 11
int LC = 0;						//global parameter latency constraint. LC = latency_parameter * ASAP latency.
int opn = 0;					 //# of operations in current DFG
int edge_num = 0;
//G_Node* ops; //operations list

void LS_outer_loop(std::map<int, int>& schlResult, std::map<int, int>& FUAllocationResult, std::map<int, std::map<int, std::vector<int>>>& bindingResult, int& actualLatency,
	std::map<int, G_Node>& ops, int& latencyConstraint, double& latencyParameter, std::vector<int>& delay, std::vector<int>& res_constr);

ofstream output_sb_result;

int main(int argc, char** argv)
{

	//read "lib"
	//get delay structure: only ADD/MUL are needed. you can change delay of ADD/MUL (# of cc's) in "lib.txt" file.
	string filename = "lib.txt";

	std::vector<int> delay, lp, dp;

	// IMPLEMENTED BY SILVIA
	std::vector<int> res_constr;
	std::vector<string> res_type;

	READ_LIB(filename, delay, lp, dp, res_type, res_constr);

	// END IMPLEMENTED BY SILVIA



	// print LIB info for debugging
	for (size_t i = 0; i < delay.size(); i++) {
		std::cout << "Function type: " << i << ", Delay: " << delay[i] << ", LP: " << lp[i] << ", DP: " << dp[i] << "\n";
	}

	//iterate all DFGs from 1 to 22 (the 16-22 are random DFGs)
	for (DFG = 1; DFG <= 24; DFG++) {

		std::map<int, G_Node> ops;
		LC = 0, opn = 0, edge_num = 0;
		ops.clear();

		string filename, dfg_name;
		Read_DFG(DFG, filename, dfg_name);			//read DFG filename
		readGraphInfo(filename, edge_num, opn, ops); //read DFG info

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

		LS_outer_loop(schlResult, FUAllocationResult, bindingResult, actualLatency,
			ops, latencyConstraint, latencyParameter, delay, res_constr);

		// END CHANGED BY SILVIA

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

		std::cout << "For the curren DFG " << DFG << ", the actual Latency is " << actualLatency << " (latency constraint is " << latencyConstraint << ")," << " # of total FUs used = " << totalFUs << endl;

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
		MAKE_DIR("Results");

		string output_sb_res = "Results/" + DFGname + "_LS_s&b_res.txt";
		
		// END IMPLEMENTED BY SILVIA

		output_sb_result.open(output_sb_res, ios::out);


		// IMPLEMENTED BY SILVIA

		for (auto i = 0; i < delay.size(); i++)
			// LINES 1, 2: function type, resource constraint
			output_sb_result << res_type[i] << " " << res_constr[i] << endl;

		for (auto i = 0; i < delay.size(); i++)
			// LINES 3, 4: function type, delay
			output_sb_result << res_type[i] << " " << delay[i] << endl;

		// LINE 5: DFG Name
		output_sb_result << "DFG name: " << DFGname << endl;

		// LINE 6: latency obtained
		output_sb_result << "Actual Latency " << actualLatency << endl;

		// LINE 7 -> END: detailed scheduling & binding result
		// Format: " <oper-ID> <schl-time>  <FU-binding ID>
		for (auto i = 0; i < opn; i++)
			output_sb_result << i << " " << schlResult[i] << " " << opBindingResult[i] << endl;


		// END IMPLEMENTED BY SILVIA


		output_sb_result.close();

		//end of output-file function part.
	}

	std::cout << "All DFGs are done." << endl;

	return 0;
}