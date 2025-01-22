#pragma once

#include "Subckt.h"

#include <phy/Tech.h>

#include <vector>
#include <map>
#include <set>
#include <stdint.h>

using namespace std;

namespace sch {

struct Netlist {
	Netlist(const Tech &tech);
	~Netlist();

	const Tech *tech;

	map<size_t, set<int> > cells;
	vector<Subckt> subckts; 

	int insert(int idx);
	int insert(const Subckt &cell);
	void erase(int idx);

	void mapCells(bool progress=false);

	int cellAt(int root, size_t index) const;
	size_t countCells(int root) const;
};

string idToString(size_t id);

}
