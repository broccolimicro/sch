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
	for (auto cell = subckts.begin(); cell != subckts.end(); cell++) {
		if (cell->isCell) {
			ckt.findAndReplace(*cell);
		}
	}
	return ckt.mos.empty();
}

void Netlist::generateCells(Subckt &ckt) {
	
}

void Netlist::mapCells() {
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
