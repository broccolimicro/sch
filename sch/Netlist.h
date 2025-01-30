#pragma once

#include "Subckt.h"

#include <phy/Tech.h>

#include <vector>
#include <map>
#include <set>
#include <stdint.h>

#include <ucs/mapping.h>

using namespace std;

namespace sch {

struct Netlist {
	Netlist(const Tech &tech);
	~Netlist();

	const Tech *tech;

	map<size_t, set<int> > cells;
	vector<Subckt> subckts; 
	vector<ucs::mapping> toLayout;

	int insert(int idx);
	int insert(const Subckt &cell);
	void erase(int idx);

	void mapCells(bool progress=false);

	int cellAt(int root, size_t index) const;
	size_t countCells(int root) const;

	void mapToLayout(int idx, const Layout &layout);
};

string idToString(size_t id);

}
