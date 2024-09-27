#include "Netlist.h"
#include "Draw.h"
#include "Placer.h"
#include "Router.h"

#include <phy/Layout.h>
#include <set>

using namespace std;

namespace sch {

Netlist::Netlist() {
	this->tech = nullptr;
}

Netlist::Netlist(const Tech &tech) {
	this->tech = &tech;
}

Netlist::~Netlist() {
}

bool Netlist::mapCells(Subckt &ckt) {
	vector<Mapping> options;
	for (auto cell = subckts.begin(); cell != subckts.end(); cell++) {
		if (ckt.mos.empty()) {
			return true;
		}

		if (cell->isCell) {
			auto found = ckt.find(*cell, cell-subckts.begin());
			options.insert(options.end(), found.begin(), found.end());
		}
	}

	// Now we have a set of possible mappings. Those mappings may overlap. We
	// want to find a selection of mappings that minimizes the total number of
	// cells used to cover the maximum possible number of devices in this subckt
	
	
	

	return ckt.mos.empty();
}

void Netlist::generateCells(Subckt &ckt) {
	
}

void Netlist::mapCells() {
	// TODO(edward.bingham) create a Trie using the device graphs of the cells.
	// Identify common sub-patterns to inform the structure of the trie. Then
	// save the resulting trie in a ascii format to the cell directory. Use that
	// to dramatically accelerate cell mapping for excessively large cell
	// databases. However, this will take a while to generate, so if it doesn't
	// already exist then we should fall back to the base cell mapping algorthms.

	for (auto ckt = subckts.begin(); ckt != subckts.end(); ckt++) {
		if (not ckt->isCell or ckt->mos.empty()) {
			continue;
		}

		if (mapCells(*ckt)) {
			continue;
		}

		generateCells(*ckt);
	}
}

void Netlist::build(set<string> cellNames) {
	for (auto ckt = subckts.begin(); ckt != subckts.end(); ckt++) {
		if (ckt->isCell and (cellNames.empty() or cellNames.find(ckt->name) != cellNames.end())) {
			printf("\rPlacing %s\n", ckt->name.c_str());
			Placement pl = Placement::solve(*ckt);
			printf("\rRouting %s\n", ckt->name.c_str());
			Router rt(*tech, pl);
			rt.solve(*tech);
			//rt.print();
			printf("\rDone %s\n", ckt->name.c_str());
		}
	}
}

}
