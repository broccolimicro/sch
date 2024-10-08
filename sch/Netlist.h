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

	map<size_t, set<int> > cells;
	vector<Subckt> subckts; 

	int insert(int idx);
	int insert(const Subckt &cell);
	void erase(int idx);

	bool mapCells(Subckt &ckt);
	void mapCells();
	void build(phy::Library &lib, set<string> cellNames = set<string>());
};

string idToString(size_t id);

}
