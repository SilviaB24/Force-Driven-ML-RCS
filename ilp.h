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

#define _CRT_SECURE_NO_WARNINGS

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


void output_schedule(string str, std::map<int, G_Node>& ops); //print the final scheduling result
//------------------INPUT FUNCTION-----------------//

//----------------FDS Core Function----------------//
void FDS(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC, double& latency_parameter); //main function FDS
void ASAP(std::map<int, G_Node>& ops, std::vector<int>& delay);
int checkParent(G_Node* op, std::vector<int>& delay);
void ALAP(std::map<int, G_Node>& ops, std::vector<int>& delay, int& LC);
int checkChild(G_Node* op, std::vector<int>& delay, int& LC);
void getLC(int& LC, double& latency_parameter, std::map<int, G_Node>& ops, std::vector<int>& delay);

//update the depth for every ops, and obtain topo-order.
void update_depth(std::map<int, int>& op_depth, vector<int>& topo_order, std::map<int, G_Node>& ops);

//get each operation's updated operation list in fanin/fanout directions based on the targeted depth.
void get_pr_su_update_list(std::map<int, G_Node>& ops, std::map<int, std::vector<int>>& ops_update_pr_list,
	std::map<int, std::vector<int>>& ops_update_su_list,
	std::map<int, int>& op_depth,
	int& depth_limit);