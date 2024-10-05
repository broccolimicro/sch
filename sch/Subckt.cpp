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

bool Net::connectedTo(int net) {
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

bool Mapping::apply(const Mapping &m) {
	printf("applying mapping to this\n");
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
	printf("adding to net d=%d g=%d s=%d nets.size=%d device=%d\n", drain, gate, source, (int)nets.size(), result);
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

void Subckt::apply(const Mapping &m) {
	int index = (int)inst.size();
	inst.push_back(m.instance());
	for (auto p = inst.back().ports.begin(); p != inst.back().ports.end(); p++) {
		nets[*p].portOf.push_back(index);
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
		cells.push_back(segments[i].generate(*this));
		segments[i].cell = &cells.back();
		apply(segments[i]);
		print();
		for (int j = (int)segments.size()-1; j > i; j--) {
			// DESIGN(edward.bingham) if two segments overlap, then we just remove
			// the extra devices from one of the segments. It's only ok to have those
			// devices in a different cell if the signals connecting them don't
			// switch (for example, shared weak ground). Otherwise it's an isochronic
			// fork assumption violation.

			if (not segments[j].apply(segments[i])) {
				printf("internal %s:%d: overlapping cells found\n", __FILE__, __LINE__);
			}
			segments[j].print();
		}
	}

	return cells;
}

bool operator<(const Subckt::partitionKey &k0, const Subckt::partitionKey &k1) {
	if (k0.size() < k1.size()) {
		return true;
	} else if (k0.size() > k1.size()) {
		return false;
	}

	for (int i = 0; i < (int)k0.size(); i++) {
		for (int j = 0; j < (int)k0[i].size(); j++) {
			if (k0[i][j] < k1[i][j]) {
				return true;
			} else if (k0[i][j] > k1[i][j]) {
				return false;
			}
		}
	}
	return false;
}

bool operator==(const Subckt::partitionKey &k0, const Subckt::partitionKey &k1) {
	if (k0.size() != k1.size()) {
		return false;
	}

	for (int i = 0; i < (int)k0.size(); i++) {
		for (int j = 0; j < (int)k0[i].size(); j++) {
			if (k0[i][j] != k1[i][j]) {
				return false;
			}
		}
	}
	return true;
}

/*int Subckt::nextVertexInCell(const vector<int> &cell, int v) const {
	int result = std::numeric_limits<int>::max();
	for (int i = 0; i < (int)cell.size(); i++) {
		if (*v0 < result and *v0 > v) {
			result = *v0;
		}
	}
	return result;
}*/

int Subckt::smallestNondiscreteCell(const vector<vector<int> > &partition) const {
	auto curr = partition.end();
	for (auto i = partition.begin(); i != partition.end(); i++) {
		if (i->size() == 2) {
			return i-partition.begin();
		} else if (i->size() > 1 and (curr == partition.end() or i->size() < curr->size())) {
			curr = i;
		}
	}
	return curr-partition.begin();
}

Subckt::partitionKey Subckt::createPartitionKey(int kind, int v, const vector<vector<int> > &beta) const {
	Subckt::partitionKey result;
	for (auto c = beta.begin(); c != beta.end(); c++) {
		Subckt::partitionKeyElem score;

		if (kind == LOGICAL or kind == DEVICE) {
			vector<int> ports({mos[v].drain, mos[v].gate, mos[v].source});
			int categories = 3;
			score.resize(ports.size()*categories+3, 0);

			if (kind == LOGICAL) {
				score[categories*(int)ports.size() + 0] = mos[v].type;
			} else if (kind == DEVICE) {
				score[categories*(int)ports.size() + 0] = mos[v].model;
				score[categories*(int)ports.size() + 1] = mos[v].size[0];
				score[categories*(int)ports.size() + 2] = mos[v].size[1];
			}
			for (int type = 0; type < 2; type++) {
				for (int p = 0; p < (int)ports.size(); p++) {
					for (auto i = nets[ports[p]].drainOf[type].begin(); i != nets[ports[p]].drainOf[type].end(); i++) {
						score[0*(int)ports.size() + p] += (std::find(c->begin(), c->end(), *i) != c->end());
					}
					for (auto i = nets[ports[p]].sourceOf[type].begin(); i != nets[ports[p]].sourceOf[type].end(); i++) {
						score[1*(int)ports.size() + p] += (std::find(c->begin(), c->end(), *i) != c->end());
					}

					score[2*(int)ports.size() + p] = score[2*(int)ports.size() + p] or not nets[ports[p]].gateOf[type].empty() or nets[ports[p]].isIO;
				}
			}
		} else if (kind == NET) {
			int categories = 2;
			score.resize(categories*2+1, 0);
			score[2*categories + 0] = nets[v].isIO or not nets[v].gateOf[0].empty() or not nets[v].gateOf[1].empty();
			for (int type = 0; type < 2; type++) {
				for (auto i = nets[v].drainOf[type].begin(); i != nets[v].drainOf[type].end(); i++) {
					score[type*categories + 0] += (std::find(c->begin(), c->end(), mos[*i].source) != c->end());
				}
				for (auto i = nets[v].sourceOf[type].begin(); i != nets[v].sourceOf[type].end(); i++) {
					score[type*categories + 1] += (std::find(c->begin(), c->end(), mos[*i].drain) != c->end());
				}
			}
		}
		result.push_back(score);
	}
	return result;
}

vector<vector<int> > Subckt::partitionByConnectivity(int kind, const vector<int> &cell, const vector<vector<int> > &beta) const {
	vector<vector<int> > result;
	map<partitionKey, int> partitions;
	for (auto v = cell.begin(); v != cell.end(); v++) {
		auto pos = partitions.insert(pair<partitionKey, int>(createPartitionKey(kind, *v, beta), (int)result.size()));
		if (pos.second) {
			result.push_back(vector<int>());
		}
		result[pos.first->second].push_back(*v);
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

vector<vector<int> > Subckt::discreteCellsOf(const vector<vector<int> > &pi) const {
	vector<vector<int> > result;
	for (auto cell = pi.begin(); cell != pi.end(); cell++) {
		if (cell->size() == 1) {
			result.push_back(*cell);
		}
	}
	return result;
}*/

bool Subckt::partitionIsDiscrete(const vector<vector<int> > &partition) const {
	for (auto p = partition.begin(); p != partition.end(); p++) {
		if (p->size() != 1) {
			return false;
		}
	}
	return true;
}

vector<vector<int> > Subckt::computePartitions(int kind, vector<vector<int> > partition, vector<vector<int> > alpha) const {
	int sz = (kind == NET ? nets.size() : mos.size());
	if (alpha.empty()) {
		alpha.push_back(vector<int>());
		for (int i = 0; i < sz; i++) {
			alpha.back().push_back(i);
		}
	}

	if (partition.empty()) {
		partition.push_back(vector<int>());
		for (int i = 0; i < sz; i++) {
			partition.back().push_back(i);
		}
	}

	vector<vector<int> > next;
	while (not alpha.empty() and not partitionIsDiscrete(partition)) {
		// choose arbitrary subset of alpha
		// TODO(edward.bingham) figure out how to make this choice so as to speed up convergence.
		vector<vector<int> > beta(1, alpha.back());
		alpha.pop_back();

		for (int i = 0; i < (int)partition.size(); i++) {
			vector<vector<int> > refined = partitionByConnectivity(kind, partition[i], beta);
			next.insert(next.end(), refined.begin(), refined.end());
			if (refined.size() > 1) {
				alpha.insert(alpha.end(), refined.begin()+1, refined.end());
			}
		}
		partition.swap(next);
		next.clear();
	}
	return partition;
}


/*int Subckt::lambda(int kind, vector<vector<int> > pi) {
	int result = 0;
	for (auto cell = pi.begin(); cell != pi.end(); cell++) {
		for (auto i = cell.begin(); i != cell.end(); i++) {
			for (auto j = i+1; j != cell.end(); j++) {
				if (kind == DEVICE) {
					int n0 = mos[*i].source;
					int n1 = mos[*i].drain;
					int n2 = mos[*j].source;
					int n3 = mos[*j].drain;
					result += (
						nets[n0].connectedTo(n2) +
						nets[n0].connectedTo(n3) +
						nets[n1].connectedTo(n2) +
						nets[n1].connectedTo(n3) +
					);
				}
			}
		}
	}
	return result;
}

vector<int> Subckt::omega(vector<vector<int> > pi) {
	vector<int> result;
	for (auto cell = pi.begin(); cell != pi.end(); cell++) {
		result.push_back(nextVertexInCell(*cell);
	}
	return result;
}


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

int Subckt::comparePartitions(const vector<vector<int> > &pi0, const vector<vector<int> > &pi1) const {
	if (pi0.size() > pi1.size()) {
		return 1;
	} else if (pi0.size() < pi1.size()) {
		return -1;
	}

	/*if (not partitionIsDiscrete(pi0) or not partitionIsDiscrete(pi1)) {
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
			part = ckt->computePartitions(NET);
			ci = ckt->smallestNondiscreteCell(part);
		}
		~frame() {}

		vector<vector<int> > part;
		int ci;
	};

	int autoCount = 0;
	vector<vector<int> > best;
	vector<frame> frames(1, frame(this));
	if (partitionIsDiscrete(frames.back().part)) {
		best.swap(frames.back().part);
		frames.pop_back();
	}

	uint64_t count = 1;
	for (auto c = frames.back().part.begin(); c != frames.back().part.end(); c++) {
		for (uint64_t i = c->size(); i >= 1; i--) {
			count *= i;
		}
	}
	printf("exploring %lu labellings\n", count);

	int explored = 0;
	while (not frames.empty()) {
		if ((explored&((1<<10)-1)) == 0) {
			printf("%d\t%d\t%d\r", (int)frames.size(), explored, autoCount);
			fflush(stdout);
		}

		auto curr = frames.back();
		frames.pop_back();
		/*for (auto p = curr.part.begin(); p != curr.part.end(); p++) {
			if (not is_sorted(p->begin(), p->end())) {
				printf("violation of sorting assumption\n");
			}
		}*/

		for (int v = 0; v < (int)curr.part[curr.ci].size(); v++) {
			auto next = curr;
			next.part.push_back(vector<int>(1, next.part[next.ci][v]));
			next.part[next.ci].erase(next.part[next.ci].begin()+v);
			next.part = computePartitions(NET, next.part, vector<vector<int> >(1, next.part.back()));

			if (partitionIsDiscrete(next.part)) {
				explored++;
				// found terminal node in tree
				int cmp = comparePartitions(next.part, best);
				if (cmp == 1) {
					best.swap(next.part);
				} else if (cmp == 0) {
					autoCount++;
				}
			} else {
				if (next.part[next.ci].size() == 1) {
					next.ci = smallestNondiscreteCell(next.part);
				}
				
				frames.push_back(next);
			}
		}
	}

	printf("%d automorphisms found\n", autoCount);

	Mapping result;
	for (int i = 0; i < (int)mos.size(); i++) {
		result.devices.push_back(i);
	}
	for (auto cell = best.begin(); cell != best.end(); cell++) {
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

	/*printf("device partitions\n");
	auto parts = computePartitions(Subckt::LOGICAL);
	for (int i = 0; i < (int)parts.size(); i++) {
		for (int j = 0; j < (int)parts[i].size(); j++) {
			printMos(parts[i][j]);
		}
		printf("\n");
	}
	printf("\n");*/

	printf("net partitions\n");
	auto parts = computePartitions(Subckt::NET);
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

}
