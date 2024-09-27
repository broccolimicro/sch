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
	
	// This is equivalent to the problem of identifying the maximal clique in the
	// graph that minimizes number of nodes while maximizing coverage. The graph
	// is constructed using the subsets in "options" as vertices and creating
	// edges between each pair that doesn't overlap. This is an NP-complete
	// problem and we are solving it using the Bron–Kerbosch algorithm.
	int covered = 0;
	vector<int> result;
	
	struct BronKerboschFrame {
		vector<int> R, P, X;
	};

	vector<BronKerboschFrame> frames;
	frames.push_back(BronKerboschFrame());
	frames.back().P.reserve(options.size());
	for (int i = 0; i < (int)options.size(); i++) {
		frames.back().P.push_back(i);
	}

	while (not frames.empty()) {
		auto frame = frames.back();
		frames.pop_back();

		if (frame.P.empty() and frame.X.empty()) {
			int newCovered = 0;
			for (auto r = frame.R.begin(); r != frame.R.end(); r++) {
				newCovered += (int)options[*r].devices.size();
			}

			// Then we've found a maximal clique
			if (newCovered > covered or (newCovered == covered and frame.R.size() < result.size())) {
				result = frame.R;
				covered = newCovered;
			}
		} else {
			// Otherwise, we need to recurse
			while (not frame.P.empty()) {
				frames.push_back(frame);
				frames.back().R.push_back(frame.P.back());
				auto m0 = options.begin() + frame.P.back();
				for (int i = (int)frames.back().P.size()-1; i >= 0; i--) {
					auto m1 = options.begin() + frames.back().P[i];
					if (m0->overlapsWith(*m1)) {
						frames.back().P.erase(frames.back().P.begin() + i);
					}
				}
				for (int i = (int)frames.back().X.size()-1; i >= 0; i--) {
					auto m1 = options.begin() + frames.back().X[i];
					if (m0->overlapsWith(*m1)) {
						frames.back().X.erase(frames.back().X.begin() + i);
					}
				}

				frame.X.push_back(frame.P.back());
				frame.P.pop_back();
			}
		}
	}

	// Now we have the best cell mapping. We need to apply it.
	for (int i = 0; i < (int)result.size(); i++) {
		ckt.apply(options[result[i]]);
		for (int j = i+1; j < (int)result.size(); j++) {
			options[result[j]].apply(options[result[i]]);
		}
	}

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
