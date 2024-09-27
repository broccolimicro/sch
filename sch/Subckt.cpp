#include "Subckt.h"
#include "Draw.h"
#include <limits>
#include <algorithm>

using namespace std;

namespace sch {

Mos::Mos() {
	model = -1;
	type = -1;
	size = vec2i(0,0);
}

Mos::Mos(int model, int type) {
	this->model = model;
	this->type = type;
	this->size = vec2i(0,0);
}

Mos::~Mos() {
}

int Mos::left(bool flip) const {
	return flip ? ports[1] : ports[0];
}

int Mos::right(bool flip) const {
	return flip ? ports[0] : ports[1];
}

Net::Net() {
	ports[0] = 0;
	ports[1] = 0;
	gates[0] = 0;
	gates[1] = 0;
	isIO = false;
}

Net::Net(string name, bool isIO) {
	this->name = name;
	this->ports[0] = 0;
	this->ports[1] = 0;
	this->gates[0] = 0;
	this->gates[1] = 0;
	this->isIO = isIO;
}

Net::~Net() {
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

}
