#include "Subckt.h"
#include "Draw.h"
#include <limits>
#include <algorithm>

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
}

Net::~Net() {
}

int Net::ports(int type) const {
	return (int)(sourceOf[type].size() + drainOf[type].size());
} 

bool Net::hasContact(int type) const {
	return isIO
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
		int index = (int)nets.size();
		nets.push_back(Net(name));
		return index;
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
	return result;
}

int Subckt::pushMos(int model, int type, int drain, int gate, int source, int base, vec2i size) {
	int result = (int)mos.size();
	nets[drain].drainOf[type].push_back(result);
	nets[source].sourceOf[type].push_back(result);
	nets[gate].gateOf[type].push_back(result);

	mos.push_back(Mos(model, type, drain, gate, source, base, size));
	return result;
}

vector<Mapping> Subckt::find(const Subckt &cell, int index) {
	struct frame {
		Mapping m;
		vector<int> todo;  // devs in cell
	};

	vector<Mapping> result;
	vector<frame> frames;
	frames.push_back(frame());
	frames.back().m.cell = index;
	for (int i = 0; i < (int)cell.mos.size(); i++) {
		frames.back().todo.push_back(i);
		frames.back().m.cellToThis.resize(cell.nets.size(), -1);
	}
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

}
