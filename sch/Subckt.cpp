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

Index::Index() {
	type = -1;
	pin = -1;
}

Index::Index(int type, int pin) {
	this->type = type;
	this->pin = pin;
}

Index::~Index() {
}

bool operator<(const Index &i0, const Index &i1) {
	return i0.type < i1.type or (i0.type == i1.type and i0.pin < i1.pin);
}

bool operator==(const Index &i0, const Index &i1) {
	return i0.type == i1.type and i0.pin == i1.pin;
}

bool operator!=(const Index &i0, const Index &i1) {
	return i0.type != i1.type or i0.pin != i1.pin;
}

Pin::Pin(const Tech &tech) : layout(tech) {
	device = -1;
	outNet = -1;
	leftNet = -1;
	rightNet = -1;
	
	align = -1;

	layer = 0;
	width = 0;
	height = 0;
	pos = 0;
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::Pin(const Tech &tech, int outNet) : layout(tech) {
	this->device = -1;
	this->outNet = outNet;
	this->leftNet = outNet;
	this->rightNet = outNet;

	align = -1;

	layer = 1;
	width = 0;
	height = 0;
	pos = 0;
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::Pin(const Tech &tech, int device, int outNet, int leftNet, int rightNet) : layout(tech) {
	this->device = device;
	this->outNet = outNet;
	this->leftNet = leftNet;
	this->rightNet = rightNet;

	align = -1;

	layer = 0;
	width = 0;
	height = 0;
	pos = 0;
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::~Pin() {
}

void Pin::offsetToPin(Index pin, int value) {
	//printf("offsetToPin %d\n", value);
	auto result = toPin.insert(pair<Index, int>(pin, value));
	if (not result.second) {
		result.first->second = max(result.first->second, value);
	}
	//printf("Done inserting %d\n", result.first->second);
}

bool Pin::isGate() const {
	return device >= 0;
}

bool Pin::isContact() const {
	return device < 0;
}

Contact::Contact(const Tech &tech) : layout(tech) {
	left = numeric_limits<int>::min();
	right = numeric_limits<int>::max();
}

Contact::Contact(const Tech &tech, Index idx) : layout(tech) {
	this->idx = idx;
	this->left = numeric_limits<int>::min();
	this->right = numeric_limits<int>::max();
}

Contact::~Contact() {
}

void Contact::offsetFromPin(Index pin, int value) {
	auto result = fromPin.insert(pair<Index, int>(pin, value));
	if (not result.second) {
		result.first->second = max(result.first->second, value);
	}
}

void Contact::offsetToPin(Index pin, int value) {
	auto result = toPin.insert(pair<Index, int>(pin, value));
	if (not result.second) {
		result.first->second = max(result.first->second, value);
	}
}

bool operator<(const Contact &c0, const Contact &c1) {
	return c0.idx < c1.idx;
}

bool operator==(const Contact &c0, const Contact &c1) {
	return c0.idx == c1.idx;
}

bool operator!=(const Contact &c0, const Contact &c1) {
	return c0.idx != c1.idx;
}

CompareIndex::CompareIndex(const Subckt *s, bool orderIndex) {
	this->s = s;
	this->orderIndex = orderIndex;
}

CompareIndex::~CompareIndex() {
	orderIndex = true;
}

bool CompareIndex::operator()(const Index &i0, const Index &i1) {
	const Pin &p0 = s->pin(i0);
	const Pin &p1 = s->pin(i1);
	return p0.pos < p1.pos or (orderIndex and p0.pos == p1.pos and i0 < i1);
}

bool CompareIndex::operator()(const Contact &c0, const Index &i1) {
	const Pin &p0 = s->pin(c0.idx);
	const Pin &p1 = s->pin(i1);
	return p0.pos < p1.pos or (orderIndex and p0.pos == p1.pos and c0.idx < i1);
}

bool CompareIndex::operator()(const Contact &c0, const Contact &c1) {
	const Pin &p0 = s->pin(c0.idx);
	const Pin &p1 = s->pin(c1.idx);
	return p0.pos < p1.pos or (orderIndex and p0.pos == p1.pos and c0.idx < c1.idx);
}

Wire::Wire(const Tech &tech) : layout(tech) {
	net = -1;
	left = -1;
	right = -1;
	pOffset = 0;
	nOffset = 0;
}

Wire::Wire(const Tech &tech, int net) : layout(tech) {
	this->net = net;
	this->left = -1;
	this->right = -1;
	this->pOffset = 0;
	this->nOffset = 0;
}

Wire::~Wire() {
}

void Wire::addPin(const Subckt *s, Index pin) {
	auto pos = lower_bound(pins.begin(), pins.end(), pin, CompareIndex(s));
	pins.insert(pos, Contact(s->tech, pin));
	if (left < 0 or s->stack[pin.type].pins[pin.pin].pos < left) {
		left = s->stack[pin.type].pins[pin.pin].pos;
	}
	if (right < 0 or s->stack[pin.type].pins[pin.pin].pos+s->stack[pin.type].pins[pin.pin].width > right) {
		right = s->stack[pin.type].pins[pin.pin].pos + s->stack[pin.type].pins[pin.pin].width;
	}
}

bool Wire::hasPin(const Subckt *s, Index pin, vector<Contact>::iterator *out) {
	auto pos = lower_bound(pins.begin(), pins.end(), pin, CompareIndex(s));
	if (out != nullptr) {
		*out = pos;
	}
	return pos != pins.end() and pos->idx == pin;
}

void Wire::resortPins(const Subckt *s) {
	sort(pins.begin(), pins.end(), CompareIndex(s));
}

int Wire::getLevel(int i) const {
	if ((int)level.size() == 0) {
		return 2;
	}

	if (i < 0) {
		return level[0];
	}

	if (i >= (int)level.size()) {
		return level[level.size()-1];
	}

	return level[i];
}

bool Wire::hasPrev(int r) const {
	return prevNodes.find(r) != prevNodes.end();
}

bool Wire::hasGate(const Subckt *s) const {
	for (int i = 0; i < (int)pins.size(); i++) {
		if (s->pin(pins[i].idx).device >= 0) {
			return true;
		}
	}
	return false;
}

vector<bool> Wire::pinTypes() const {
	vector<bool> result(3,false);
	for (int i = 0; i < (int)pins.size(); i++) {
		result[pins[i].idx.type] = true;
	}
	return result;
}

Stack::Stack() {
	type = -1;
}

Stack::Stack(int type) {
	this->type = type;
}

Stack::~Stack() {
}

// index into Placement::dangling
void Stack::push(const Subckt *ckt, int device, bool flip) {
	int fromNet = -1;
	int toNet = -1;
	int gateNet = -1;

	if (device >= 0) {
		fromNet = ckt->mos[device].ports[flip];
		toNet = ckt->mos[device].ports[not flip];
		gateNet = ckt->mos[device].gate;
	}

	// Get information about the previous transistor on the stack. First if
	// statement in the funtion guarantees that there is at least one transistor
	// already on the stack.
	bool link = (pins.size() > 0 and gateNet >= 0 and fromNet == pins.back().rightNet);

	// We can't link this transistor to the previous one in the stack, so we
	// need to cap off the stack with a contact, start a new stack with a new
	// contact, then add this transistor. We need to test both the flipped and
	// unflipped orderings.

	if (not link and not pins.empty() and pins.back().isGate()) {
		pins.push_back(Pin(ckt->tech, pins.back().rightNet));
	}

	if (fromNet >= 0) {
		bool hasContact = (ckt->nets[fromNet].ports[type] > 2 or ckt->nets[fromNet].ports[1-type] > 0 or ckt->nets[fromNet].gates[0] > 0 or ckt->nets[fromNet].gates[1] > 0);
		if (fromNet >= 0 and (not link or pins.empty() or hasContact or ckt->nets[fromNet].isIO)) {
			// Add a contact for the first net or between two transistors.
			pins.push_back(Pin(ckt->tech, fromNet));
		}
	}

	if (device >= 0) {
		pins.push_back(Pin(ckt->tech, device, gateNet, fromNet, toNet));
	}
}

void Stack::draw(const Tech &tech, const Subckt *base, Layout &dst) {
	dst.clear();
	// Draw the stacks
	for (int i = 0; i < (int)pins.size(); i++) {
		vec2i dir = vec2i(1, type == Model::NMOS ? -1 : 1);
		drawLayout(dst, pins[i].layout, vec2i(pins[i].pos, 0), dir);
		if (i > 0 and (pins[i].device >= 0 or pins[i-1].device >= 0)) {
			int height = min(pins[i].height, pins[i-1].height);
			int model = -1;
			if (pins[i].device >= 0) {
				model = base->mos[pins[i].device].model;
			} else {
				model = base->mos[pins[i-1].device].model;
			}

			drawDiffusion(tech, dst, model, -1, vec2i(pins[i-1].pos, 0), vec2i(pins[i].pos, height)*dir, dir);
		}
	}
}

Subckt::Subckt(const Tech &tech) : tech(tech) {
	cellHeight = 0;
	for (int type = 0; type < (int)stack.size(); type++) {
		stack[type].type = type;
	}
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

Pin &Subckt::pin(Index i) {
	return stack[i.type].pins[i.pin];
}

const Pin &Subckt::pin(Index i) const {
	return stack[i.type].pins[i.pin];
}

// horizontal size of pin
int Subckt::pinWidth(Index p) const {
	int device = pin(p).device;
	if (device >= 0) {
		// this pin is a transistor, use length of transistor
		return tech.paint[tech.wires[0].draw].minWidth;
		//return base->mos[device].size[0];
	}
	// this pin is a contact
	return tech.paint[tech.wires[1].draw].minWidth;
}

// vertical size of pin
int Subckt::pinHeight(Index p) const {
	int device = pin(p).device;
	if (device >= 0) {
		// this pin is a transistor, use width of transistor
		return mos[device].size[1];
	}
	// this is a contact, height should be min of transistor widths on either side.
	int result = -1;
	if (p.pin-1 >= 0) {
		int leftDevice = stack[p.type].pins[p.pin-1].device;
		if (leftDevice >= 0 and (result < 0 or mos[leftDevice].size[1] < result)) {
			result = mos[leftDevice].size[1];
		}
	}
	for (int i = p.pin+1; i < (int)stack[p.type].pins.size(); i++) {
		int rightDevice = stack[p.type].pins[i].device;
		if (rightDevice >= 0 and (result < 0 or mos[rightDevice].size[1] < result)) {
			result = mos[rightDevice].size[1];
			break;
		} else if (result >= 0) {
			break;
		}
	}
	for (int i = p.pin-1; result < 0 and i >= 0; i--) {
		int leftDevice = stack[p.type].pins[i].device;
		if (leftDevice >= 0 and (result < 0 or mos[leftDevice].size[1] < result)) {
			result = mos[leftDevice].size[1];
		}
	}
	if (result < 0) {
		// This should never happen.
		printf("warning: failed to size the pin (%d,%d).\n", p.type, p.pin);
		return 0;
	}
	return result;
}

void Subckt::draw(Layout &dst) {
	vec2i dir(1,-1);
	dst.name = name;

	dst.nets.reserve(nets.size());
	for (int i = 0; i < (int)nets.size(); i++) {
		dst.nets.push_back(nets[i].name);
	}

	for (int i = 0; i < (int)routes.size(); i++) {
		drawLayout(dst, routes[i].layout, vec2i(0, routes[i].pOffset)*dir, dir);
	}

	for (int type = 0; type < (int)stack.size(); type++) {
		for (int i = 0; i < (int)stack[type].pins.size(); i++) {
			const Pin &pin = stack[type].pins[i];
			bool first = true;
			int bottom = 0;
			int top = 0;
			
			int pinLevel = pin.layer;
			int pinLayer = tech.wires[pinLevel].draw;
			int width = tech.paint[pinLayer].minWidth;

			for (int j = 0; j < (int)routes.size(); j++) {
				if (routes[j].hasPin(this, Index(type, i))) {
					int v = routes[j].pOffset;
					if (routes[j].net >= 0) {
						top = first ? v+width : max(top, v+width);
					} else {
						top = first ? v : max(top, v);
					}
					bottom = first ? v : min(bottom, v);
					first = false;
				}
			}

 			dst.push(tech.wires[pinLevel], Rect(pin.outNet, vec2i(pin.pos, bottom)*dir, vec2i(pin.pos+width, top)*dir));
		}
	}

	for (int i = 0; i < (int)dst.layers.size(); i++) {
		if (tech.paint[dst.layers[i].draw].fill) {
			Rect box = dst.layers[i].bbox();
			dst.layers[i].clear();
			dst.layers[i].push(box, true);
		}
	}

	if (tech.boundary >= 0) {
		dst.push(tech.boundary, dst.bbox()); 
	}

	dst.merge();
}

}