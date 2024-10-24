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
	this->size = vec2i(0,0);
	this->area = vec2i(0,0);
	this->perim = vec2i(0,0);
	this->drain = -1;
	this->gate = -1;
	this->source = -1;
	this->base = -1;
}

Mos::Mos(int model, int type, int drain, int gate, int source, int base) {
	this->model = model;
	this->type = type;
	this->drain = drain;
	this->gate = gate;
	this->source = source;
	this->base = base;
	this->size = vec2i(0,0);
	this->area = vec2i(0,0);
	this->perim = vec2i(0,0);
}


Mos::Mos(const Tech &tech, int model, int type, int drain, int gate, int source, int base, vec2i size) {
	this->model = model;
	this->type = type;
	this->drain = drain;
	this->gate = gate;
	this->source = source;
	this->base = base;
	setSize(tech, size);
}

Mos::~Mos() {
}

void Mos::setSize(const Tech &tech, vec2i size) {
	this->size = size;

	int via = -1;
	for (int i = 0; i < (int)tech.vias.size() and via < 0; i++) {
		if (tech.vias[i].downLevel == -model-1 and tech.vias[i].upLevel == 1) {
			via = i;
		}
	}

	int gateToVia = 0;
	int viaSize = 0;
	if (via >= 0) {
		gateToVia = tech.getSpacing(tech.wires[0].draw, tech.vias[via].draw);
		viaSize = tech.paint[tech.vias[via].draw].minWidth;
	} else {
		gateToVia = tech.getSpacing(tech.wires[0].draw, tech.wires[0].draw)/2;
	}

	int w = size[1];
	int l = gateToVia + viaSize + gateToVia;

	int a = w*l/2;
	int p = w+l;

	this->area = vec2i(a, a);
	this->perim = vec2i(p, p);
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

Instance::Instance() {
	subckt = -1;
}

Instance::Instance(const Subckt &ckt, const Mapping &m, int subckt) {
	this->subckt = subckt;
	for (int i = 0; i < (int)ckt.ports.size(); i++) {
		this->ports.push_back(m.nets[ckt.ports[i]]);
	}
}

Instance::~Instance() {
}

Subckt::Subckt(bool isCell) {
	this->isCell = isCell;
	id = (size_t)-1;
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

		if (d->base >= 0 and d->base > index) {
			d->base--;
		} else if (d->base >= 0 and d->base == index) {
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

int Subckt::pushMos(int model, int type, int drain, int gate, int source, int base) {
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

	mos.push_back(Mos(model, type, drain, gate, source, base));
	return result;
}

int Subckt::pushMos(const Tech &tech, int model, int type, int drain, int gate, int source, int base, vec2i size) {
	int result = pushMos(model, type, drain, gate, source, base);
	mos[result].setSize(tech, size);
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

void Subckt::pushInst(Instance ckt) {
	int index = (int)inst.size();
	inst.push_back(ckt);
	for (auto p = inst.back().ports.begin(); p != inst.back().ports.end(); p++) {
		nets[*p].portOf.push_back(index);
	}
}

void Subckt::extract(const Segment &seg) {
	for (int i = (int)seg.mos.size()-1; i >= 0; i--) {
		popMos(seg.mos[i]);
	}
}

void Subckt::cleanDangling(bool remIO) {
	for (int i = (int)nets.size()-1; i >= 0; i--) {
		if (nets[i].dangling(remIO)) {
			popNet(i);
		}
	}
}

Segment Subckt::segment(int net, set<int> *covered) {
	// TODO(edward.bingham) Generate a cell based on the following constraints:
	// 1. Follow the graph from drain to source starting from "net"
	// 2. Stop whenever we hit a power rail
	// 3. Stop whenever we hit a "named net" at the source. Anonymous nets
	//    follow the pattern "_0" or "#0" where '0' is any integer value.
	// 4. Stop if this net is not the drain of anything.
	// 5. Stop whenever we hit a net at the source that is not in the same
	//    isochronic region as the gate or a net at the gate that is not in the
	//    same isochronic region as the drain.

	//printf("generating gate for %d\n", net);
	Segment result;

	vector<int> stack(1, net);
	while (not stack.empty()) {
		int curr = stack.back();
		stack.pop_back();

		if (covered != nullptr) {
			covered->insert(curr);
		}

		//printf("current net %d\n", curr);

		//printf("drainOf = {%d, %d}\n", (int)nets[curr].drainOf[0].size(), (int)nets[curr].drainOf[1].size());
		for (int type = 0; type < 2; type++) {
			for (auto i = nets[curr].drainOf[type].begin(); i != nets[curr].drainOf[type].end(); i++) {
				result.mos.push_back(*i);
				
				int source = mos[*i].source;
				if (nets[source].isAnonymous()) {
					//printf("anon %d\n", source);
					stack.push_back(source);
				} else {
					//printf("not anon %d\n", source);
				}
			}
		}
	}

	sort(result.mos.begin(), result.mos.end());
	result.mos.erase(unique(result.mos.begin(), result.mos.end()), result.mos.end());
	return result;
}

vector<Segment> Subckt::segment() {
	//print();
	vector<Segment> segments;
	set<int> covered;
	for (int i = 0; i < (int)nets.size(); i++) {
		if (not nets[i].isAnonymous() and (not nets[i].drainOf[0].empty() or not nets[i].drainOf[1].empty())) {
			auto seg = segment(i, &covered);
			if (not seg.mos.empty()) {
				segments.push_back(seg);
				//segments.back().print();
			} else {
				printf("found empty segment\n");
				seg.print();
			}
		}
	}

	for (int i = 0; i < (int)nets.size(); i++) {
		if (covered.find(i) == covered.end()
			and (not nets[i].drainOf[0].empty()
				or not nets[i].drainOf[1].empty())
			and nets[i].sourceOf[0].empty()
			and nets[i].sourceOf[1].empty()) {
			auto seg = segment(i, &covered);
			if (not seg.mos.empty()) {
				segments.push_back(seg);
				//segments.back().print();
			} else {
				printf("found empty segment\n");
				seg.print();
			}
		}
	}

	for (int i = 0; i < (int)nets.size(); i++) {
		if (covered.find(i) == covered.end()
			and (not nets[i].drainOf[0].empty()
				or not nets[i].drainOf[1].empty())) {
			auto seg = segment(i, &covered);
			if (not seg.mos.empty()) {
				segments.push_back(seg);
				//segments.back().print();
			} else {
				printf("found empty segment\n");
				seg.print();
			}
		}
	}

	// Merge cells based on the following constraints:
	// 1. Identify all cross-coupled (the output of each cell is an input
	//    to the other) or overlapping cells.
	// 2. merge all disjoint maximal cliques while the size of the cell is less
	//    than some threshold. Make the threshold configurable.
	// 3. If there are still cells with room within the threshold and they are
	//    given by pass transistor logic at their source, then selectively merge
	//    the drivers into the cell as long as they are within the same isochronic
	//    region.

	// TODO(edward.bingham) only do this merge if the signals crossing the bounds don't switch. How do I figure that out?
	for (int i = (int)segments.size()-2; i >= 0; i--) {
		for (int j = (int)segments.size()-1; j > i; j--) {
			if (segments[j].overlapsWith(segments[i])
				or areCoupled(segments[i], segments[j])) {
				segments[i].merge(segments[j]);
				segments.erase(segments.begin() + j);
			}
		}
	}

	return segments;
}

bool Subckt::areCoupled(const Segment &s0, const Segment &s1) const {
	std::set<int> fromA, toA, fromB, toB;
	for (auto d = s0.mos.begin(); d != s0.mos.end(); d++) {
		fromA.insert(mos[*d].drain);
		toA.insert(mos[*d].gate);
		toA.insert(mos[*d].source);
		toA.insert(mos[*d].base);
	}
	for (auto d = s1.mos.begin(); d != s1.mos.end(); d++) {
		fromB.insert(mos[*d].drain);
		toB.insert(mos[*d].gate);
		toB.insert(mos[*d].source);
		toB.insert(mos[*d].base);
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

void Subckt::apply(const Mapping &m) {
	for (int i = 0; i < (int)ports.size(); i++) {
		ports[i] = m.nets[ports[i]];
	}

	for (int i = 0; i < (int)mos.size(); i++) {
		mos[i].gate = m.nets[mos[i].gate];
		mos[i].source = m.nets[mos[i].source];
		mos[i].drain = m.nets[mos[i].drain];
		mos[i].base = m.nets[mos[i].base];
	}

	for (int i = 0; i < (int)nets.size(); i++) {
		for (int j = 0; j < (int)nets[i].remote.size(); j++) {
			nets[i].remote[j] = m.nets[nets[i].remote[j]];
		}
	}

	for (int i = 0; i < (int)inst.size(); i++) {
		for (int j = 0; j < (int)inst[i].ports.size(); j++) {
			inst[i].ports[j] = m.nets[inst[i].ports[j]];
		}
	}

	vector<Net> reorder;
	reorder.reserve(m.nets.size());
	for (int i = 0; i < (int)m.nets.size(); i++) {
		reorder.push_back(nets[m.nets[i]]);
	}
	std::swap(nets, reorder);
	reorder.clear();
}

Mapping Subckt::canonicalize() {
	Mapping lbl = canonicalLabels(*this);
	apply(lbl);
	id = std::hash<Subckt>{}(*this);
	return lbl;
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

vector<vector<int> > Subckt::createPartitionKey(int net, const Partition &beta) const {
	const int N = 3;
	vector<vector<int> > result;
	for (auto c = beta.cells.begin(); c != beta.cells.end(); c++) {
		vector<int> score(2*N+1, 0);
		score[2*N + 0] = nets[net].isIO or not nets[net].gateOf[0].empty() or not nets[net].gateOf[1].empty();
		for (int type = 0; type < 2; type++) {
			for (auto i = nets[net].drainOf[type].begin(); i != nets[net].drainOf[type].end(); i++) {
				score[type*N + 0] += (std::find(c->begin(), c->end(), mos[*i].source) != c->end());
			}
			for (auto i = nets[net].sourceOf[type].begin(); i != nets[net].sourceOf[type].end(); i++) {
				score[type*N + 1] += (std::find(c->begin(), c->end(), mos[*i].drain) != c->end());
			}
			for (auto i = nets[net].gateOf[type].begin(); i != nets[net].gateOf[type].end(); i++) {
				score[type*N + 2] += (std::find(c->begin(), c->end(), mos[*i].gate) != c->end());
			}
		}
		result.push_back(score);
	}
	return result;
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
array<int, 4> Subckt::lambda(const Partition::Cell &c0, const Partition::Cell &c1) const {
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

vector<array<int, 4> > Subckt::lambda(Partition pi) const {
	// consistent sort order
	sort(pi.cells.begin(), pi.cells.end(), lambda_sort());

	vector<array<int, 4> > result;
	result.reserve(pi.cells.size()*pi.cells.size());
	for (auto cell = pi.cells.begin(); cell != pi.cells.end(); cell++) {
		result.push_back(lambda(*cell, *cell));
	}

	for (auto c0 = pi.cells.begin(); c0 != pi.cells.end(); c0++) {
		for (auto c1 = pi.cells.begin(); c1 != pi.cells.end(); c1++) {
			if (c0 != c1) {
				result.push_back(lambda(*c0, *c1));
			}
		}
	}
	return result;
}

int Subckt::comparePartitions(const Partition &pi0, const Partition &pi1) const {
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
		auto n0 = nets.begin()+pi0.cells[i].back();
		auto n1 = nets.begin()+pi1.cells[i].back();
		
		for (int type = 0; type < 2; type++) {
			vector<int> g0, g1;
			for (auto j = n0->sourceOf[type].begin(); j != n0->sourceOf[type].end(); j++) {
				g0.push_back(pi0.cellOf(mos[*j].drain));
			}
			sort(g0.begin(), g0.end());
			for (auto j = n1->sourceOf[type].begin(); j != n1->sourceOf[type].end(); j++) {
				g1.push_back(pi1.cellOf(mos[*j].drain));
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

int Subckt::verts() const {
	return (int)nets.size();
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
}

bool operator==(const Subckt &c0, const Subckt &c1) {
	return c0.compare(c1) == 0;
}

bool operator!=(const Subckt &c0, const Subckt &c1) {
	return c0.compare(c1) != 0;
}

bool operator<(const Subckt &c0, const Subckt &c1) {
	return c0.compare(c1) == -1;
}

bool operator>(const Subckt &c0, const Subckt &c1) {
	return c0.compare(c1) == 1;
}

bool operator<=(const Subckt &c0, const Subckt &c1) {
	return c0.compare(c1) != 1;
}

bool operator>=(const Subckt &c0, const Subckt &c1) {
	return c0.compare(c1) != -1;
}

}
