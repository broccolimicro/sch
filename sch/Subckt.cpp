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

}
