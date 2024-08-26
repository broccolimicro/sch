#include "Netlist.h"
#include "Draw.h"
#include "Placer.h"
#include "Router.h"

#include <phy/Layout.h>
#include <set>

using namespace std;

Netlist::Netlist() {
	this->tech = nullptr;
}

Netlist::Netlist(const Tech &tech) {
	this->tech = &tech;
}

Netlist::~Netlist() {
}

void Netlist::build(set<string> cellNames) {
	for (int i = 0; i < (int)cells.size(); i++) {
		if (cellNames.empty() or cellNames.find(cells[i].name) != cellNames.end()) {
			printf("\rPlacing %s\n", cells[i].name.c_str());
			Placement::solve(*tech, &cells[i]);
			printf("\rRouting %s\n", cells[i].name.c_str());
			Router router(&cells[i]);
			router.solve(*tech);
			//router.print();
			printf("\rDone %s\n", cells[i].name.c_str());
		}
	}
}

