#pragma once
#include <stdint.h>
#include <tchar.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <ctime>
#include <list>
#include <vector>
#include <SDKDDKVer.h>
#include <algorithm>
#include <queue>
#include <sstream>

using namespace std;

struct CmpByValue {
	bool operator()(const std::pair<int, double>& lhs, const std::pair<int, double>& rhs) {
		return lhs.second < rhs.second;
	}
};

struct G_Node    //save the info for operation node
{
	int id; //node ID
	int type; //node Function-type
	list<G_Node*> child;   // successor nodes (distance = 1)
	list<G_Node*> parent;  // predecessor nodes (distance = 1)
	int asap, alap;
	bool schl;

	// IMPLEMENTED BY SILVIA
	int criticalSuccessorId; // ID of the critical successor node
	float priority1; // priority value for FDS-based scheduling
	float priority2; // second priority value for FDS-based scheduling
	int priority3; // third priority value for FDS-based scheduling
	// END IMPLEMENTED BY SILVIA
};

//Local-PG part:

//input S&B solution storage structure

/*
	What S&B solution is easiler to access:

	using 2-D map:

	1-D: FUs
	2-D: Schedulings
*/

struct S
{
	std::map<int, int> schedule; //<operation, sch'ed cc>
	std::map<int, int> bind; //<operation, FU>
	std::map<int, int> fu_info; //FU-id, FU-type(int)>
};

struct Lib
{
	std::map<string, int> fu_type; //<str-name, type-id>
	std::map<int, int> delay; //<type-id, delay>
	std::map<int, double> dp; //<type-id, dp>
	std::map<int, double> lp; //<type-id, lp>
};


void READ_LIB(const string& file_name,
	vector<int>& delay,
	vector<int>& lp,
	vector<int>& dp);


//------------------INPUT FUNCTION-----------------//

void Read_DFG(int& DFG, string& filename, string& dfg_name); //Read-DFG filename

void readGraphInfo(string& filename, int& edge_num, int& opn, std::map<int, G_Node>& ops);

//			New Change for ML-RCS: adding "int rc[2]"
int get_S_structure(string& filename, string& DFG_name, std::map<int, string>& FU_type, std::map<int, int>& FU_constraint, std::map<int, int>& reported_FUs, std::map<int, int>& FU_delay, std::map<string, int>& FU_type_name_to_id,
	int& actual_latency_reported, S& s);