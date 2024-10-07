#include <limits>
#include <algorithm>
#include <string>
#include <set>

#include "Mapping.h"
#include "Subckt.h"

using namespace std;

namespace sch {

Mapping::Mapping() {
}

// Create an identity mapping
Mapping::Mapping(const Subckt &cell) {
	identity(cell);
}

Mapping::~Mapping() {
}

int Mapping::pushNet(int net) {
	for (int i = 0; i < (int)cellToThis.size(); i++) {
		if (cellToThis[i] == net) {
			return i;
		}
	}

	int result = cellToThis.size();
	cellToThis.push_back(net);
	return result;
}

void Mapping::identity(const Subckt &from) {
	cellToThis.clear();
	devices.clear();

	cellToThis.reserve(from.nets.size());
	for (int i = 0; i < (int)from.nets.size(); i++) {
		cellToThis.push_back(i);
	}
	devices.reserve(from.mos.size());
	for (int i = 0; i < (int)from.mos.size(); i++) {
		devices.push_back(i);
	}
}

bool Mapping::extract(const Mapping &m) {
	bool success = true;
	int j = (int)m.devices.size()-1;
	for (int i = (int)devices.size()-1; i >= 0; i--) {
		while (devices[i] < m.devices[j]) {
			j--;
		}

		if (devices[i] == m.devices[j]) {
			success = false;
			devices.erase(devices.begin() + i);
		} else {
			devices[i] -= (j+1);
		}
	}

	return success;
}

void Mapping::apply(const Mapping &m) {
	// this: cell -> main
	// m: canon -> cell
	// want: canon -> main

	vector<int> nets;
	nets.reserve(m.cellToThis.size());
	for (int i = 0; i < (int)m.cellToThis.size(); i++) {
		nets.push_back(cellToThis[m.cellToThis[i]]);
	}
	cellToThis = nets;
}

void Mapping::merge(const Mapping &m) {
	for (auto i = m.cellToThis.begin(); i != m.cellToThis.end(); i++) {
		bool found = false;
		for (auto j = cellToThis.begin(); j != cellToThis.end() and not found; j++) {
			found = found or (*i == *j);
		}
		if (not found) {
			cellToThis.push_back(*i);
		}
	}

	devices.insert(devices.end(), m.devices.begin(), m.devices.end());
	sort(devices.begin(), devices.end());
	devices.erase(unique(devices.begin(), devices.end()), devices.end());
}

Mapping &Mapping::remap(vector<int> nets) {
	vector<int> updated;
	updated.reserve(nets.size());
	for (int i = 0; i < (int)nets.size(); i++) {
		updated.push_back(cellToThis[nets[i]]);
	}
	cellToThis = updated;
	return *this;
}

bool Mapping::overlapsWith(const Mapping &m) const {
	int i = 0, j = 0;
	while (i < (int)devices.size() and j < (int)m.devices.size()) {
		if (devices[i] == m.devices[j]) {
			return true;
		} else if (devices[i] < m.devices[j]) {
			i++;
		} else {
			j++;
		}
	}
	return false;
}

Subckt Mapping::generate(const Subckt &from, string name, bool isCell) const {
	Subckt cell;	
	cell.name = name;
	cell.isCell = isCell;

	for (auto i = cellToThis.begin(); i != cellToThis.end(); i++) {
		auto n = from.nets.begin()+*i;

		bool isIO = n->isIO or not n->portOf.empty();
		for (int type = 0; type < 2 and not isIO; type++) {
			for (auto j = n->gateOf[type].begin(); j != n->gateOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(devices.begin(), devices.end(), *j);
				isIO = (pos != devices.end() and *pos == *j);
			}
			for (auto j = n->sourceOf[type].begin(); j != n->sourceOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(devices.begin(), devices.end(), *j);
				isIO = (pos != devices.end() and *pos == *j);
			}
			for (auto j = n->drainOf[type].begin(); j != n->drainOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(devices.begin(), devices.end(), *j);
				isIO = (pos != devices.end() and *pos == *j);
			}
		}

		cell.pushNet(n->name, isIO);
	}

	for (auto i = devices.begin(); i != devices.end(); i++) {
		auto d = from.mos.begin()+*i;
		int gate = -1, source = -1, drain = -1, base = -1;
		for (int j = 0; j < (int)cellToThis.size(); j++) {
			if (d->gate == cellToThis[j]) {
				gate = j;
			}
			if (d->source == cellToThis[j]) {
				source = j;
			}
			if (d->drain == cellToThis[j]) {
				drain = j;
			}
			if (d->base == cellToThis[j]) {
				base = j;
			}
		}

		if (gate < 0 or source < 0 or drain < 0 or base < 0) {
			printf("internal %s:%d: cell net map missing nets\n", __FILE__, __LINE__);
		}

		cell.pushMos(d->model, d->type, drain, gate, source, base, d->size);
		cell.mos.back().params = d->params;
	}

	for (auto i = cell.nets.begin(); i != cell.nets.end(); i++) {
		if (i->isIO and i->isOutput()) {
			i->name = "o" + to_string(i-cell.nets.begin());
		} else if (i->isIO and i->isInput()) {
			i->name = "i" + to_string(i-cell.nets.begin());
		} else if (not i->isIO) {
			i->name = "_" + to_string(i-cell.nets.begin());
		}
	}

	return cell;
}

void Mapping::print() const {
	printf("mapping\n");
	printf("nets:{");
	for (int i = 0; i < (int)cellToThis.size(); i++) {
		if (i != 0) {
			printf(", ");
		}
		printf("%d -> %d", i, cellToThis[i]);
	}
	printf("}\ndevices:{");
	for (int i = 0; i < (int)devices.size(); i++) {
		if (i != 0) {
			printf(", ");
		}
		printf("%d", devices[i]);
	}
	printf("}\n\n");
}

}
