#include "Subckt.h"
#include "Draw.h"
#include <limits>
#include <algorithm>
#include <string>
#include <set>

using namespace std;

namespace sch {

Mos::Mos() {
	model = -1;
	type = -1;
	drain = -1;
	gate = -1;
	source = -1;
	base = -1;
	size = vec2i(1.0,1.0);
}

Mos::Mos(int model, int type) {
	this->model = model;
	this->type = type;
	this->size = vec2i(1.0,1.0);
	this->drain = -1;
	this->gate = -1;
	this->source = -1;
	this->base = -1;
}

Mos::Mos(int model, int type, int drain, int gate, int source, int base, vec2i size) {
	this->model = model;
	this->type = type;
	this->size = vec2i(1.0,1.0);
	this->drain = drain;
	this->gate = gate;
	this->source = source;
	this->base = base < 0 ? source : base;
	this->size = size;
}

Mos::~Mos() {
}

int Mos::left(bool flip) const {
	return flip ? drain : source;
}

int Mos::right(bool flip) const {
	return flip ? source : drain;
}

bool operator==(const Mos &m0, const Mos &m1) {
	return m0.model == m1.model
		and m0.type == m1.type
		and m0.size == m1.size
		and m0.params == m1.params;
}

bool operator!=(const Mos &m0, const Mos &m1) {
	return m0.model != m1.model
		or m0.type != m1.type 
		or m0.size != m1.size 
		or m0.params != m1.params;
}

Net::Net() {
	isIO = false;
}

Net::Net(string name, bool isIO) {
	this->name = name;
	this->isIO = isIO;
	this->remoteIO = isIO;
}

Net::~Net() {
}

int Net::ports(int type) const {
	return (int)(sourceOf[type].size() + drainOf[type].size());
} 

bool Net::hasContact(int type) const {
	return remoteIO
		or ports(type) > 2
		or ports(1-type) != 0
		or (gateOf[0].size()+gateOf[1].size()) != 0;
}

bool Net::isPairedGate() const {
	return (int)gateOf[0].size() == 1
		and (int)gateOf[1].size() == 1;
}

bool Net::isPairedDriver() const {
	return ports(0) == 1 and ports(1) == 1;
}

bool Net::isOutput() const {
	return not drainOf[0].empty() or not drainOf[1].empty();
}

bool Net::isInput() const {
	return (gateOf[0].size()+gateOf[1].size()) > (sourceOf[0].size()+sourceOf[1].size());
}

bool Net::connectedTo(int net) const {
	return find(remote.begin(), remote.end(), net) != remote.end();
}

bool Net::dangling(bool remIO) const {
	return (remIO or not remoteIO)
		and gateOf[0].empty()
		and gateOf[1].empty()
		and sourceOf[0].empty()
		and sourceOf[1].empty()
		and drainOf[0].empty()
		and drainOf[1].empty()
		and portOf.empty();
}

bool Net::isAnonymous() const {
	if (name.empty()) {
		return true;
	}

	if (name[0] != '#' and name[0] != '_') {
		return false;
	}

	for (int i = 1; i < (int)name.size(); i++) {
		if (name[i] < '0' or name[i] > '9') {
			return false;
		}
	}
	return true;
}

Mapping::Mapping() {
	this->index = -1;
	this->cell = nullptr;
}

Mapping::Mapping(const Subckt *cell, int index) {
	this->index = index;
	this->cell = cell;
	this->cellToThis.resize(cell->nets.size(), -1);
}

Mapping::~Mapping() {
}

Subckt Mapping::generate(const Subckt &main) {
	Subckt cell;	
	cell.name = "cell_" + to_string(index);
	cell.isCell = true;

	for (auto i = cellToThis.begin(); i != cellToThis.end(); i++) {
		auto n = main.nets.begin()+*i;

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
		auto d = main.mos.begin()+*i;
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

bool Mapping::extract(const Mapping &m) {
	printf("extracting mapping from main\n");
	m.print();
	print();

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

	printf("done %d\n", success);
	print();

	return success;
}

void Mapping::apply(const Mapping &m) {
	// this: cell -> main
	// m: canon -> cell
	// want: canon -> main

	vector<int> nets;
	nets.reserve(cellToThis.size());
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

Instance Mapping::instance() const {
	Instance result;
	result.subckt = index;
	for (auto p = cell->ports.begin(); p != cell->ports.end(); p++) {
		result.ports.push_back(cellToThis[*p]);
	}
	return result;
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

bool Mapping::coupledWith(const Subckt &main, const Mapping &m) const {
	std::set<int> fromA, toA, fromB, toB;
	for (auto d = devices.begin(); d != devices.end(); d++) {
		fromA.insert(main.mos[*d].drain);
		toA.insert(main.mos[*d].gate);
		toA.insert(main.mos[*d].source);
		toA.insert(main.mos[*d].base);
	}
	for (auto d = m.devices.begin(); d != m.devices.end(); d++) {
		fromB.insert(main.mos[*d].drain);
		toB.insert(main.mos[*d].gate);
		toB.insert(main.mos[*d].source);
		toB.insert(main.mos[*d].base);
	}
	bool hasAtoB = false, hasBtoA = false;
	for (auto a = fromA.begin(); a != fromA.end() and not hasAtoB; a++) {
		hasAtoB = (toB.find(*a) != toB.end());
	}
	for (auto b = fromB.begin(); b != fromB.end() and not hasBtoA; b++) {
		hasBtoA = (toA.find(*b) != toA.end());
	}
	return (hasAtoB and hasBtoA);
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

void Mapping::print() const {
	printf("mapping %d\n", index);
	printf("\tcell -> this\n");
	for (int i = 0; i < (int)cellToThis.size(); i++) {
		printf("\t%d -> %d\n", i, cellToThis[i]);
	}
	printf("{");
	for (int i = 0; i < (int)devices.size(); i++) {
		if (i != 0) {
			printf(", ");
		}
		printf("%d", devices[i]);
	}
	printf("}\n\n");
}

Subckt::Subckt() {
	isCell = false;
}

Subckt::~Subckt() {
}

int Subckt::findNet(string name, bool create) {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}
	if (create) {
		return pushNet(name);
	}
	return -1;
}

string Subckt::netName(int net) const {
	if (net < 0) {
		return "_";
	}

	return nets[net].name;
}

int Subckt::pushNet(string name, bool isIO) {
	int result = (int)nets.size();
	nets.push_back(Net(name, isIO));
	nets.back().remote.push_back(result);
	if (isIO) {
		ports.push_back(result);
	}
	return result;
}

void Subckt::popNet(int index) {
	nets.erase(nets.begin()+index);

	for (int i = (int)ports.size()-1; i >= 0; i--) {
		if (ports[i] > index) {
			ports[i]--;
		} else if (ports[i] == index) {
			ports.erase(ports.begin()+i);
		}
	}

	for (auto d = mos.begin(); d != mos.end(); d++) {
		if (d->gate > index) {
			d->gate--;
		} else if (d->gate == index) {
			d->gate = -1;
		}

		if (d->source > index) {
			d->source--;
		} else if (d->source == index) {
			d->source = -1;
		}

		if (d->drain > index) {
			d->drain--;
		} else if (d->drain == index) {
			d->drain = -1;
		}

		if (d->base > index) {
			d->base--;
		} else if (d->base == index) {
			d->base = -1;
		}
	}
}

void Subckt::connectRemote(int n0, int n1) {
	nets[n0].remote.push_back(n1);
	nets[n1].remote.push_back(n0);

	for (int type = 0; type < 2; type++) {
		nets[n0].gateOf[type].insert(nets[n0].gateOf[type].end(), nets[n1].gateOf[type].begin(), nets[n1].gateOf[type].end());
		sort(nets[n0].gateOf[type].begin(), nets[n0].gateOf[type].end());
		nets[n0].gateOf[type].erase(unique(nets[n0].gateOf[type].begin(), nets[n0].gateOf[type].end()), nets[n0].gateOf[type].end());
		nets[n1].gateOf[type] = nets[n0].gateOf[type];

		nets[n0].drainOf[type].insert(nets[n0].drainOf[type].end(), nets[n1].drainOf[type].begin(), nets[n1].drainOf[type].end());
		sort(nets[n0].drainOf[type].begin(), nets[n0].drainOf[type].end());
		nets[n0].drainOf[type].erase(unique(nets[n0].drainOf[type].begin(), nets[n0].drainOf[type].end()), nets[n0].drainOf[type].end());
		nets[n1].drainOf[type] = nets[n0].drainOf[type];

		nets[n0].sourceOf[type].insert(nets[n0].sourceOf[type].end(), nets[n1].sourceOf[type].begin(), nets[n1].sourceOf[type].end());
		sort(nets[n0].sourceOf[type].begin(), nets[n0].sourceOf[type].end());
		nets[n0].sourceOf[type].erase(unique(nets[n0].sourceOf[type].begin(), nets[n0].sourceOf[type].end()), nets[n0].sourceOf[type].end());
		nets[n1].sourceOf[type] = nets[n0].sourceOf[type];
	}
	nets[n0].portOf.insert(nets[n0].portOf.end(), nets[n1].portOf.begin(), nets[n1].portOf.end());
	sort(nets[n0].portOf.begin(), nets[n0].portOf.end());
	nets[n0].portOf.erase(unique(nets[n0].portOf.begin(), nets[n0].portOf.end()), nets[n0].portOf.end());
	nets[n1].portOf = nets[n0].portOf;

	nets[n0].remoteIO = nets[n0].remoteIO or nets[n1].remoteIO;
	nets[n1].remoteIO = nets[n0].remoteIO;
}

int Subckt::pushMos(int model, int type, int drain, int gate, int source, int base, vec2i size) {
	int result = (int)mos.size();
	for (auto i = nets[drain].remote.begin(); i != nets[drain].remote.end(); i++) {
		nets[*i].drainOf[type].push_back(result);
	}
	for (auto i = nets[source].remote.begin(); i != nets[source].remote.end(); i++) {
		nets[*i].sourceOf[type].push_back(result);
	}
	for (auto i = nets[gate].remote.begin(); i != nets[gate].remote.end(); i++) {
		nets[*i].gateOf[type].push_back(result);
	}

	mos.push_back(Mos(model, type, drain, gate, source, base, size));
	return result;
}

void Subckt::popMos(int index) {
	mos.erase(mos.begin() + index);

	for (auto n = nets.begin(); n != nets.end(); n++) {
		for (int type = 0; type < 2; type++) {
			for (int j = (int)n->gateOf[type].size()-1; j >= 0; j--) {
				if (n->gateOf[type][j] > index) {
					n->gateOf[type][j]--;
				} else if (n->gateOf[type][j] == index) {
					n->gateOf[type].erase(n->gateOf[type].begin()+j);
				}
			}
			for (int j = (int)n->sourceOf[type].size()-1; j >= 0; j--) {
				if (n->sourceOf[type][j] > index) {
					n->sourceOf[type][j]--;
				} else if (n->sourceOf[type][j] == index) {
					n->sourceOf[type].erase(n->sourceOf[type].begin()+j);
				}
			}
			for (int j = (int)n->drainOf[type].size()-1; j >= 0; j--) {
				if (n->drainOf[type][j] > index) {
					n->drainOf[type][j]--;
				} else if (n->drainOf[type][j] == index) {
					n->drainOf[type].erase(n->drainOf[type].begin()+j);
				}
			}
		}
	}
}

vector<Mapping> Subckt::find(const Subckt &cell, int index) {
	struct frame {
		frame(const Subckt &cell, int index) : m(&cell, index) {
			todo.reserve(cell.mos.size());
			for (int i = 0; i < (int)cell.mos.size(); i++) {
				todo.push_back(i);
			}
		}
		~frame() {}

		Mapping m;
		vector<int> todo;  // devs in cell
	};

	vector<Mapping> result;
	vector<frame> frames(1, frame(cell, index));
	while (not frames.empty()) {
		auto curr = frames.back();
		frames.pop_back();

		// TODO(edward.bingham) Be more intelligent about target selection, using
		// the graph structures so we don't have to do a linear-time search
		auto target = cell.mos.begin() + curr.todo.back();
		curr.todo.pop_back();

		for (auto dev = mos.begin(); dev != mos.end(); dev++) {
			if (*dev != *target) {
				continue;
			}

			if (dev->gate >= (int)curr.m.cellToThis.size()) {
				curr.m.cellToThis.resize(dev->gate+1, -1);
			}
			if (curr.m.cellToThis[dev->gate] < 0) {
				// this net hasn't been assigned yet
				curr.m.cellToThis[dev->gate] = target->gate;
			} else if (target->gate != curr.m.cellToThis[dev->gate]) {
				// not a match
				continue;
			}

			if (dev->source >= (int)curr.m.cellToThis.size()) {
				curr.m.cellToThis.resize(dev->source+1, -1);
			}
			if (curr.m.cellToThis[dev->source] < 0) {
				// this net hasn't been assigned yet
				curr.m.cellToThis[dev->source] = target->source;
			} else if (target->source != curr.m.cellToThis[dev->source]) {
				// not a match
				continue;
			}

			if (dev->drain >= (int)curr.m.cellToThis.size()) {
				curr.m.cellToThis.resize(dev->drain+1, -1);
			}
			if (curr.m.cellToThis[dev->drain] < 0) {
				// this net hasn't been assigned yet
				curr.m.cellToThis[dev->drain] = target->drain;
			} else if (target->drain != curr.m.cellToThis[dev->drain]) {
				// not a match
				continue;
			}

			if (dev->base >= (int)curr.m.cellToThis.size()) {
				curr.m.cellToThis.resize(dev->base+1, -1);
			}
			if (curr.m.cellToThis[dev->base] < 0) {
				// this net hasn't been assigned yet
				curr.m.cellToThis[dev->base] = target->base;
			} else if (target->base != curr.m.cellToThis[dev->base]) {
				// not a match
				continue;
			}

			curr.m.devices.push_back(dev-mos.begin());
			if (not curr.todo.empty()) {
				frames.push_back(curr);
				continue;
			}

			sort(curr.m.devices.begin(), curr.m.devices.end());

			// we found a full match for this cell. Are any of the cell's internal
			// nets being used outside the cell?
			bool violation = false;
			for (int i = 0; i < (int)curr.m.cellToThis.size() and not violation; i++) {
				if (curr.m.cellToThis[i] < 0 or cell.nets[i].isIO) {
					// We found an unused net, or this net is externally accessible
					continue;
				}

				auto net = nets.begin() + curr.m.cellToThis[i];

				for (int type = 0; type < 2 and not violation; type++) {
					for (auto j = net->gateOf[type].begin(); j != net->gateOf[type].end() and not violation; j++) {
						auto pos = lower_bound(curr.m.devices.begin(), curr.m.devices.end(), *j);
						violation = (pos == curr.m.devices.end() or *pos != *j);
					}
					for (auto j = net->sourceOf[type].begin(); j != net->sourceOf[type].end() and not violation; j++) {
						auto pos = lower_bound(curr.m.devices.begin(), curr.m.devices.end(), *j);
						violation = (pos == curr.m.devices.end() or *pos != *j);
					}
					for (auto j = net->drainOf[type].begin(); j != net->drainOf[type].end() and not violation; j++) {
						auto pos = lower_bound(curr.m.devices.begin(), curr.m.devices.end(), *j);
						violation = (pos == curr.m.devices.end() or *pos != *j);
					}
				}
			}
			
			if (not violation) {
				result.push_back(curr.m);
			}
		}
	}

	return result;
}

void Subckt::extract(const Mapping &m) {
	if (m.index >= 0 and m.cell != nullptr) {
		int index = (int)inst.size();
		inst.push_back(m.instance());
		for (auto p = inst.back().ports.begin(); p != inst.back().ports.end(); p++) {
			nets[*p].portOf.push_back(index);
		} 
	}

	for (int i = (int)m.devices.size()-1; i >= 0; i--) {
		popMos(m.devices[i]);
	}
}

void Subckt::cleanDangling(bool remIO) {
	for (int i = (int)nets.size()-1; i >= 0; i--) {
		if (nets[i].dangling(remIO)) {
			popNet(i);
		}
	}
}

Mapping Subckt::segment(int net) {
	// TODO(edward.bingham) Generate a cell based on the following constraints:
	// 1. Follow the graph from drain to source starting from "net"
	// 2. Stop whenever we hit a power rail
	// 3. Stop whenever we hit a "named net" at the source. Anonymous nets
	//    follow the pattern "_0" or "#0" where '0' is any integer value.
	// 4. Stop if this net is not the drain of anything.
	// 5. Stop whenever we hit a net at the source that is not in the same
	//    isochronic region as the gate or a net at the gate that is not in the
	//    same isochronic region as the drain.

	printf("generating gate for %d\n", net);
	Mapping result;

	vector<int> stack(1, net);
	while (not stack.empty()) {
		int curr = stack.back();
		stack.pop_back();

		printf("current net %d\n", curr);

		printf("drainOf = {%d, %d}\n", (int)nets[curr].drainOf[0].size(), (int)nets[curr].drainOf[1].size());
		for (int type = 0; type < 2; type++) {
			for (auto i = nets[curr].drainOf[type].begin(); i != nets[curr].drainOf[type].end(); i++) {
				result.devices.push_back(*i);
				result.pushNet(mos[*i].drain);
				result.pushNet(mos[*i].gate);
				result.pushNet(mos[*i].source);
				result.pushNet(mos[*i].base);
				
				int source = mos[*i].source;
				if (nets[source].isAnonymous()) {
					printf("anon %d\n", source);
					stack.push_back(source);
				} else {
					printf("not anon %d\n", source);
				}
			}
		}
	}

	sort(result.devices.begin(), result.devices.end());
	return result;
}

vector<Subckt> Subckt::generateCells(int start) {
	print();
	vector<Mapping> segments;
	for (int i = 0; i < (int)nets.size(); i++) {
		if (not nets[i].isAnonymous() and (not nets[i].drainOf[0].empty() or not nets[i].drainOf[1].empty())) {
			auto seg = segment(i);
			if (not seg.devices.empty()) {
				segments.push_back(seg);
				segments.back().print();
			} else {
				printf("found empty segment\n");
				seg.print();
			}
		}
	}

	// TODO(edward.bingham) only do this merge if the signals crossing the bounds don't switch. How do I figure that out?
	for (int i = 0; i < (int)segments.size(); i++) {
		for (int j = (int)segments.size()-1; j > i; j--) {
			if (segments[j].overlapsWith(segments[i])) {
				segments[i].merge(segments[j]);
				segments.erase(segments.begin() + j);
			} else if (segments[j].coupledWith(*this, segments[i])) {
				segments[i].merge(segments[j]);
				segments.erase(segments.begin() + j);
			}
		}
	}
	
	// TODO(edward.bingham) Merge cells based on the following constraints:
	// 1. Identify all cross-coupled (the output of each cell is an input
	//    to the other) or overlapping cells.
	// 2. merge all disjoint maximal cliques while the size of the cell is less
	//    than some threshold. Make the threshold configurable.
	// 3. If there are still cells with room within the threshold and they are
	//    given by pass transistor logic at their source, then selectively merge
	//    the drivers into the cell as long as they are within the same isochronic
	//    region.

	vector<Subckt> cells;
	cells.reserve(segments.size());
	for (int i = 0; i < (int)segments.size(); i++) {
		segments[i].index = start + (int)cells.size();
		Subckt ckt = segments[i].generate(*this);
		Mapping lbl = ckt.canonicalLabeling();
		segments[i].apply(lbl);
		Subckt canon = segments[i].generate(*this);
		for (int j = 0; j < (int)cells.size(); j++) {
			if (cells[j].compare(canon) == 0) {
				segments[i].index = start+j;
				segments[i].cell = &cells[j];
				break;
			}
		}
		if (segments[i].index >= start+(int)cells.size()) {
			cells.push_back(canon);
			segments[i].cell = &cells.back();
		}
		// TODO(edward.bingham) update segments[i] to the appropriate canonical mapping
		extract(segments[i]);
		print();
		for (int j = (int)segments.size()-1; j > i; j--) {
			// DESIGN(edward.bingham) if two segments overlap, then we just remove
			// the extra devices from one of the segments. It's only ok to have those
			// devices in a different cell if the signals connecting them don't
			// switch (for example, shared weak ground). Otherwise it's an isochronic
			// fork assumption violation.

			if (not segments[j].extract(segments[i])) {
				printf("internal %s:%d: overlapping cells found\n", __FILE__, __LINE__);
			}
			segments[j].print();
		}
	}

	return cells;
}

int Subckt::smallestNondiscreteCell(const vector<vector<int> > &partition) const {
	int curr = -1;
	for (int i = 0; i < (int)partition.size(); i++) {
		if (partition[i].size() == 2) {
			return i;
		} else if (partition[i].size() > 2 and (curr < 0 or partition[i].size() < partition[curr].size())) {
			curr = i;
		}
	}
	return curr;
}

vector<vector<int> > Subckt::createPartitionKey(int v, const vector<vector<int> > &beta) const {
	const int N = 3;
	vector<vector<int> > result;
	for (auto c = beta.begin(); c != beta.end(); c++) {
		vector<int> score(2*N+1, 0);
		score[2*N + 0] = nets[v].isIO or not nets[v].gateOf[0].empty() or not nets[v].gateOf[1].empty();
		for (int type = 0; type < 2; type++) {
			for (auto i = nets[v].drainOf[type].begin(); i != nets[v].drainOf[type].end(); i++) {
				score[type*N + 0] += (std::find(c->begin(), c->end(), mos[*i].source) != c->end());
			}
			for (auto i = nets[v].sourceOf[type].begin(); i != nets[v].sourceOf[type].end(); i++) {
				score[type*N + 1] += (std::find(c->begin(), c->end(), mos[*i].drain) != c->end());
			}
			for (auto i = nets[v].gateOf[type].begin(); i != nets[v].gateOf[type].end(); i++) {
				score[type*N + 2] += (std::find(c->begin(), c->end(), mos[*i].gate) != c->end());
			}
		}
		result.push_back(score);
	}
	return result;
}

vector<vector<int> > Subckt::partitionByConnectivity(const vector<int> &cell, const vector<vector<int> > &beta) const {
	map<vector<vector<int> >, vector<int> > partitions;
	for (auto v = cell.begin(); v != cell.end(); v++) {
		auto pos = partitions.insert(pair<vector<vector<int> >, vector<int> >(createPartitionKey(*v, beta), vector<int>()));
		pos.first->second.push_back(*v);
	}

	vector<vector<int> > result;
	for (auto c = partitions.begin(); c != partitions.end(); c++) {
		result.push_back(c->second);
	}
	return result;
}

/*vector<vector<int> > Subckt::discretePartition() const {
	vector<vector<int> > theta;
	for (int i = 0; i < (int)mos.size(); i++) {
		theta.push_back(vector<int>(1, i));
	}
	return theta;
}

*/

bool Subckt::partitionIsDiscrete(const vector<vector<int> > &partition) const {
	for (auto p = partition.begin(); p != partition.end(); p++) {
		if (p->size() != 1) {
			return false;
		}
	}
	return true;
}

bool Subckt::computePartitions(vector<vector<int> > &partition, vector<vector<int> > alpha) const {
	bool change = false;	
	if (alpha.empty()) {
		alpha.push_back(vector<int>());
		alpha.back().reserve(nets.size());
		for (int i = 0; i < (int)nets.size(); i++) {
			alpha.back().push_back(i);
		}
	}

	if (partition.empty()) {
		partition.push_back(vector<int>());
		partition.back().reserve(nets.size());
		for (int i = 0; i < (int)nets.size(); i++) {
			partition.back().push_back(i);
		}
		change = true;
	}

	vector<vector<int> > next;
	while (not alpha.empty() and not partitionIsDiscrete(partition)) {
		// choose arbitrary subset of alpha
		// TODO(edward.bingham) figure out how to make this choice so as to speed up convergence.
		vector<vector<int> > beta(1, alpha.back());
		alpha.pop_back();

		for (int i = 0; i < (int)partition.size(); i++) {
			vector<vector<int> > refined = partitionByConnectivity(partition[i], beta);
			next.insert(next.end(), refined.begin(), refined.end());
			if (refined.size() > 1) {
				alpha.insert(alpha.end(), refined.begin()+1, refined.end());
				change = true;
			}
		}
		partition.swap(next);
		next.clear();
	}
	return change;
}


/*
bool Subckt::sameCell(int i, int j, vector<vector<int> > theta) {
	for (auto t = theta.begin(); t != theta.end(); t++) {
		if (find(t->begin(), t->end(), i) != t->end() and find(t->begin(), t->end(), j) != t->end()) {
			return true;
		}
	}
	return false;
}*/

bool Subckt::vertexInCell(const vector<int> &cell, int v) const {
	for (auto v0 = cell.begin(); v0 != cell.end(); v0++) {
		if (*v0 == v) {
			return true;
		}
	}
	return false;
}

int Subckt::cellIndex(const vector<vector<int> > pi, int v) const {
	for (int i = 0; i < (int)pi.size(); i++) {
		if (vertexInCell(pi[i], v)) {
			return i;
		}
	}
	return -1;
}

// Lambda functions are indicator functions that are used to prune the search
// tree. They must be invariant between graph isomorphisms and
// lexicographically comparable.
//
// Compute the following:
//   1. number of connections from drain in c0 to source in c1 through nmos
//   2. number of connections from drain in c0 to source in c1 through pmos
//   3. number of connections from drain in c0 to gate in c1 through nmos
//   4. number of connections from drain in c0 to gate in c1 through pmos
array<int, 4> Subckt::lambda(const vector<int> &c0, const vector<int> &c1) const {
	array<int, 4> result;
	if (c0.size() == 1 and c1.size() == 1 and c0[0] == c1[0]) {
		return result;
	}

	for (auto i = c0.begin(); i != c0.end(); i++) {
		auto n0 = nets.begin()+*i;

		for (int type = 0; type < 2; type++) {
			for (auto k = n0->drainOf[type].begin(); k != n0->drainOf[type].end(); k++) {
				for (auto j = c1.begin(); j != c1.end(); j++) {
					auto n1 = nets.begin()+*j;
					if (n1->connectedTo(mos[*k].source)) {
						result[0*2 + type]++;
					}
					if (n1->connectedTo(mos[*k].gate)) {
						result[1*2 + type]++;
					}
				}
			}
		}
	}
	return result;
}

struct lambda_sort {
    bool operator()(const vector<int> &a, const vector<int> &b) const {
        return a.size() < b.size() or (a.size() == b.size() and not a.empty() and a[0] < b[0]);
    }
};

vector<array<int, 4> > Subckt::lambda(vector<vector<int> > pi) const {
	// consistent sort order
	sort(pi.begin(), pi.end(), lambda_sort());

	vector<array<int, 4> > result;
	result.reserve(pi.size()*pi.size());
	for (auto cell = pi.begin(); cell != pi.end(); cell++) {
		result.push_back(lambda(*cell, *cell));
	}

	for (auto c0 = pi.begin(); c0 != pi.end(); c0++) {
		for (auto c1 = pi.begin(); c1 != pi.end(); c1++) {
			if (c0 != c1) {
				result.push_back(lambda(*c0, *c1));
			}
		}
	}
	return result;
}

int Subckt::comparePartitions(const vector<vector<int> > &pi0, const vector<vector<int> > &pi1) const {
	/*if (pi0.size() > pi1.size()) {
		return 1;
	} else if (pi0.size() < pi1.size()) {
		return -1;
	}*/

	/*if (not partitionIsDiscrete(pi0) or not partitionIsDiscrete(pi1) or pi0.size() != pi1.size()) {
		printf("comparePartitions assumptions violated\n");
	}*/
	// implement G^pi0 <=> G^pi1
	// The naive way would be to apply pi0 to G to create G0 and apply pi1 to G
	// to create G1, then represent G0 and G1 as sorted adjacency lists and
	// compare those lists lexographically. All of this can be done without
	// directly applying the mappings and storing the whole graph.

	// The goal is to iterate through each mapping in lexographic order to
	// generate the relevant edges to compare. This would prevent us from
	// applying the whole mapping if we can determine order sooner.
	for (int i = 0; i < (int)nets.size(); i++) {
		auto n0 = nets.begin()+pi0[i].back();
		auto n1 = nets.begin()+pi1[i].back();
		
		for (int type = 0; type < 2; type++) {
			vector<int> g0, g1;
			for (auto j = n0->sourceOf[type].begin(); j != n0->sourceOf[type].end(); j++) {
				g0.push_back(cellIndex(pi0, mos[*j].drain));
			}
			sort(g0.begin(), g0.end());
			for (auto j = n1->sourceOf[type].begin(); j != n1->sourceOf[type].end(); j++) {
				g1.push_back(cellIndex(pi1, mos[*j].drain));
			}
			sort(g1.begin(), g1.end());

			int m = (int)min(g0.size(), g1.size());
			for (int j = 0; j < m; j++) {
				if (g0[j] < g1[j]) {
					return -1;
				} else if (g0[j] > g1[j]) {
					return 1;
				}
			}

			if (m < (int)g0.size()) {
				return -1;
			} else if (m < (int)g1.size()) {
				return 1;
			}
		}
	}
	return 0;
}

/*vector<int> Subckt::omega(vector<vector<int> > pi) const {
	vector<int> result;
	for (auto cell = pi.begin(); cell != pi.end(); cell++) {
		result.push_back((*cell)[0]);
	}
	return result;
}

vector<vector<int> > Subckt::discreteCellsOf(vector<vector<int> > pi) const {
	vector<vector<int> > result;
	for (int i = 0; i < (int)pi.size(); i++) {
		if (pi[i].size() == 1) {
			result.push_back(pi[i]);
		}
	}
	return result;
}*/

// This function will be derived from Nauty, Bliss, and DviCL
//
// B.D. McKay: Computing automorphisms and canonical labellings of
// graphs. International Conference on Combinatorial Mathematics,
// Canberra (1977), Lecture Notes in Mathematics 686, Springer-
// Verlag, 223-232.
//
// Junttila, Tommi, and Petteri Kaski. "Engineering an efficient canonical
// labeling tool for large and sparse graphs." 2007 Proceedings of the Ninth
// Workshop on Algorithm Engineering and Experiments (ALENEX). Society for
// Industrial and Applied Mathematics, 2007.
//
// Lu, Can, et al. "Graph ISO/auto-morphism: a divide-&-conquer approach."
// Proceedings of the 2021 International Conference on Management of Data.
// 2021.
//
// TODO(edward.bingham) implement optimizations from nauty, bliss, and dvicl
Mapping Subckt::canonicalLabeling() const {	
	struct frame {
		frame() {}
		frame(const Subckt *ckt) {
			ckt->computePartitions(part);
			ci = ckt->smallestNondiscreteCell(part);
			vi = 0;
			
			v = -1;
		}
		~frame() {}

		vector<vector<int> > part;
		int ci;
		int vi;
		
		int v;

		vector<array<int, 4> > l;

		bool inc() {
			v = part[ci][vi];
			return (++vi < (int)part[ci].size());
		}

		bool pop(const Subckt *ckt) {
			part.push_back(vector<int>(1, part[ci][vi]));
			part[ci].erase(part[ci].begin()+vi);
			vi = 0;

			vector<vector<int> > alpha(1, part.back());
			if (ckt->computePartitions(part, alpha) or (int)part[ci].size() == 1) {
				ci = ckt->smallestNondiscreteCell(part);
			}

			l = ckt->lambda(part);
			return ci < 0;
		}

		bool isLessThan(const Subckt *ckt, const frame &f) const {
			int m = (int)min(l.size(), f.l.size());
			for (int i = 0; i < m; i++) {
				for (int j = 0; j < 4; j++) {
					if (l[i][j] < f.l[i][j]) {
						return false;
					} else if (l[i][j] > f.l[i][j]) {
						return true;
					}
				}
			}

			if (not ckt->partitionIsDiscrete(part) or not ckt->partitionIsDiscrete(f.part)) {
				return false;
			}
			return ckt->comparePartitions(part, f.part) == -1;
		}
	};

	// TODO(edward.bingham) map discreteCellsOf() -> omega() and use this to
	// prune automorphisms from the search tree.
	// map<vector<int>, vector<int> > stored;

	vector<frame> best;
	vector<frame> frames(1, frame(this));
	if (partitionIsDiscrete(frames.back().part)) {
		best = frames;
		frames.pop_back();
	}

	int explored = 0;
	while (not frames.empty()) {
		frame next = frames.back();
		if (not frames.back().inc()) {
			frames.pop_back();
		}

		/*for (auto p = next.part.begin(); p != next.part.end(); p++) {
			if (not is_sorted(p->begin(), p->end())) {
				printf("violation of sorting assumption\n");
			}
		}*/

		if (next.pop(this)) {
			// found a discrete partition
			explored++;
			// found terminal node in tree
			int cmp = best.empty() ? 1 : comparePartitions(next.part, best.back().part);
			if (cmp == 1) {
				best = frames;
				best.push_back(next);
			} else if (cmp == 0) {
				// We found an automorphism. We can use this to prune the search space.
				// Find a shared node in the frames list between best and next
				int from = 0;
				while (from < (int)frames.size()
					and from < (int)best.size()
					and frames[from].v == best[from].v) {
					from++;
				}
				frames.resize(from);
			}
		} else if (best.empty() or not next.isLessThan(this, best.back())) {
			frames.push_back(next);
		}
	}

	Mapping result;
	for (int i = 0; i < (int)mos.size(); i++) {
		result.devices.push_back(i);
	}
	for (auto cell = best.back().part.begin(); cell != best.back().part.end(); cell++) {
		result.cellToThis.push_back(cell->back());
	}
	return result;
}

void Subckt::printNet(int i) const {
	printf("%s(%d)%s gateOf=", nets[i].name.c_str(), i, (nets[i].isIO ? " io" : ""));
	for (int type = 0; type < 2; type++) {
		printf("{");
		for (int j = 0; j < (int)nets[i].gateOf[type].size(); j++) {
			if (j != 0) {
				printf(", ");
			}
			printf("%d", nets[i].gateOf[type][j]);
		}
		printf("}");
	}

	printf(" sourceOf=");
	for (int type = 0; type < 2; type++) {
		printf("{");
		for (int j = 0; j < (int)nets[i].sourceOf[type].size(); j++) {
			if (j != 0) {
				printf(", ");
			}
			printf("%d", nets[i].sourceOf[type][j]);
		}
		printf("}");
	}

	printf(" drainOf=");
	for (int type = 0; type < 2; type++) {
		printf("{");
		for (int j = 0; j < (int)nets[i].drainOf[type].size(); j++) {
			if (j != 0) {
				printf(", ");
			}
			printf("%d", nets[i].drainOf[type][j]);
		}
		printf("}");
	}

	printf(" portOf={");
	for (int j = 0; j < (int)nets[i].portOf.size(); j++) {
		if (j != 0) {
			printf(", ");
		}
		printf("%d", nets[i].portOf[j]);
	}
	printf("}\n");
}

void Subckt::printMos(int i) const {
	printf("%s(%d) d=%s(%d) g=%s(%d) s=%s(%d) b=%s(%d)\n", (mos[i].type == 0 ? "nmos" : "pmos"), i, nets[mos[i].drain].name.c_str(), mos[i].drain, nets[mos[i].gate].name.c_str(), mos[i].gate, nets[mos[i].source].name.c_str(), mos[i].source, nets[mos[i].base].name.c_str(), mos[i].base);
}

void Subckt::print() const {
	printf("nets\n");
	for (int i = 0; i < (int)nets.size(); i++) {
		printNet(i);
	}
	printf("\nmos\n");
	for (int i = 0; i < (int)mos.size(); i++) {
		printMos(i);
	}
	printf("\n");

	printf("net partitions\n");
	vector<vector<int> > parts;
	computePartitions(parts);
	for (int i = 0; i < (int)parts.size(); i++) {
		for (int j = 0; j < (int)parts[i].size(); j++) {
			printNet(parts[i][j]);
		}
		printf("\n");
	}
	printf("\n");

	printf("canonical labels\n");
	auto lbl = canonicalLabeling();
	lbl.print();
}

int Subckt::compare(const Subckt &ckt) const {
	if (nets.size() > ckt.nets.size()) {
		return 1;
	} else if (nets.size() < ckt.nets.size()) {
		return -1;
	}

	for (int i = 0; i < (int)nets.size(); i++) {
		auto n0 = nets.begin()+i;
		auto n1 = ckt.nets.begin()+i;
		
		for (int type = 0; type < 2; type++) {
			vector<int> g0, g1;
			for (auto j = n0->sourceOf[type].begin(); j != n0->sourceOf[type].end(); j++) {
				g0.push_back(mos[*j].drain);
			}
			sort(g0.begin(), g0.end());
			for (auto j = n1->sourceOf[type].begin(); j != n1->sourceOf[type].end(); j++) {
				g1.push_back(ckt.mos[*j].drain);
			}
			sort(g1.begin(), g1.end());

			int m = (int)min(g0.size(), g1.size());
			for (int j = 0; j < m; j++) {
				if (g0[j] < g1[j]) {
					return -1;
				} else if (g0[j] > g1[j]) {
					return 1;
				}
			}

			if (m < (int)g0.size()) {
				return -1;
			} else if (m < (int)g1.size()) {
				return 1;
			}
		}
	}
	return 0;
}

}
