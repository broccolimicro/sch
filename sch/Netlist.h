#pragma once

#include "Subckt.h"

#include <phy/Tech.h>
#include <set>

using namespace std;

struct Netlist {
	Netlist();
	Netlist(const Tech &tech);
	~Netlist();

	const Tech *tech;

	string libPath;
	
	vector<Subckt> cells; 

	void build(set<string> cellNames = set<string>());
};

