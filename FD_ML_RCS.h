#include <list>


using namespace std;

// Struct to store information about each operation node
struct G_Node 
{
	int id; //node ID
	int type; //node Function-type
	list<G_Node*> child;   // successor nodes (distance = 1)
	list<G_Node*> parent;  // predecessor nodes (distance = 1)
	int asap, alap;
	bool schl;
};