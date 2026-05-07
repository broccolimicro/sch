#pragma once

#include "Subckt.h"

#include <phy/Tech.h>

#include <vector>
#include <map>
#include <set>
#include <stdint.h>

#include <common/mapping.h>

using namespace std;

namespace sch {

struct Netlist {
	Netlist();
	~Netlist();

	// DESIGN(edward.bingham) This provides a fast lookup mechanism while
	// respecting hash collisions.
	// subckt id (hash) -> index into subckts
	map<size_t, set<int> > cells;

	// DESIGN(edward.bingham) These have matching indices, one is the
	// subckt definition, the other is a mapping of nets in the subckt to
	// the associated nets in the layout.
	vector<Subckt> subckts;
	vector<Mapping<int> > toLayout;

	int insert(int idx);
	int insert(const Subckt &cell);
	void erase(int idx);

	void mapCells(const Tech &tech, bool progress=false);
	void mapToLayout(int idx, const Layout &layout);
	int cellAt(int root, size_t index) const;
	size_t countCells(int root) const;
};

}
