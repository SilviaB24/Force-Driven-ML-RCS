#pragma once

#include <stdint.h>
// #include <tchar.h> on Windows
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <ctime>
#include <list>
#include <vector>
// #include <SDKDDKVer.h> on Windows
#include <algorithm>
#include <queue>
#include <sstream>

#define MAX_ITERATIONS 100

// #define _CRT_SECURE_NO_WARNINGS on Windows

using namespace std;

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

void READ_LIB(const string& file_name,
	vector<int>& delay,
	vector<int>& lp,
	vector<int>& dp);


void Read_DFG(int& DFG, string& filename, string& dfg_name); //Read-DFG filename
void readGraphInfo(string& filename, int& edge_num, int& opn, std::map<int, G_Node>& ops);




