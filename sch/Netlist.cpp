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

void Netlist::build(set<string> cellNames) {
	for (auto ckt = subckts.begin(); ckt != subckts.end(); ckt++) {
		if (cellNames.empty() or cellNames.find(ckt->name) != cellNames.end()) {
			printf("\rPlacing %s\n", ckt->name.c_str());
			Placement::solve(*tech, &(*ckt));
			printf("\rRouting %s\n", ckt->name.c_str());
			Router router(&(*ckt));
			router.solve(*tech);
			//router.print();
			printf("\rDone %s\n", ckt->name.c_str());
		}
	}
}

}
