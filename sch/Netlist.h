#pragma once

#include "Subckt.h"

#include <phy/Tech.h>
#include <phy/Library.h>
#include <set>

using namespace std;

namespace sch {

struct Netlist {
	Netlist();
	Netlist(const Tech &tech);
	~Netlist();

	const Tech *tech;

	string libPath;
	
	vector<Subckt> subckts; 

	bool mapCells(Subckt &ckt);
	void mapCells();
	void build(phy::Library &lib, set<string> cellNames = set<string>());
};

}
