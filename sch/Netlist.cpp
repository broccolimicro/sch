#include "Netlist.h"
#include "Draw.h"
#include "CellPlacer.h"
#include "CellRouter.h"

#include <common/text.h>

#include <chrono>

using namespace std::chrono;
using namespace std;

namespace sch {

Netlist::Netlist() {
}

Netlist::~Netlist() {
}

int Netlist::insert(int idx) {
	auto pos = cells.insert(pair<size_t, set<int> >(subckts[idx].id, set<int>()));
	for (auto j = pos.first->second.begin(); j != pos.first->second.end(); j++) {
		if (*j == idx) {
			return idx;
		} else if (subckts[*j] == subckts[idx]) {
			int result = *j > idx ? *j-1 : *j;
			erase(idx);
			return result;
		}
	}
	pos.first->second.insert(idx);
	return idx;
}

int Netlist::insert(const Subckt &cell) {
	auto pos = cells.insert(pair<size_t, set<int> >(cell.id, set<int>()));
	int index = (int)subckts.size();
	for (auto j = pos.first->second.begin(); j != pos.first->second.end(); j++) {
		if (subckts[*j] == cell) {
			index = *j;
			break;
		}
	}
	if (index >= (int)subckts.size()) {
		pos.first->second.insert(index);
		subckts.push_back(cell);
		toLayout.push_back(Mapping<int>(-1, true));
	}
	return index;
}

void Netlist::erase(int idx) {
	for (auto i = cells.begin(); i != cells.end(); ) {
		set<int> updated;
		for (auto j = i->second.begin(); j != i->second.end(); j++) {
			if (*j < idx) {
				updated.insert(*j);
			} else if (*j > idx) {
				updated.insert(*j-1);
			}
		}
		if (updated.empty()) {
			i = cells.erase(i);
		} else {
			i->second = updated;
			i++;
		}
	}

	subckts.erase(subckts.begin()+idx);
	toLayout.erase(toLayout.begin()+idx);
}

void Netlist::mapCells(const Tech &tech, bool progress) {
	// check existing cells
	for (int i = (int)subckts.size()-1; i >= 0; i--) {
		if (subckts[i].isCell and not subckts[i].mos.empty()) {
			subckts[i].canonicalize();
			insert(i);
		}
	}

	for (int i = (int)subckts.size()-1; i >= 0; i--) {
		if (not sch::mapCells(tech, *this, i, nullptr, progress)) {
			printf("failed to segment all devices\n");
		}
	}
}

bool mapCells(const Tech &tech, Netlist &net, int idx, vector<int> *cells, bool progress) {
	if (net.subckts[idx].isCell and not net.subckts[idx].mos.empty()) {
		net.subckts[idx].canonicalize();
		net.insert(idx);
		return true;
	} else if (net.subckts[idx].mos.empty()) {
		return true;
	}

	net.subckts[idx].splitDevices(tech);

	auto segments = net.subckts[idx].segment();
	for (auto s = segments.begin(); s != segments.end(); s++) {
		Subckt cell(true);
		Mapping<int> m = s->generate(cell, net.subckts[idx]);
		m *= cell.canonicalize();
		// TODO(edward.bingham) clean dangling?
		cell.name = "cell_" + encodeBase32(cell.id);
		int index = net.insert(cell);
		if (cells != nullptr) {
			cells->push_back(index);
		}

		net.subckts[idx].extract(*s);
		net.subckts[idx].push(Instance(net.subckts[index], m.flip(), index));

		//print();
		for (auto s1 = s+1; s1 != segments.end(); s1++) {
			// DESIGN(edward.bingham) if two segments overlap, then we just remove
			// the extra devices from one of the segments. It's only ok to have those
			// devices in a different cell if the signals connecting them don't
			// switch (for example, shared weak ground). Otherwise it's an isochronic
			// fork assumption violation.

			if (not s1->extract(*s)) {
				printf("internal %s:%d: overlapping cells found\n", __FILE__, __LINE__);
			}
		}
	}

	net.subckts[idx].cleanDangling();
	if (cells != nullptr) {
		sort(cells->begin(), cells->end());
		cells->erase(unique(cells->begin(), cells->end()), cells->end());
	}
	return net.subckts[idx].mos.empty();
}

int Netlist::cellAt(int root, size_t index) const {
	while (root < (int)subckts.size() and not subckts[root].inst.empty()) {
		int next = (int)(index % (size_t)subckts[root].inst.size());
		index /= (size_t)subckts[root].inst.size();
		root = subckts[root].inst[next].subckt;
	}
	return root;
}

size_t Netlist::countCells(int root) const {
	vector<size_t> sub;
	sub.resize(subckts.size(), std::numeric_limits<size_t>::max());
	
	vector<int> stack(1, root);
	while (not stack.empty()) {
		int curr = stack.back();
		auto currCkt = subckts.begin()+curr;
		
		bool done = true;
		for (auto i = currCkt->inst.begin(); i != currCkt->inst.end(); i++) {
			if (sub[i->subckt] == std::numeric_limits<size_t>::max()) {
				done = false;
				stack.erase(remove(stack.begin(), stack.end(), i->subckt), stack.end());
				stack.push_back(i->subckt);
			}
		}

		if (done) {
			sub[curr] = 0;
			for (auto i = currCkt->inst.begin(); i != currCkt->inst.end(); i++) {
				sub[curr] += sub[i->subckt] == 0 ? 1 : sub[i->subckt];
			}
			stack.pop_back();
		}
	}
	return sub[root];
}

void Netlist::mapToLayout(int idx, const Layout &layout) {
	if (idx >= (int)toLayout.size()) {
		toLayout.resize(idx+1, Mapping<int>(-1, true));
	}
	toLayout[idx] = subckts[idx].mapToLayout(layout);
}

}
