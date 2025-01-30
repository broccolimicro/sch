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
	this->size = vec2i(0,0);
	this->area = vec2i(0,0);
	this->perim = vec2i(0,0);
	setSize(tech, size);
}

Mos::~Mos() {
}

void Mos::setSize(const Tech &tech, vec2i size) {
	this->size = size;

	auto m = tech.models.begin()+model;

	vector<int> vias = tech.via(m->diff, Level(Level::ROUTE, 1));

	int gateToVia = 0;
	int viaSize = 0;
	if (not vias.empty()) {
		gateToVia = tech.getSpacing(tech.wires[0].draw, tech.vias[vias[0]].draw);
		viaSize = tech.getWidth(tech.vias[vias[0]].draw);
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

bool Mos::combineParallel(const Mos &m) {
	if (m.model != model
		or m.gate != gate
		or m.base != base
		or m.params != params) {
		return false;
	}

	if (m.size[0] == size[0]
		and ((m.source == source and m.drain == drain)
			or (m.source == drain and m.drain == source))) {
		size[1] += m.size[1];
		area += m.area;
		perim += m.perim;
		return true;
	}

	return false;
}

Mos Mos::flip() const {
	Mos result(*this);
	swap(result.source, result.drain);
	return result;
}

bool operator==(const Mos &m0, const Mos &m1) {
	return m0.model == m1.model
	and m0.base == m1.base
	and m0.source == m1.source
	and m0.drain == m1.drain
	and m0.gate == m1.gate
	and m0.size[0] == m1.size[0]
	and m0.size[1] == m1.size[1];
}

bool operator!=(const Mos &m0, const Mos &m1) {
	return m0.model != m1.model
	or m0.base != m1.base
	or m0.source != m1.source
	or m0.drain != m1.drain
	or m0.gate != m1.gate
	or m0.size[0] != m1.size[0]
	or m0.size[1] != m1.size[1];
}

bool operator<(const Mos &m0, const Mos &m1) {
	return m0.model < m1.model
			or (m0.model == m1.model
				and (m0.base < m1.base
					or (m0.base == m1.base
						and (m0.source < m1.source
							or (m0.source == m1.source
								and (m0.drain < m1.drain
									or (m0.drain == m1.drain
										and (m0.gate < m1.gate
											or (m0.gate == m1.gate
												and (m0.size[0] < m1.size[0]
													or (m0.size[0] == m1.size[0]
														and (m0.size[1] < m1.size[1]))))))))))));
}

bool operator>(const Mos &m0, const Mos &m1) {
	return m1.model < m0.model
			or (m1.model == m0.model
				and (m1.base < m0.base
					or (m1.base == m0.base
						and (m1.source < m0.source
							or (m1.source == m0.source
								and (m1.drain < m0.drain
									or (m1.drain == m0.drain
										and (m1.gate < m0.gate
											or (m1.gate == m0.gate
												and (m1.size[0] < m0.size[0]
													or (m1.size[0] == m0.size[0]
														and (m1.size[1] < m0.size[1]))))))))))));
}

Net::Net() {
	isIO = false;
	remoteIO = false;
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
		and baseOf[0].empty()
		and baseOf[1].empty()
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

Instance::Instance(int subckt, vector<int> ports) {
	this->subckt = subckt;
	this->ports = ports;
}

Instance::Instance(const Subckt &ckt, const ucs::mapping &m, int subckt) {
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
		return push(Net(name));
	}
	return -1;
}

string Subckt::netName(int net) const {
	if (net < 0) {
		return "_";
	}

	return nets[net].name;
}

int Subckt::push(Net n) {
	int result = (int)nets.size();
	nets.push_back(n);
	nets.back().remote.push_back(result);
	if (n.isIO) {
		ports.push_back(result);
	}
	return result;
}

int Subckt::push(Mos m) {
	int result = (int)mos.size();
	if (m.drain >= 0) {
		for (auto i = nets[m.drain].remote.begin(); i != nets[m.drain].remote.end(); i++) {
			nets[*i].drainOf[m.type].push_back(result);
		}
	}
	if (m.source >= 0) {
		for (auto i = nets[m.source].remote.begin(); i != nets[m.source].remote.end(); i++) {
			nets[*i].sourceOf[m.type].push_back(result);
		}
	}
	if (m.gate >= 0) {
		for (auto i = nets[m.gate].remote.begin(); i != nets[m.gate].remote.end(); i++) {
			nets[*i].gateOf[m.type].push_back(result);
		}
	}
	if (m.base >= 0) {
		for (auto i = nets[m.base].remote.begin(); i != nets[m.base].remote.end(); i++) {
			nets[*i].baseOf[m.type].push_back(result);
		}
	}

	mos.push_back(m);
	return result;
}

void Subckt::push(Instance ckt) {
	int index = (int)inst.size();
	inst.push_back(ckt);
	for (auto p = inst.back().ports.begin(); p != inst.back().ports.end(); p++) {
		for (auto n = nets[*p].remote.begin(); n != nets[*p].remote.end(); n++) {
			nets[*n].portOf.push_back(index);
		}
	}
}

void Subckt::popNet(int index) {
	nets.erase(nets.begin()+index);

	for (auto n = nets.begin(); n != nets.end(); n++) {
		for (int i = (int)n->remote.size()-1; i >= 0; i--) {
			if (n->remote[i] > index) {
				n->remote[i]--;
			} else if (n->remote[i] == index) {
				n->remote.erase(n->remote.begin()+i);
			}
		}
	}

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

	for (auto d = inst.begin(); d != inst.end(); d++) {
		for (int i = 0; i < (int)d->ports.size(); i++) {
			if (d->ports[i] > index) {
				d->ports[i]--;
			} else if (d->ports[i] == index) {
				d->ports[i] = -1;
			}
		}
	}
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
			for (int j = (int)n->baseOf[type].size()-1; j >= 0; j--) {
				if (n->baseOf[type][j] > index) {
					n->baseOf[type][j]--;
				} else if (n->baseOf[type][j] == index) {
					n->baseOf[type].erase(n->baseOf[type].begin()+j);
				}
			}
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

		nets[n0].baseOf[type].insert(nets[n0].baseOf[type].end(), nets[n1].baseOf[type].begin(), nets[n1].baseOf[type].end());
		sort(nets[n0].baseOf[type].begin(), nets[n0].baseOf[type].end());
		nets[n0].baseOf[type].erase(unique(nets[n0].baseOf[type].begin(), nets[n0].baseOf[type].end()), nets[n0].baseOf[type].end());
		nets[n1].baseOf[type] = nets[n0].baseOf[type];
	}
	nets[n0].portOf.insert(nets[n0].portOf.end(), nets[n1].portOf.begin(), nets[n1].portOf.end());
	sort(nets[n0].portOf.begin(), nets[n0].portOf.end());
	nets[n0].portOf.erase(unique(nets[n0].portOf.begin(), nets[n0].portOf.end()), nets[n0].portOf.end());
	nets[n1].portOf = nets[n0].portOf;

	nets[n0].remoteIO = nets[n0].remoteIO or nets[n1].remoteIO;
	nets[n1].remoteIO = nets[n0].remoteIO;
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
	// 4. If all of their connections go to the same place or to eachother.

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

	int count = 8;
	for (int i = (int)segments.size()-2; i >= 0; i--) {
		for (int j = (int)segments.size()-1; j > i; j--) {
			if ((int)(segments[i].mos.size() + segments[i].mos.size()) <= count and areSimilar(segments, i, j)) {
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

bool Subckt::areSimilar(const vector<Segment> &segs, int a, int b) const {
	std::set<int> aNets, bNets;
	for (auto d = segs[a].mos.begin(); d != segs[a].mos.end(); d++) {
		aNets.insert(mos[*d].drain);
		aNets.insert(mos[*d].gate);
		aNets.insert(mos[*d].source);
		aNets.insert(mos[*d].base);
	}
	for (auto d = segs[b].mos.begin(); d != segs[b].mos.end(); d++) {
		bNets.insert(mos[*d].drain);
		bNets.insert(mos[*d].gate);
		bNets.insert(mos[*d].source);
		bNets.insert(mos[*d].base);
	}

	std::set<int> A, B;
	for (auto n = aNets.begin(); n != aNets.end(); n++) {
		for (int s = 0; s < (int)segs.size(); s++) {
			if (segs[s].contains(*n)) {
				A.insert(s);
			}
		}
		if (nets[*n].isIO) {
			A.insert(-1);
		}
	}
	for (auto n = bNets.begin(); n != bNets.end(); n++) {
		for (int s = 0; s < (int)segs.size(); s++) {
			if (segs[s].contains(*n)) {
				B.insert(s);
			}
		}
		if (nets[*n].isIO) {
			B.insert(-1);
		}
	}

	A.erase(a);
	A.erase(b);
	B.erase(a);
	B.erase(b);

	return A == B;
}

void Subckt::combineDevices() {
	for (int i = (int)mos.size()-2; i >= 0; i--) {
		for (int j = (int)mos.size()-1; j > i; j--) {
			if (mos[i].combineParallel(mos[j])) {
				popMos(j);
			}
		}
	}
}

void Subckt::splitDevices(const Tech &tech) {
	// check devices against sizing constraints and bins
	for (int i = (int)mos.size()-1; i >= 0; i--) {
		auto model = tech.models.begin() + mos[i].model;
		if (model->bins.empty()) {
			continue;
		}
		int diff = tech.at(model->diff).draw;
		int minWidth = tech.getWidth(diff);

		int bin = -1;
		for (bin = (int)model->bins.size()-1;
			bin >= 0 and model->bins[bin].first > mos[i].size[1];
			bin--);

		if (model->bins[bin].second >= mos[i].size[1]) {
			continue;
		}

		int N = 2;
		for (; mos[i].size[1]/N > model->bins[bin].second; N++);

		int s0 = mos[i].size[1]/N;

		int b0 = -1;
		for (b0 = (int)model->bins.size()-1;
			b0 >= 0 and model->bins[b0].first > s0;
			b0--);
		
		if (model->bins[b0].second < s0) {
			s0 = model->bins[b0].second;
		}
		if (s0 < model->bins[0].first) {
			s0 = model->bins[0].first;
		}
		if (s0 < minWidth) {
			s0 = minWidth;
		}

		while (mos[i].size[1] > model->bins[b0].second) {
			mos[push(mos[i])].size[1] = s0;
			mos[i].size[1] -= s0;
		}
	}
}

void Subckt::apply(const ucs::mapping &m) {
	for (int i = 0; i < (int)ports.size(); i++) {
		int idx = m.unmap(ports[i]);
		if (idx < 0) {
			printf("error: %s not found in mapping\n", ports[i] < 0 ? "NULL" : nets[ports[i]].name.c_str());
		}
		ports[i] = idx;
	}

	for (int i = 0; i < (int)mos.size(); i++) {
		int gate = -1, source = -1, drain = -1, base = -1;
		for (int j = 0; j < (int)m.nets.size(); j++) {
			if (mos[i].gate == m.nets[j]) {
				gate = j;
			}
			if (mos[i].source == m.nets[j]) {
				source = j;
			}
			if (mos[i].drain == m.nets[j]) {
				drain = j;
			}
			if (mos[i].base == m.nets[j]) {
				base = j;
			}
		}

		if (gate < 0) {
			printf("error: gate %s not found in mapping\n", mos[i].gate < 0 ? "NULL" : nets[mos[i].gate].name.c_str());
		}
		if (source < 0) {
			printf("error: source %s not found in mapping\n", mos[i].source < 0 ? "NULL" : nets[mos[i].source].name.c_str());
		}
		if (drain < 0) {
			printf("error: drain %s not found in mapping\n", mos[i].drain < 0 ? "NULL" : nets[mos[i].drain].name.c_str());
		}
		if (base < 0) {
			printf("error: base %s not found in mapping\n", mos[i].base < 0 ? "NULL" : nets[mos[i].base].name.c_str());
		}
		mos[i].gate = gate;
		mos[i].source = source;
		mos[i].drain = drain;
		mos[i].base = base;
	}

	for (int i = 0; i < (int)nets.size(); i++) {
		for (int j = 0; j < (int)nets[i].remote.size(); j++) {
			int idx = m.unmap(nets[i].remote[j]);
			if (idx < 0) {
				printf("error: %s not found in mapping\n", nets[i].remote[j] < 0 ? "NULL" : nets[nets[i].remote[j]].name.c_str());
			}
			nets[i].remote[j] = idx;
		}
	}

	for (int i = 0; i < (int)inst.size(); i++) {
		for (int j = 0; j < (int)inst[i].ports.size(); j++) {
			int idx = m.unmap(inst[i].ports[j]);
			if (idx < 0) {
				printf("error: %s not found in mapping\n", inst[i].ports[j] < 0 ? "NULL" : nets[inst[i].ports[j]].name.c_str());
			}
			inst[i].ports[j] = idx;
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

ucs::mapping Subckt::canonicalize() {
	ucs::mapping lbl = canonicalLabels(*this);
	apply(lbl);
	for (int i = 0; i < (int)mos.size(); i++) {
		if (mos[i].drain < mos[i].source) {
			std::swap(mos[i].source, mos[i].drain);
		}
	}
	sort(mos.begin(), mos.end());
	for (auto n = nets.begin(); n != nets.end(); n++) {
		for (int type = 0; type < 2; type++) {
			n->sourceOf[type].clear();	
			n->drainOf[type].clear();	
			n->gateOf[type].clear();	
			n->baseOf[type].clear();
		}
	}
	for (int i = 0; i < (int)mos.size(); i++) {
		nets[mos[i].source].sourceOf[mos[i].type].push_back(i);
		nets[mos[i].drain].drainOf[mos[i].type].push_back(i);
		nets[mos[i].gate].gateOf[mos[i].type].push_back(i);
		nets[mos[i].base].baseOf[mos[i].type].push_back(i);
	}
	id = std::hash<Subckt>{}(*this);
	return lbl;
}

int Subckt::compare(const Subckt &ckt) const {
	// TODO(edward.bingham) This needs to consider device gates and bases, and
	// needs to consider device size
	if (nets.size() > ckt.nets.size()) {
		return 1;
	} else if (nets.size() < ckt.nets.size()) {
		return -1;
	}

	set<int> s0;
	set<int> s1;

	vector<Mos> m0, m1;
	for (int i = 0; i < (int)nets.size(); i++) {

		for (int type = 0; type < 2; type++) {
			for (auto j = nets[i].sourceOf[type].begin(); j != nets[i].sourceOf[type].end(); j++) {
				if (s0.find(*j) == s0.end()) {
					s0.insert(*j);
					m0.push_back(mos[*j]);
				}
			}
			for (auto j = nets[i].drainOf[type].begin(); j != nets[i].drainOf[type].end(); j++) {
				if (s0.find(*j) == s0.end()) {
					s0.insert(*j);
					m0.push_back(mos[*j]);
					std::swap(m0.back().source, m0.back().drain);	
				}
			}

			for (auto j = ckt.nets[i].sourceOf[type].begin(); j != ckt.nets[i].sourceOf[type].end(); j++) {
				if (s1.find(*j) == s1.end()) {
					s1.insert(*j);
					m1.push_back(ckt.mos[*j]);
				}
			}
			for (auto j = ckt.nets[i].drainOf[type].begin(); j != ckt.nets[i].drainOf[type].end(); j++) {
				if (s1.find(*j) == s1.end()) {
					s1.insert(*j);
					m1.push_back(ckt.mos[*j]);
					std::swap(m1.back().source, m1.back().drain);	
				}
			}
		}

		sort(m0.begin(), m0.end());
		sort(m1.begin(), m1.end());

		for (int j = 0; j < (int)m0.size() and j < (int)m1.size(); j++) {
			if (m0[j] < m1[j]) {
				return -1;
			} else if (m0[j] > m1[j]) {
				return 1;
			}
		}

		if (m0.size() < m1.size()) {
			return -1;
		} else if (m0.size() > m1.size()) {
			return 1;
		}
		m0.clear();
		m1.clear();
	}

	return 0;


	/*if (mos.size() > ckt.mos.size()) {
		return 1;
	} else if (mos.size() < ckt.mos.size()) {
		return -1;
	}

	vector<Mos> m0 = mos;
	vector<Mos> m1 = ckt.mos;

	for (int i = 0; i < (int)m0.size(); i++) {
		if (m0[i].drain < m0[i].source) {
			std::swap(m0[i].source, m0[i].drain);
		}
		if (m1[i].drain < m1[i].source) {
			std::swap(m1[i].source, m1[i].drain);
		}
	}

	sort(m0.begin(), m0.end());
	sort(m1.begin(), m1.end());
	if (m0 == m1) {
		return 0;
	} else if (m0 < m1) {
		return -1;
	} else {
		return 1;
	}*/
}

vector<Subckt::PartitionKey> Subckt::createPartitionKey(int net, const Partition &beta) const {
	vector<Subckt::PartitionKey> result;
	for (auto c = beta.cells.begin(); c != beta.cells.end(); c++) {
		Subckt::PartitionKey score;
		// remote nets
		for (int type = 0; type < 2; type++) {
			for (auto i = nets[net].sourceOf[type].begin(); i != nets[net].sourceOf[type].end(); i++) {
				bool hasDrain = std::find(c->begin(), c->end(), mos[*i].drain) != c->end();
				bool hasGate = std::find(c->begin(), c->end(), mos[*i].gate) != c->end();
				bool hasBase = std::find(c->begin(), c->end(), mos[*i].base) != c->end();
				score.addS(hasDrain, hasGate, hasBase, mos[*i].model, (1000*mos[*i].size[1])/mos[*i].size[0]);
			}
			for (auto i = nets[net].drainOf[type].begin(); i != nets[net].drainOf[type].end(); i++) {
				bool hasSource = std::find(c->begin(), c->end(), mos[*i].source) != c->end();
				bool hasGate = std::find(c->begin(), c->end(), mos[*i].gate) != c->end();
				bool hasBase = std::find(c->begin(), c->end(), mos[*i].base) != c->end();
				score.addS(hasSource, hasGate, hasBase, mos[*i].model, (1000*mos[*i].size[1])/mos[*i].size[0]);
			}
			for (auto i = nets[net].gateOf[type].begin(); i != nets[net].gateOf[type].end(); i++) {
				int sdCount = 0;
				sdCount += (std::find(c->begin(), c->end(), mos[*i].source) != c->end());
				sdCount += (std::find(c->begin(), c->end(), mos[*i].drain) != c->end());
				bool hasBase = std::find(c->begin(), c->end(), mos[*i].base) != c->end();
				score.addG(sdCount, hasBase, mos[*i].model, mos[*i].size[0]*mos[*i].size[1]);
			}
			for (auto i = nets[net].baseOf[type].begin(); i != nets[net].baseOf[type].end(); i++) {
				int sdCount = 0;
				sdCount += (std::find(c->begin(), c->end(), mos[*i].source) != c->end());
				sdCount += (std::find(c->begin(), c->end(), mos[*i].drain) != c->end());
				bool hasGate = std::find(c->begin(), c->end(), mos[*i].gate) != c->end();
				score.addB(sdCount, hasGate, mos[*i].model, mos[*i].size[0]*mos[*i].size[1]);
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
Subckt::PartitionKey Subckt::lambda(const Partition::Cell &c0, const Partition::Cell &c1) const {
	Subckt::PartitionKey result;
	if (c0.size() == 1 and c1.size() == 1 and c0[0] == c1[0]) {
		return result;
	}

	for (auto j = c0.begin(); j != c0.end(); j++) {
		auto n0 = nets.begin()+*j;

		for (int type = 0; type < 2; type++) {
			for (auto i = n0->sourceOf[type].begin(); i != n0->sourceOf[type].end(); i++) {
				bool hasDrain = std::find(c1.begin(), c1.end(), mos[*i].drain) != c1.end();
				bool hasGate = std::find(c1.begin(), c1.end(), mos[*i].gate) != c1.end();
				bool hasBase = std::find(c1.begin(), c1.end(), mos[*i].base) != c1.end();
				result.addS(hasDrain, hasGate, hasBase, mos[*i].model, (1000*mos[*i].size[1])/mos[*i].size[0]);
			}
			for (auto i = n0->drainOf[type].begin(); i != n0->drainOf[type].end(); i++) {
				bool hasSource = std::find(c1.begin(), c1.end(), mos[*i].source) != c1.end();
				bool hasGate = std::find(c1.begin(), c1.end(), mos[*i].gate) != c1.end();
				bool hasBase = std::find(c1.begin(), c1.end(), mos[*i].base) != c1.end();
				result.addS(hasSource, hasGate, hasBase, mos[*i].model, (1000*mos[*i].size[1])/mos[*i].size[0]);
			}
			for (auto i = n0->gateOf[type].begin(); i != n0->gateOf[type].end(); i++) {
				int sdCount = 0;
				sdCount += (std::find(c1.begin(), c1.end(), mos[*i].source) != c1.end());
				sdCount += (std::find(c1.begin(), c1.end(), mos[*i].drain) != c1.end());
				bool hasBase = std::find(c1.begin(), c1.end(), mos[*i].base) != c1.end();
				result.addG(sdCount, hasBase, mos[*i].model, mos[*i].size[0]*mos[*i].size[1]);
			}
			for (auto i = n0->baseOf[type].begin(); i != n0->baseOf[type].end(); i++) {
				int sdCount = 0;
				sdCount += (std::find(c1.begin(), c1.end(), mos[*i].source) != c1.end());
				sdCount += (std::find(c1.begin(), c1.end(), mos[*i].drain) != c1.end());
				bool hasGate = std::find(c1.begin(), c1.end(), mos[*i].gate) != c1.end();
				result.addB(sdCount, hasGate, mos[*i].model, mos[*i].size[0]*mos[*i].size[1]);
			}

			/*for (auto k = n0->drainOf[type].begin(); k != n0->drainOf[type].end(); k++) {
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
			for (auto k = n0->sourceOf[type].begin(); k != n0->sourceOf[type].end(); k++) {
				for (auto j = c1.begin(); j != c1.end(); j++) {
					auto n1 = nets.begin()+*j;
					if (n1->connectedTo(mos[*k].drain)) {
						result[0*2 + type]++;
					}
					if (n1->connectedTo(mos[*k].gate)) {
						result[1*2 + type]++;
					}
				}
			}*/
		}
	}
	return result;
}

struct lambda_sort {
    bool operator()(const vector<int> &a, const vector<int> &b) const {
        return a.size() < b.size() or (a.size() == b.size() and not a.empty() and a[0] < b[0]);
    }
};

vector<Subckt::PartitionKey> Subckt::lambda(Partition pi) const {
	// consistent sort order
	sort(pi.cells.begin(), pi.cells.end(), lambda_sort());

	vector<Subckt::PartitionKey> result;
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

	if (pi0.cells.size() > pi1.cells.size()) {
		return 1;
	} else if (pi0.cells.size() < pi1.cells.size()) {
		return -1;
	}

	vector<int> cell0(pi0.cells.size(), -1);
	vector<int> cell1(pi0.cells.size(), -1);
	for (int i = 0; i < (int)pi0.cells.size(); i++) {
		for (int j = 0; j < (int)pi0.cells[i].size(); j++) {
			cell0[pi0.cells[i][j]] = i;
		}
	}
	for (int i = 0; i < (int)pi1.cells.size(); i++) {
		for (int j = 0; j < (int)pi1.cells[i].size(); j++) {
			cell1[pi1.cells[i][j]] = i;
		}
	}

	vector<bool> s0(mos.size(), false);
	vector<bool> s1(mos.size(), false);

	vector<Mos> m0, m1;
	for (int i = 0; i < (int)pi0.cells.size(); i++) {
		int n0 = pi0.cells[i].back();
		int n1 = pi1.cells[i].back();

		for (int type = 0; type < 2; type++) {
			for (auto j = nets[n0].sourceOf[type].begin(); j != nets[n0].sourceOf[type].end(); j++) {
				if (not s0[*j]) {
					s0[*j] = true;
					m0.push_back(mos[*j]);
					m0.back().gate = cell0[m0.back().gate];
					m0.back().source = cell0[m0.back().source];
					m0.back().drain = cell0[m0.back().drain];
					m0.back().base = cell0[m0.back().base];	
				}
			}
			for (auto j = nets[n0].drainOf[type].begin(); j != nets[n0].drainOf[type].end(); j++) {
				if (not s0[*j]) {
					s0[*j] = true;
					m0.push_back(mos[*j]);
					m0.back().gate = cell0[m0.back().gate];
					m0.back().source = cell0[m0.back().source];
					m0.back().drain = cell0[m0.back().drain];
					m0.back().base = cell0[m0.back().base];
					std::swap(m0.back().source, m0.back().drain);	
				}
			}

			for (auto j = nets[n1].sourceOf[type].begin(); j != nets[n1].sourceOf[type].end(); j++) {
				if (not s1[*j]) {
					s1[*j] = true;
					m1.push_back(mos[*j]);
					m1.back().gate = cell1[m1.back().gate];
					m1.back().source = cell1[m1.back().source];
					m1.back().drain = cell1[m1.back().drain];
					m1.back().base = cell1[m1.back().base];	
				}
			}
			for (auto j = nets[n1].drainOf[type].begin(); j != nets[n1].drainOf[type].end(); j++) {
				if (not s1[*j]) {
					s1[*j] = true;
					m1.push_back(mos[*j]);
					m1.back().gate = cell1[m1.back().gate];
					m1.back().source = cell1[m1.back().source];
					m1.back().drain = cell1[m1.back().drain];
					m1.back().base = cell1[m1.back().base];
					std::swap(m1.back().source, m1.back().drain);	
				}
			}
		}

		sort(m0.begin(), m0.end());
		sort(m1.begin(), m1.end());

		for (int j = 0; j < (int)m0.size() and j < (int)m1.size(); j++) {
			if (m0[j] < m1[j]) {
				return -1;
			} else if (m0[j] > m1[j]) {
				return 1;
			}
		}

		if (m0.size() < m1.size()) {
			return -1;
		} else if (m0.size() > m1.size()) {
			return 1;
		}
		m0.clear();
		m1.clear();
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

	printf(" baseOf=");
	for (int type = 0; type < 2; type++) {
		printf("{");
		for (int j = 0; j < (int)nets[i].baseOf[type].size(); j++) {
			if (j != 0) {
				printf(", ");
			}
			printf("%d", nets[i].baseOf[type][j]);
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
	printf("%s[%d](%d) d=%s(%d) g=%s(%d) s=%s(%d) b=%s(%d) w=%d l=%d\n", (mos[i].type == 0 ? "nmos" : "pmos"), mos[i].model, i, nets[mos[i].drain].name.c_str(), mos[i].drain, nets[mos[i].gate].name.c_str(), mos[i].gate, nets[mos[i].source].name.c_str(), mos[i].source, nets[mos[i].base].name.c_str(), mos[i].base, mos[i].size[1], mos[i].size[0]);
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

void Subckt::PartitionKey::add(int key, int model, int score) {
	auto pos = scores.insert(pair<pair<int, int>, int>(pair<int, int>(key, model), 0));
	pos.first->second += score;
}

void Subckt::PartitionKey::addS(bool hasOther, bool hasGate, bool hasBase, int model, int score) {
	int type = -1;
	if (hasOther and hasGate and hasBase) {
		type = S2GB;
	} else if (hasOther and hasGate) {
		type = S2G;
	} else if (hasOther and hasBase) {
		type = S2B;
	} else if (hasOther) {
		type = S2;
	} else if (hasGate and hasBase) {
		type = S1GB;
	} else if (hasGate) {
		type = S1G;
	} else if (hasBase) {
		type = S1B;
	} else {
		return;
	}
	add(type, model, score);
}

void Subckt::PartitionKey::addG(int sdCount, bool hasBase, int model, int score) {
	int type = -1;
	if (sdCount == 2 and hasBase) {
		type = G2B;
	} else if (sdCount == 2 and not hasBase) {
		type = G2;
	} else if (sdCount == 1 and hasBase) {
		type = G1B;
	} else if (sdCount == 1 and not hasBase) {
		type = G1;
	} else if (sdCount == 0 and hasBase) {
		type = GB;
	} else {
		return;
	}
	add(type, model, score);
}

void Subckt::PartitionKey::addB(int sdCount, bool hasGate, int model, int score) {
	int type = -1;
	if (sdCount == 2 and hasGate) {
		type = B2G;
	} else if (sdCount == 2 and not hasGate) {
		type = B2;
	} else if (sdCount == 1 and hasGate) {
		type = B1G;
	} else if (sdCount == 1 and not hasGate) {
		type = B1;
	} else if (sdCount == 0 and hasGate) {
		type = BG;
	} else {
		return;
	}
	add(type, model, score);
}

bool operator==(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1) {
	return k0.scores == k1.scores;
}

bool operator!=(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1) {
	return k0.scores != k1.scores;
}

bool operator<(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1) {
	return k0.scores < k1.scores;
}

bool operator>(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1) {
	return k0.scores > k1.scores;
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
