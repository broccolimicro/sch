#include <algorithm>
#include <unordered_set>
#include <set>
#include <list>

#include "Router.h"
#include "Draw.h"

namespace sch {

Pin::Pin(const Tech &tech) : layout(tech) {
	device = -1;
	outNet = -1;
	leftNet = -1;
	rightNet = -1;
	
	width = 0;
	height = 0;
	offset[0] = 0;
	offset[1] = 0;
	bound[0] = 0;
	bound[1] = 0;
	layer = Level(Level::ROUTE, 0);
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::Pin(const Tech &tech, int outNet, int baseNet) : layout(tech) {
	this->device = -1;
	this->outNet = outNet;
	this->leftNet = outNet;
	this->rightNet = outNet;
	this->baseNet = baseNet;

	width = 0;
	height = 0;
	offset[0] = 0;
	offset[1] = 0;
	bound[0] = 0;
	bound[1] = 0;
	layer = Level(Level::ROUTE, 1);
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::Pin(const Tech &tech, int device, int outNet, int leftNet, int rightNet, int baseNet) : layout(tech) {
	this->device = device;
	this->outNet = outNet;
	this->leftNet = leftNet;
	this->rightNet = rightNet;
	this->baseNet = baseNet;

	width = 0;
	height = 0;
	offset[0] = 0;
	offset[1] = 0;
	bound[0] = 0;
	bound[1] = 0;
	layer = Level(Level::ROUTE, 0);
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::~Pin() {
}

bool Pin::isGate() const {
	return device >= 0;
}

bool Pin::isContact() const {
	return device < 0;
}

Contact::Contact(const Tech &tech) : layout(tech) {
	offset[0] = numeric_limits<int>::min();
	offset[1] = numeric_limits<int>::min();
	bound[0] = numeric_limits<int>::max();
	bound[1] = numeric_limits<int>::max();
}

Contact::Contact(const Tech &tech, Index idx) : layout(tech) {
	this->idx = idx;
	this->offset[0] = numeric_limits<int>::min();
	this->offset[1] = numeric_limits<int>::min();
	this->bound[0] = numeric_limits<int>::max();
	this->bound[1] = numeric_limits<int>::max();
}

Contact::~Contact() {
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

CompareIndex::CompareIndex(const Router *rt, bool orderIndex) {
	this->rt = rt;
	this->orderIndex = orderIndex;
}

CompareIndex::~CompareIndex() {
	orderIndex = true;
}

bool CompareIndex::operator()(const Index &i0, const Index &i1) {
	const Pin &p0 = rt->pin(i0);
	const Pin &p1 = rt->pin(i1);
	return p0.offset[0] < p1.offset[0] or (orderIndex and p0.offset[0] == p1.offset[0] and i0 < i1);
}

bool CompareIndex::operator()(const Contact &c0, const Index &i1) {
	const Pin &p0 = rt->pin(c0.idx);
	const Pin &p1 = rt->pin(i1);
	return p0.offset[0] < p1.offset[0] or (orderIndex and p0.offset[0] == p1.offset[0] and c0.idx < i1);
}

bool CompareIndex::operator()(const Contact &c0, const Contact &c1) {
	const Pin &p0 = rt->pin(c0.idx);
	const Pin &p1 = rt->pin(c1.idx);
	return p0.offset[0] < p1.offset[0] or (orderIndex and p0.offset[0] == p1.offset[0] and c0.idx < c1.idx);
}

Wire::Wire(const Tech &tech) : layout(tech) {
	net = -1;
	left = -1;
	right = -1;
	offset[Model::PMOS] = 0;
	offset[Model::NMOS] = 0;
}

Wire::Wire(const Tech &tech, int net) : layout(tech) {
	this->net = net;
	this->left = -1;
	this->right = -1;
	this->offset[Model::PMOS] = 0;
	this->offset[Model::NMOS] = 0;
}

Wire::~Wire() {
}

void Wire::addPin(const Router *rt, Contact ct) {
	auto pos = lower_bound(pins.begin(), pins.end(), ct.idx, CompareIndex(rt));
	pins.insert(pos, ct);
	const Pin &pin = rt->pin(ct.idx);
	if (left < 0 or pin.offset[0] < left) {
		left = pin.offset[0];
	}
	if (right < 0 or pin.offset[0]+pin.width > right) {
		right = pin.offset[0] + pin.width;
	}
}

int Wire::findPin(const Router *rt, Index pin) const {
	auto pos = lower_bound(pins.begin(), pins.end(), pin, CompareIndex(rt));
	if (pos != pins.end() and pos->idx == pin) {
		return pos - pins.begin();
	}
	return -1;
}

bool Wire::hasPin(const Router *rt, Index pin) const {
	return findPin(rt, pin) != -1;
}

void Wire::resortPins(const Router *rt) {
	if (not pins.empty()) {
		sort(pins.begin(), pins.end(), CompareIndex(rt));
		left = rt->pin(pins[0].idx).offset[0];
		right = rt->pin(pins.back().idx).offset[0]+rt->pin(pins.back().idx).width;
	} else {
		left = -1;
		right = -1;
	}
}

Level Wire::getLevel(int i) const {
	if ((int)level.size() == 0) {
		return Level(Level::ROUTE, 2);
	}

	if (i < 0) {
		return Level(Level::ROUTE, level[0]);
	}

	if (i >= (int)level.size()) {
		return Level(Level::ROUTE, level[level.size()-1]);
	}

	return Level(Level::ROUTE, level[i]);
}

bool Wire::hasGate(const Router *rt) const {
	for (int i = 0; i < (int)pins.size(); i++) {
		if (rt->pin(pins[i].idx).device >= 0) {
			return true;
		}
	}
	return false;
}

int Wire::numSourceDrain(const Router *rt) const {
	int result = 0;
	for (int i = 0; i < (int)pins.size(); i++) {
		result += (rt->pin(pins[i].idx).device < 0);
	}
	return result;
}

vector<bool> Wire::pinTypes() const {
	vector<bool> result(3,false);
	for (int i = 0; i < (int)pins.size(); i++) {
		result[pins[i].idx.type] = true;
	}
	return result;
}

void Wire::buildContacts(const Router *rt) {
	if (net < 0) {
		return;
	}

	for (int j = 0; j < (int)pins.size(); j++) {
		const Pin &pin = rt->pin(pins[j].idx);
		Level prevLevel = getLevel(j-1);
		Level nextLevel = getLevel(j);
		Level maxLevel = max(pin.layer, max(nextLevel, prevLevel));
		Level minLevel = min(pin.layer, min(nextLevel, prevLevel));

		pins[j].layout.clear();
		drawViaStack(pins[j].layout, net, pin.baseNet, minLevel, maxLevel, vec2i(0, 0), vec2i(0,0), vec2i(0,0));
	}
}

Stack::Stack() {
	type = -1;
	route = -1;
}

Stack::Stack(int type) {
	this->type = type;
	this->route = -1;
}

Stack::~Stack() {
}

// index into Placement::dangling
void Stack::push(const Tech &tech, const Subckt &ckt, int device, bool flip) {
	int fromNet = -1;
	int toNet = -1;
	int gateNet = -1;
	int baseNet = -1;
	int prevModel = -1;
	if (not pins.empty() and pins.back().isGate()) {
		prevModel = ckt.mos[pins.back().device].model;
	} else if (pins.size() >= 2 and pins[(int)pins.size()-2].isGate()) {
		prevModel = ckt.mos[pins[(int)pins.size()-2].device].model;
	}
	int model = -1;

	if (device >= 0) {
		fromNet = ckt.mos[device].left(flip);
		toNet = ckt.mos[device].right(flip);
		gateNet = ckt.mos[device].gate;
		baseNet = ckt.mos[device].base;
		model = ckt.mos[device].model;
	}

	// Get information about the previous transistor on the stack. First if
	// statement in the funtion guarantees that there is at least one transistor
	// already on the stack.
	bool link = (pins.size() > 0 and gateNet >= 0 and fromNet == pins.back().rightNet and baseNet == pins.back().baseNet and (prevModel < 0 or model < 0 or prevModel == model));

	// We can't link this transistor to the previous one in the stack, so we
	// need to cap off the stack with a contact, start a new stack with a new
	// contact, then add this transistor. We need to test both the flipped and
	// unflipped orderings.

	if (not link and not pins.empty() and pins.back().isGate()) {
		pins.push_back(Pin(tech, pins.back().rightNet, pins.back().baseNet));
	}

	if (fromNet >= 0 and (not link or pins.empty() or ckt.nets[fromNet].hasContact(type))) {
		// Add a contact for the first net or between two transistors.
		pins.push_back(Pin(tech, fromNet, baseNet));
	}

	if (device >= 0) {
		pins.push_back(Pin(tech, device, gateNet, fromNet, toNet, baseNet));
	}
}

Router::Router(const Tech &tech, const Placement &place, bool progress, bool debug) {
	this->tech = &tech;
	this->ckt = place.ckt;
	this->cycleCount = 0;
	this->cellHeight = 0;
	this->cost = 0;
	this->progress = progress;
	this->debug = debug;
	this->allowOverCell = true;
	this->unresolvedCycle[0] = false;
	this->unresolvedCycle[1] = false;
	this->unresolvedPinCycle[0] = false;
	this->unresolvedPinCycle[1] = false;
	for (int type = 0; type < (int)this->stack.size(); type++) {
		this->stack[type].type = type;
	}
	this->load(place);
}

Router::~Router() {
}

Pin &Router::pin(Index i) {
	return stack[i.type].pins[i.pin];
}

const Pin &Router::pin(Index i) const {
	return stack[i.type].pins[i.pin];
}

// horizontal size of pin
int Router::pinWidth(Index p) const {
	int device = pin(p).device;
	if (device >= 0) {
		// this pin is a transistor, use length of transistor
		//return tech->getWidth(tech->wires[0].draw);
		return ckt->mos[device].size[0];
	}
	// this pin is a contact
	return tech->getWidth(tech->wires[1].draw);
}

// vertical size of pin
int Router::pinHeight(Index p) const {
	int device = pin(p).device;
	if (device >= 0) {
		// this pin is a transistor, use width of transistor
		return ckt->mos[device].size[1];
	}
	// this is a contact, height should be min of transistor widths on either side.
	int result = -1;
	if (p.pin-1 >= 0) {
		int leftDevice = stack[p.type].pins[p.pin-1].device;
		if (leftDevice >= 0 and (result < 0 or ckt->mos[leftDevice].size[1] < result)) {
			result = ckt->mos[leftDevice].size[1];
		}
	}
	for (int i = p.pin+1; i < (int)stack[p.type].pins.size(); i++) {
		int rightDevice = stack[p.type].pins[i].device;
		if (rightDevice >= 0 and (result < 0 or ckt->mos[rightDevice].size[1] < result)) {
			result = ckt->mos[rightDevice].size[1];
			break;
		} else if (result >= 0) {
			break;
		}
	}
	for (int i = p.pin-1; result < 0 and i >= 0; i--) {
		int leftDevice = stack[p.type].pins[i].device;
		if (leftDevice >= 0 and (result < 0 or ckt->mos[leftDevice].size[1] < result)) {
			result = ckt->mos[leftDevice].size[1];
		}
	}
	if (result < 0) {
		// This should never happen.
		printf("warning: failed to size the pin (%d,%d).\n", p.type, p.pin);
		return 0;
	}
	return result;
}

void Router::propagateOrderMap(vector<bitset> &m, set<int> todo) {
	if (todo.empty()) {
		for (int i = 0; i < (int)m.size(); i++) {
			todo.insert(i);
		}
	}
	while (not todo.empty()) {
		int curr = *todo.begin();
		todo.erase(todo.begin());
	
		for (int i = 0; i < (int)m.size(); i++) {
			if (m[i].get(curr) and not m[curr].isSubsetOf(m[i])) {
				m[i] |= m[curr];
				todo.insert(i);
			}
		}
	}
}

vector<bitset> Router::routeOrderMap(int type) {
	vector<bitset> result;
	result.resize(routes.size());
	for (int i = 0; i < (int)routes.size(); i++) {
		auto n = type == Model::PMOS ? next(i) : prev(i);
		for (auto j = n.begin(); j != n.end(); j++) {
			result[j->first].set(i, true);
		}
	}

	propagateOrderMap(result);
	return result;
}

vector<bitset> Router::pinOrderMap() {
	array<int, 3> offset = {0, (int)stack[0].pins.size(),
		(int)stack[0].pins.size()+(int)stack[1].pins.size()};

	vector<bitset> result;
	result.resize(offset[2] + stack[2].pins.size());
	for (auto cnst = stackConstraints.begin(); cnst != stackConstraints.end(); cnst++) {
		if (cnst->select >= 0) {
			Index from = cnst->pins[cnst->select];
			Index to = cnst->pins[1-cnst->select];
			int fromIdx = offset[from.type]+from.pin;
			int toIdx = offset[to.type]+to.pin;

			result[toIdx].set(fromIdx, true);
		}
	}


	for (auto route = routes.begin(); route != routes.end(); route++) {
		for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
			for (auto cnst = ct->constraints.begin(); cnst != ct->constraints.end(); cnst++) {
				if (cnst->select >= 0) {
					Index from = cnst->select == 0 ? cnst->pin : ct->idx;
					Index to = cnst->select == 0 ? ct->idx : cnst->pin;

					int fromIdx = offset[from.type]+from.pin;
					int toIdx = offset[to.type]+to.pin;

					result[toIdx].set(fromIdx, true);
				}
			}
		}
	}

	propagateOrderMap(result);
	return result;
}


vector<set<int> > Router::createAdjacencyList() {
	vector<set<int> > Ak(routes.size(), set<int>());
	for (int i = 0; i < (int)routes.size(); i++) {
		for (auto c = routeConstraints.begin(); c != routeConstraints.end(); c++) {
			if (c->select >= 0 and c->wires[c->select] == i) {
				Ak[i].insert(c->wires[1-c->select]);
			}
		}

		for (auto c = pinConstraints.begin(); c != pinConstraints.end(); c++) {
			if (routes[i].hasPin(this, Index(Model::PMOS, c->from))) {
				for (int j = 0; j < (int)routes.size(); j++) {
					if (j != i and Ak[i].find(j) == Ak[i].end()
						and routes[j].hasPin(this, Index(Model::NMOS, c->to))) {
						Ak[i].insert(j);
					}
				}
			}
		}
	}
	return Ak;
}

void Router::delRoute(int route) {
	for (int i = (int)routeConstraints.size()-1; i >= 0; i--) {
		if (routeConstraints[i].wires[0] == route or routeConstraints[i].wires[1] == route) {
			routeConstraints.erase(routeConstraints.begin()+i);
		} else {
			if (routeConstraints[i].wires[0] > route) {
				routeConstraints[i].wires[0]--;
			}
			if (routeConstraints[i].wires[1] > route) {
				routeConstraints[i].wires[1]--;
			}
		}
	}
	for (int i = 0; i < (int)stack.size(); i++) {
		if (stack[i].route > route) {
			stack[i].route--;
		} else if (stack[i].route == route) {
			stack[i].route = -1;
		}
	}

	routes.erase(routes.begin()+route);
}

// depends on:
// buildPinOffsets() - this determines what the pin constraints are
bool Router::buildPinConstraints(int level, bool reset) {
	// TODO(edward.bingham) this could be more efficiently done as a 1d rectangle
	// overlap problem
	set<PinConstraint> old;
	old.swap(pinConstraints);
	if (level == 0) {
		// Compare pin layout (without contact) to pin layout
		for (int p = 0; p < (int)this->stack[Model::PMOS].pins.size(); p++) {
			for (int n = 0; n < (int)this->stack[Model::NMOS].pins.size(); n++) {
				int off = 0;
				Pin &pmos = this->stack[Model::PMOS].pins[p];
				Pin &nmos = this->stack[Model::NMOS].pins[n];
				if (pmos.outNet != nmos.outNet and
					minOffset(&off, 1, pmos.layout, pmos.offset[0],
														 nmos.layout, nmos.offset[0],
										Layout::IGNORE, Layout::MERGENET)) {
					pinConstraints.insert(PinConstraint(p, n));
				}
			}
		}
	} else if (level == 1) {
		// Compare pin layout (without contact) to contact layout
		for (auto r0 = routes.begin(); r0 != routes.end(); r0++) {
			for (auto ct = r0->pins.begin(); ct != r0->pins.end(); ct++) {
				if (ct->idx.type == Model::NMOS or ct->idx.type == Model::PMOS) {
					for (int i = 0; i < (int)this->stack[1-ct->idx.type].pins.size(); i++) {
						int off = 0;
						const Pin &pmos = ct->idx.type == Model::PMOS ? this->pin(ct->idx) : this->stack[1-ct->idx.type].pins[i];
						const Pin &nmos = ct->idx.type == Model::NMOS ? this->pin(ct->idx) : this->stack[1-ct->idx.type].pins[i];
						const Layout &playout = ct->idx.type == Model::PMOS ? ct->layout : pmos.layout;
						const Layout &nlayout = ct->idx.type == Model::NMOS ? ct->layout : nmos.layout;
						int p = ct->idx.type == Model::PMOS ? ct->idx.pin : i;
						int n = ct->idx.type == Model::NMOS ? ct->idx.pin : i;
						if (pmos.outNet != nmos.outNet and
							minOffset(&off, 1, playout, pmos.offset[0],
																 nlayout, nmos.offset[0],
												Layout::IGNORE, Layout::MERGENET)) {
							pinConstraints.insert(PinConstraint(p, n));
						}
					}
				}
			}
		}
	} else if (level == 2) {
		// Compare contact layout to contact layout
		for (auto r0 = routes.begin(); r0 != routes.end(); r0++) {
			for (auto r1 = r0+1; r1 != routes.end(); r1++) {
				if (r0->net != r1->net) {
					for (auto c0 = r0->pins.begin(); c0 != r0->pins.end(); c0++) {
						for (auto c1 = r1->pins.begin(); c1 != r1->pins.end(); c1++) {
							if ((c0->idx.type == Model::NMOS and c1->idx.type == Model::PMOS)
								or (c0->idx.type == Model::PMOS and c1->idx.type == Model::NMOS)) {
								int off = 0;
								const Pin &pmos = c0->idx.type == Model::PMOS ? this->pin(c0->idx) : this->pin(c1->idx);
								const Pin &nmos = c0->idx.type == Model::NMOS ? this->pin(c0->idx) : this->pin(c1->idx);
								int p = c0->idx.type == Model::PMOS ? c0->idx.pin : c1->idx.pin;
								int n = c0->idx.type == Model::NMOS ? c0->idx.pin : c1->idx.pin;
								if (minOffset(&off, 1, c0->layout, pmos.offset[0],
								                       c1->layout, nmos.offset[0],
								                 Layout::IGNORE, Layout::MERGENET)) {
									pinConstraints.insert(PinConstraint(p, n));
								}
							}
						}
					}
				}
			}
		}
	}

	if (not reset) {
		for (auto c = old.begin(); c != old.end(); c++) {
			pinConstraints.insert(*c);
		}
	}
	if (pinConstraints.size() != old.size()) {
		return true;
	}
	auto c0 = pinConstraints.begin();
	auto c1 = old.begin();
	while (c0 != pinConstraints.end() and c1 != old.end()) {
		if (!(*c0 == *c1)) {
			return true;
		}
		c0++;
		c1++;
	}
	return false;
}

bool Router::lockPinConstraints() {
	// TODO(edward.bingham) I need to check pairs of pins and set up a
	// stack constraint between them if there isn't a pin constraint
	// between them. { PMOS -> 2, PMOS -> NMOS, 2 -> NMOS }
	bool change = false;
	for (auto c0 = pinConstraints.begin(); c0 != pinConstraints.end(); c0++) {
		array<bool, 2> hasPrev = {
			c0->to-1 < 0,
			c0->from-1 < 0
		};
		array<bool, 2> hasNext = {
			c0->to+1 >= (int)stack[Model::NMOS].pins.size(),
			c0->from+1 >= (int)stack[Model::PMOS].pins.size()
		};
		for (auto c1 = pinConstraints.begin(); c1 != pinConstraints.end(); c1++) {
			if (c0->from == c1->from and c0->to+1 == c1->to) {
				hasNext[Model::NMOS] = true;
			}
			if (c0->from == c1->from and c0->to-1 == c1->to) {
				hasPrev[Model::NMOS] = true;
			}
			if (c0->from+1 == c1->from and c0->to == c1->to) {
				hasNext[Model::PMOS] = true;
			}
			if (c0->from-1 == c1->from and c0->to == c1->to) {
				hasPrev[Model::PMOS] = true;
			}
		}

		for (auto cnst = stackConstraints.begin(); cnst != stackConstraints.end(); cnst++) {
			for (int d = 0; d < 2; d++) {
				if (not hasNext[Model::NMOS]
					and cnst->select < 0
					and cnst->pins[d] == Index(Model::PMOS, c0->from)
					and cnst->pins[1-d] == Index(Model::NMOS, c0->to+1)) {
					cnst->select = d;
					change = true;
				}
				if (not hasNext[Model::PMOS]
					and cnst->select < 0
					and cnst->pins[d] == Index(Model::PMOS, c0->from+1)
					and cnst->pins[1-d] == Index(Model::NMOS, c0->to)) {
					cnst->select = 1-d;
					change = true;
				}
				if (not hasPrev[Model::NMOS]
					and cnst->select < 0
					and cnst->pins[d] == Index(Model::PMOS, c0->from)
					and cnst->pins[1-d] == Index(Model::NMOS, c0->to-1)) {
					cnst->select = 1-d;
					change = true;
				}
				if (not hasPrev[Model::PMOS]
					and cnst->select < 0
					and cnst->pins[d] == Index(Model::PMOS, c0->from-1)
					and cnst->pins[1-d] == Index(Model::NMOS, c0->to)) {
					cnst->select = d;
					change = true;
				}
			}
		}
	}
	return change;
}

void Router::buildViaConstraints() {
	viaConstraints.clear();
	// Compute via constraints
	/*for (int type = 0; type < 2; type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			if (this->stack[type].pins[i].conLayout.layers.size() == 0) {
				continue;
			}

			viaConstraints.push_back(ViaConstraint(Index(type, i)));
	
			for (int j = i-1; j >= 0; j--) {
				int off = 0;
				if (minOffset(&off, 0, this->stack[type].pins[j].layout, 0, this->stack[type].pins[i].conLayout, this->stack[type].pins[j].height/2, Layout::IGNORE, Layout::MERGENET)) {
					viaConstraints.back().side[0].push_back(ViaConstraint::Pin{Index(type, j), off});
				}
			}

			for (int j = i+1; j < (int)this->stack[type].pins.size(); j++) {
				int off = 0;
				if (minOffset(&off, 0, this->stack[type].pins[i].conLayout, this->stack[type].pins[j].height/2, this->stack[type].pins[j].layout, 0, Layout::IGNORE, Layout::MERGENET)) {
					viaConstraints.back().side[1].push_back(ViaConstraint::Pin{Index(type, j), off});
				}
			}

			if (viaConstraints.back().side[0].empty() or viaConstraints.back().side[1].empty()) {
				viaConstraints.pop_back();
			}
		}
	}*/
}

map<int, int> Router::next(int i) {
	map<int, int> result;
	for (auto c = routeConstraints.begin(); c != routeConstraints.end(); c++) {
		if (c->select >= 0 and c->wires[c->select] == i) {
			result.insert(pair<int, int>(c->wires[1-c->select], c->off[c->select]));
		}
	}

	for (auto c = pinConstraints.begin(); c != pinConstraints.end(); c++) {
		if (routes[i].hasPin(this, Index(Model::PMOS, c->from))) {
			for (int j = 0; j < (int)routes.size(); j++) {
				if (j != i and result.find(j) == result.end()
					and routes[j].hasPin(this, Index(Model::NMOS, c->to))) {
					result.insert(pair<int, int>(j, 0));
				}
			}
		}
	}
	return result;
}

map<int, int> Router::prev(int i) {
	map<int, int> result;
	for (auto c = routeConstraints.begin(); c != routeConstraints.end(); c++) {
		if (c->select >= 0 and c->wires[1-c->select] == i) {
			result.insert(pair<int, int>(c->wires[c->select], c->off[c->select]));
		}
	}

	for (auto c = pinConstraints.begin(); c != pinConstraints.end(); c++) {
		if (routes[i].hasPin(this, Index(Model::NMOS, c->to))) {
			for (int j = 0; j < (int)routes.size(); j++) {
				if (j != i and result.find(j) == result.end()
					and routes[j].hasPin(this, Index(Model::PMOS, c->from))) {
					result.insert(pair<int, int>(j, 0));
				}
			}
		}
	}
	return result;
}

bool Router::hasPinConstraint(int from, int to) {
	for (auto i = pinConstraints.begin(); i != pinConstraints.end(); i++) {
		if (routes[from].hasPin(this, Index(Model::PMOS, i->from))
			and routes[to].hasPin(this, Index(Model::NMOS, i->to))) {
			return true;
		}
	}
	return false;
}

bool Router::findCycle(int s, const vector<set<int> > &Ak, vector<pair<int, set<int> > > *cycles) {
	/*printf("looking for cycle\n");
	for (int i = 0; i < (int)Ak.size(); i++) {
		printf("%d:{", i);
		for (auto w = Ak[i].begin(); w != Ak[i].end(); w++) {
			printf("%d ", *w);
		}
		printf("}\n");
	}*/
	vector<bool> blocked(Ak.size(), false);
	vector<set<int> > B(Ak.size(), set<int>());

	struct frame {
		frame() {
			searched = false;
		}
		frame(vector<int> path) {
			this->path = path;
			searched = false;
		}
		~frame() {}

		vector<int> path;
		bool searched;
	};

	vector<frame> stack;
	stack.push_back(frame(vector<int>(1, s)));

	bool found = false;
	while (not stack.empty()) {
		int v = stack.back().path.back();
		//printf("stack=%d %d/%d\n", (int)stack.size(), v, (int)Ak.size());
		if (stack.back().searched) {
			if (found) {
				// Unblock vertex v
				vector<int> unblock(1, v);
				while (not unblock.empty()) {
					//printf("\tunblock=%d\n", (int)unblock.size());
					int u = unblock.back();
					unblock.pop_back();
					blocked[u] = false;
					for (auto w = B[u].begin(); w != B[u].end(); w++) {
						if (blocked[*w]) {
							unblock.push_back(*w);
						}
					}
					B[u].clear();
				}
			} else {
				for (auto w = Ak[v].begin(); w != Ak[v].end(); w++) {
					B[*w].insert(v);
				}
			}
			stack.pop_back();
		} else {
			stack.back().searched = true;
			vector<int> path = stack.back().path;

			blocked[v] = true;
			for (auto w = Ak[v].begin(); w != Ak[v].end(); w++) {
				if (*w == s) {
					// We found a cycle
					if (cycles == nullptr) {
						//printf("done\n");
						return true;
					}

					for (auto c0 = path.begin(); c0 != path.end(); c0++) {
						(*cycles)[*c0].first++;
						for (auto c1 = path.begin(); c1 != path.end(); c1++) {
							if (*c1 != *c0) {
								(*cycles)[*c0].second.insert(*c1);
							}
						}
					}
					found = true;
				} else if (not blocked[*w]) {
					stack.push_back(frame(path));
					stack.back().path.push_back(*w);
				}
			}
		}
	}

	//printf("done\n");
	return found;
}

bool Router::findCycles(vector<pair<int, set<int> > > *cycles) {
	// DESIGN(edward.bingham) There can be multiple cycles with the same set of
	// nodes as a result of multiple pin constraints. This function does not
	// differentiate between those cycles. Doing so could introduce an
	// exponential blow up, and we can ensure that we split those cycles by
	// splitting on the node in the cycle that has the most pin constraints
	// (maximising min(in.size(), out.size()))
	vector<set<int> > Ak = createAdjacencyList();
	bool found = false;
	for (int s = 0; s < (int)Ak.size(); s++) {
		if (not Ak[s].empty()) {
			found = findCycle(s, Ak, cycles) or found;
			if (found and cycles == nullptr) {
				return true;
			}
			Ak[s].clear();
		}
	}

	/*printf("cycles: {");
	for (auto c = cycles.begin(); c != cycles.end(); c++) {
		if (c != cycles.begin()) {
			printf("\n\t");
		}
		printf("{");
		for (auto v = c->begin(); v != c->end(); v++) {
			if (v != c->begin()) {
				printf(" ");
			}
			printf("%d", *v);
		}
		printf("}");
	}
	printf("}\n");*/
	return found;
}

bool Router::breakRoute(int route, set<int> cycleRoutes) {
	// DESIGN(edward.bingham) There are two obvious options to mitigate cycles
	// in the constraint graph by splitting a route. Either way, one pin needs
	// to be shared across the two routes to handle the vertical connection
	// between them.
	// 1. Split the net vertically, putting all nmos pins in one route and all
	//    pmos in the other. We can really pick any pin to be the split point for
	//    this strategy.
	// 2. Split the net horizontally, picking a pin to be the split point and
	//    putting all the left pins in one and right pins in the other. The
	//    split point will be the shared pin.
	//
	// Either way, we should share the pin with the fewest pin constraints
	// as the vertical route. We may also want to check the number of
	// route constraints and distance to other pins in the new net.

	int left = min(this->stack[0].pins[0].offset[0], this->stack[1].pins[0].offset[0]);
	int right = max(this->stack[0].pins.back().offset[0], this->stack[1].pins.back().offset[0]);
	int center = (left + right)/2;

	Wire wp(*tech, routes[route].net);
	Wire wn(*tech, routes[route].net);
	wp.offset = routes[route].offset;
	wn.offset = routes[route].offset;
	vector<int> count(routes[route].pins.size(), 0);
	bool wpHasGate = false;
	bool wnHasGate = false;

	// Move all of the pins that participate in pin constraints associated
	// with the cycle.
	for (auto i = pinConstraints.begin(); i != pinConstraints.end(); i++) {
		vector<Contact>::iterator from = routes[route].pins.end();
		vector<Contact>::iterator to = routes[route].pins.end();
		// A pin cannot have both a pin constraint in and a pin
		// constraint out because a pin is either PMOS or NMOS and pin
		// constraints always go out PMOS pins and in NMOS pins.

		// TODO(edward.bingham) This assumption may be invalidated by adding stack
		// constraints or via constraints because it is specific to pin
		// constraints.

		// First, check if this pin constraint is connected to any of the pins
		// in this route.
		int fromIdx = routes[route].findPin(this, Index(Model::PMOS, i->from));
		if (fromIdx >= 0 and count[fromIdx] < 0) {
			fromIdx = -1;
		}
		// error checking version
		//int toIdx = routes[route].findPin(this, Index(Model::NMOS, i->to));
		//if (fromIdx == -1 and toIdx == -1) {
		//	continue;
		//} else if (fromIdx != -1 and toIdx != -1) {
		//	printf("unitary cycle\n");
		//}

		// optimized version
		int toIdx = -1;
		if (fromIdx == -1) {
			toIdx = routes[route].findPin(this, Index(Model::NMOS, i->to));
			if (toIdx >= 0 and count[toIdx] < 0) {
				toIdx = -1;
			}
			if (toIdx == -1) {
				continue;
			}
		}

		if (fromIdx != -1) {
			from = routes[route].pins.begin()+fromIdx;
			count[fromIdx]++;
		}
		if (toIdx != -1) {
			to = routes[route].pins.begin()+toIdx;
			count[toIdx]++;
		}

		// Second, check if this pin constraint is connected to any of the
		// pins in any of the other routes that were found in the cycle.
		bool found = false;
		for (auto other = cycleRoutes.begin(); not found and other != cycleRoutes.end(); other++) {
			found = ((fromIdx != -1 and routes[*other].hasPin(this, Index(Model::NMOS, i->to))) or
							 (toIdx != -1 and routes[*other].hasPin(this, Index(Model::PMOS, i->from))));
		}
		if (not found) {
			continue;
		}

		// Move the pin to wp or wn depending on hasFrom and hasTo
		if (fromIdx != -1) {
			wp.addPin(this, *from);
			wpHasGate = wpHasGate or this->pin(from->idx).isGate();
			count[fromIdx] = -1;
		} else if (toIdx != -1) {
			wn.addPin(this, *to);
			wnHasGate = wnHasGate or this->pin(to->idx).isGate();
			count[toIdx] = -1;
		}
	}

	if (wp.pins.empty() or wn.pins.empty()) {
		return false;
	}

	//printf("Step 1: w={");
	//for (int i = 0; i < (int)routes[route].pins.size(); i++) {
	//	printf("(%d,%d) ", routes[route].pins[i].type, routes[route].pins[i].pin);
	//}
	//printf("} wp={");
	//for (int i = 0; i < (int)wp.pins.size(); i++) {
	//	printf("(%d,%d) ", wp.pins[i].type, wp.pins[i].pin);
	//}
	//printf("} wn={");
	//for (int i = 0; i < (int)wn.pins.size(); i++) {
	//	printf("(%d,%d) ", wn.pins[i].type, wn.pins[i].pin);
	//}
	//printf("}\n");

	// DESIGN(edward.bingham) Pick one of the remaining pins to be a shared pin.
	// Pick the remaining pin that has the fewest pin constraints, is not a
	// gate, and is furthest toward the outer edge of the cell. Break ties
	// arbitrarily. This will move the vertical route out of the way as much as
	// possible from all of the other constraint problems. If there are no
	// remaining pins, then we need to record wp and wn for routing with A*
	int sharedPin = -1;
	int sharedCount = -1;
	bool sharedIsGate = true;
	int sharedDistanceFromCenter = 0;
	for (int i = 0; i < (int)routes[route].pins.size(); i++) {
		if (count[i] < 0) {
			continue;
		}

		const Pin &pin = this->pin(routes[route].pins[i].idx);
		int distanceFromCenter = abs(pin.offset[0]-center);

		if ((sharedCount < 0 or count[i] < sharedCount)
		  or (count[i] == sharedCount
		    and ((sharedIsGate and pin.isContact())
		      or (sharedIsGate == pin.isGate()
		        and distanceFromCenter > sharedDistanceFromCenter)))) {
			sharedPin = i;
			sharedCount = count[i];
			sharedIsGate = pin.isGate();
			sharedDistanceFromCenter = distanceFromCenter;
		}
	}

	if (sharedPin >= 0) {
		// TODO(edward.bingham) bug in which a non-cycle is being split resulting
		// in redundant vias on various vertical routes
		wp.addPin(this, routes[route].pins[sharedPin]);
		wn.addPin(this, routes[route].pins[sharedPin]);
		count[sharedPin] = -1;
		wpHasGate = wpHasGate or sharedIsGate;
		wnHasGate = wnHasGate or sharedIsGate;
	} else {
		// Add a virtual pin to facilitate dogleg routing where no current pin is useable because of some constraint conflict
		// TODO(edward.bingham) Two things, I need to determine the horizontal
		// position of the pin by looking for available vertical tracks. The
		// problem is that I don't know the ordering of the routes at this point in
		// time, so the only really safe vertical track at the moment is all the
		// way at the end of the cell. I also need to create functionality for
		// saving the vertical position of the pin so the drawing functionality
		// knows where to draw the vertical path.
		Index idx = createVirtualPin(routes[route].net);
		this->stack[2].pins.back().lo = routes[route].offset[Model::PMOS];
		this->stack[2].pins.back().hi = routes[route].offset[Model::PMOS];
		
		// TODO(edward.bingham) draw contacts for virtual pins
		// TODO(edward.bingham) add horizontal constraints for virtual pins
		wp.addPin(this, Contact(*tech, idx));
		wn.addPin(this, Contact(*tech, idx));
	}

	//printf("Step 2: w={");
	//for (int i = 0; i < (int)routes[route].pins.size(); i++) {
	//	printf("(%d,%d) ", routes[route].pins[i].type, routes[route].pins[i].pin);
	//}
	//printf("} wp={");
	//for (int i = 0; i < (int)wp.pins.size(); i++) {
	//	printf("(%d,%d) ", wp.pins[i].type, wp.pins[i].pin);
	//}
	//printf("} wn={");
	//for (int i = 0; i < (int)wn.pins.size(); i++) {
	//	printf("(%d,%d) ", wn.pins[i].type, wn.pins[i].pin);
	//}
	//printf("}\n");

	// DESIGN(edward.bingham) If it is possible to avoid putting a gate pin in
	// one of wp or wn and put all of the PMOS in wp and all of the NMOS in wn,
	// try to do so. This will allow us to route whichever route it is over the
	// transistor stack.
	
	// DESIGN(edward.bingham) prefer routing overtop the pmos stack since it is
	// often wider as a result of the PN ratio.
	if (not wpHasGate) {
		// Put all non-gate PMOS pins into wp and all remaining pins into wn
		for (int i = (int)routes[route].pins.size()-1; i >= 0; i--) {
			if (count[i] < 0) {
				continue;
			}

			auto ct = routes[route].pins.begin()+i;

			bool isGate = this->pin(ct->idx).isContact();
			if (ct->idx.type == Model::PMOS and not isGate) {
				wp.addPin(this, *ct);
			} else {
				wn.addPin(this, *ct);
				wnHasGate = wnHasGate or isGate;
			}
			count[i] = -1;
		}
	}	else if (not wnHasGate) {
		// Put all non-gate NMOS pins into wn and all remaining pins into wp
		for (int i = (int)routes[route].pins.size()-1; i >= 0; i--) {
			if (count[i] < 0) {
				continue;
			}

			auto ct = routes[route].pins.begin()+i;
			bool isGate = this->pin(ct->idx).isContact();
			if (ct->idx.type == Model::NMOS and not isGate) {
				wn.addPin(this, *ct);
			} else {
				wp.addPin(this, *ct);
				wpHasGate = wpHasGate or isGate;
			}
			count[i] = -1;
		}
	} else {
		for (int i = (int)routes[route].pins.size()-1; i >= 0; i--) {
			if (count[i] < 0) {
				continue;
			}

			auto ct = routes[route].pins.begin()+i;
			// Determine horizontal location of wp and wn relative to sharedPin, then
			// place all pins on same side of sharedPin as wp or wn into that wp or wn
			// respectively.
			bool isGate = this->pin(ct->idx).isContact();
			int pos = this->pin(ct->idx).offset[0];
			if (pos >= wn.left and pos >= wp.left and pos <= wn.right and pos <= wp.right) {
				if (ct->idx.type == Model::PMOS) {
					wp.addPin(this, *ct);
					wpHasGate = wpHasGate or isGate;
				} else {
					wn.addPin(this, *ct);
					wnHasGate = wnHasGate or isGate;
				}
			} else if (pos >= wn.left and pos <= wn.right) {
				wn.addPin(this, *ct);
				wnHasGate = wnHasGate or isGate;
			} else if (pos >= wp.left and pos <= wp.right) {
				wp.addPin(this, *ct);
				wpHasGate = wpHasGate or isGate;
			} else if (min(abs(pos-wn.right),abs(pos-wn.left)) < min(abs(pos-wp.right),abs(pos-wp.left))) {
				wn.addPin(this, *ct);
				wnHasGate = wnHasGate or isGate;
			} else {
				wp.addPin(this, *ct);
				wpHasGate = wpHasGate or isGate;
			}
			count[i] = -1;
		}
	}

	//printf("Step 3: w={");
	//for (int i = 0; i < (int)routes[route].pins.size(); i++) {
	//	printf("(%d,%d) ", routes[route].pins[i].type, routes[route].pins[i].pin);
	//}
	//printf("} wp={");
	//for (int i = 0; i < (int)wp.pins.size(); i++) {
	//	printf("(%d,%d) ", wp.pins[i].type, wp.pins[i].pin);
	//}
	//printf("} wn={");
	//for (int i = 0; i < (int)wn.pins.size(); i++) {
	//	printf("(%d,%d) ", wn.pins[i].type, wn.pins[i].pin);
	//}
	//printf("}\n");

	if (not routes[route].layout.layers.empty()) {
		if (routes[route].net >= 0) {
			drawWire(wp.layout, *this, wp);
			drawWire(wn.layout, *this, wn);
		} else {
			drawStack(wp.layout, *ckt, this->stack[flip(wp.net)]);
			drawStack(wn.layout, *ckt, this->stack[flip(wn.net)]);
		}
	}

	wp.buildContacts(this);
	wn.buildContacts(this);

	routes[route] = wp;
	routes.push_back(wn);

	// Update Route Constraints
	if (not routeConstraints.empty()) {
		for (int i = (int)routeConstraints.size()-1; i >= 0; i--) {
			if (routeConstraints[i].wires[0] == route
				or routeConstraints[i].wires[1] == route) {
				routeConstraints.erase(routeConstraints.begin()+i);
			}
		}

		for (int i = 0; i < (int)routes.size()-1; i++) {
			createRouteConstraint(i, (int)routes.size()-1);
			if (i != route) {
				createRouteConstraint(i, route);
			}
		}
	}

	return true;
}

bool Router::breakCycles() {
	bool change = false;
	vector<pair<int, set<int> > > cycles(routes.size(), pair<int, set<int> >(0, set<int>()));
	while (findCycles(&cycles)) {
		// compute pin constraint density for heuristic
		vector<int> numIn(routes.size(), 0);
		vector<int> numOut(routes.size(), 0);
		for (auto i = pinConstraints.begin(); i != pinConstraints.end(); i++) {
			for (int j = 0; j < (int)routes.size(); j++) {
				if (routes[j].hasPin(this, Index(Model::PMOS, i->from))) {
					numOut[j]++;
				}
				if (routes[j].hasPin(this, Index(Model::NMOS, i->to))) {
					numIn[j]++;
				}
			}
		}

		// DESIGN(edward.bingham) We have multiple cycles and a route may
		// participate in more than one. It's unclear whether we want to minimize
		// the number of doglegs or not. Introducing a dogleg requires adding
		// another via, which may make some layouts more difficult, but it also
		// frees up constraints which may make other layouts easier. What we do
		// know is that cycles consist of two types of pins: PMOS and NMOS, and all
		// the PMOS pins are closer to eachother than they are to the NMOS pins and
		// visa versa. We also know that there can be multiple constraint arcs from
		// one node to another or back, and we ultimately need to break all of the
		// cycles represented by those constraint arcs.
		//
		//     a <--- d        a b a b a c b d  .
		//   ^^ |||   ^        | | | | | | | |  .
		//   || vvv   |        v v v v v v v v  .
		//     b ---> c        b a b a b d c a  .
		//
		//
		//  a1 -> a0 <--- d    a b a b a c b d   nodes   .
		//  \\\   ^^      ^    | | | | | | | |           .
		//   vvv //       |  o-o-|-o-|-o | | |     a1    .
		//      b ------> c  |   |   |   | | |    vvv\   .
		//                   | o-o-o-o-o-|-o |     b  \  .
		//                   | |   |   | |   |     v\\|  .
		//                   | |   |   | o-o |     c|||  .
		//                   | |   |   |   | |     v|||  .
		//                   | |   |   | o-|-o     d|||  .
		//                   | |   |   | | |       vvvv  .
		//                   o---o-|-o-|-|-|-o     a0    .
		//                     | | | | | | | |           .
		//                     b a b a b d c a           .
		//
		// In this example, there were no pins in a that we could share between a0
		// and a1 to connect the two nets. Sharing any of the pins would have
		// re-introduced the cycle. In this case, we need to use A* to route the
		// connection from a1 to a0, expanding the width of the cell if necessary.
		//
		// Therefore, we prefer to share pins if we can, but we can only share pins
		// if they themselves don't participate in the cycle.
		map<pair<int, int>, vector<int> > order;
		for (int i = 0; i < (int)cycles.size(); i++) {
			int density = min(numIn[i], numOut[i]);
			if (routes[i].net >= 0) {
				auto pos = order.insert(pair<pair<int, int>, vector<int> >(pair<int, int>(cycles[i].first, density), vector<int>())).first;
				pos->second.push_back(i);
			}
		}

		// attempt to break routes until we're successful.
		while (not order.empty()) {
			auto pos = std::prev(order.end());
			int route = pos->second.back();
			if (breakRoute(route, cycles[route].second)) {
				break;
			}
			pos->second.pop_back();
			if (pos->second.empty()) {
				order.erase(pos);
			}
		}
		if (order.empty()) {
			printf("error: found unbreakable cycle\n");
			// found an unbreakable cycle
			return change;
		}
		change = true;

		for (auto i = cycles.begin(); i != cycles.end(); i++) {
			i->first = 0;
			i->second.clear();
		}
	}
	return change;
}

void Router::findAndBreakViaCycles() {
	/*for (int type = 0; type < 2; type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			this->stack[type].pins[i].viaToPin.clear();
			this->stack[type].pins[i].pinToVia.clear();
		}
	}
	
	// <index into Router::stack[via->type], index into Router::viaConstraints>
	vector<vector<ViaConstraint>::iterator> active;
	for (auto via = viaConstraints.begin(); via != viaConstraints.end(); via++) {
		for (auto s0p = via->side[0].begin(); s0p != via->side[0].end(); s0p++) {
			for (auto s1p = via->side[1].begin(); s1p != via->side[1].end(); s1p++) {
				// Because routes have been broken up at this point in order to
				// fix pin constraint cycles, a pin could participate in
				// multiple routes. We need to check all via relations.
				array<vector<int>, 2> hasSide;
				vector<int> hasMid;
				for (int i = 0; i < (int)routes.size(); i++) {
					if (routes[i].hasPin(this, s0p->idx)) {
						hasSide[0].push_back(i);
					}
					if (routes[i].hasPin(this, s1p->idx)) {
						hasSide[1].push_back(i);
					}
					if (routes[i].hasPin(this, via->idx)) {
						hasMid.push_back(i);
					}
				}

				// Identify potentially violated via constraints.
				//
				// DESIGN(edward.bingham) This could probably be done more
				// intelligently by setting up a graph structure of vias and
				// navigating that to look for violations, but I suspect that
				// the number of vias we need to check across the three vectors
				// will be quite low...  likely just a single via in each list
				// most of the time. Occationally two vias in one of the lists.
				// So just brute forcing the problem shouldn't cause too much of
				// an issue.
				bool found = false;
				for (auto s0 = hasSide[0].begin(); not found and s0 != hasSide[0].end(); s0++) {
					for (auto s1 = hasSide[1].begin(); not found and s1 != hasSide[1].end(); s1++) {
						for (auto m = hasMid.begin(); not found and m != hasMid.end(); m++) {
							found = found or (
								((s0p->idx.type == Model::PMOS and routes[*s0].hasPrev(*m)) or
								 (s0p->idx.type == Model::NMOS and routes[*m].hasPrev(*s0))) and
								((s1p->idx.type == Model::PMOS and routes[*s1].hasPrev(*m)) or
								 (s1p->idx.type == Model::NMOS and routes[*m].hasPrev(*s1)))
							);
						}
					}
				}

				if (found) {
					this->pin(via->idx).addOffset(Pin::PINTOVIA, s0p->idx, s0p->off);
					this->pin(s1p->idx).addOffset(Pin::VIATOPIN, via->idx, s1p->off);
				}
			}
		}
	}*/
}

Index Router::createVirtualPin(int net) {
	Index fromId(2, (int)this->stack[2].pins.size());
	stack[2].pins.push_back(Pin(*tech, net, -1));
	Pin &from = stack[2].pins.back();

	drawPin(from.layout, *ckt, stack[2], fromId.pin);

	// Add the new stack constraints
	Index toId;
	for (toId.type = 0; toId.type < (int)stack.size(); toId.type++) {
		for (toId.pin = 0; toId.pin < (int)stack[toId.type].pins.size(); toId.pin++) {
			if (toId == fromId) {
				continue;
			}
			Pin &to = pin(toId);

			array<int, 2> off={0,0};
			bool fromto = minOffset(&off[0], 0, from.layout, 0, to.layout, 0, Layout::IGNORE, Layout::MERGENET, false);
			bool tofrom = minOffset(&off[1], 0, to.layout, 0, from.layout, 0, Layout::IGNORE, Layout::MERGENET, false);
			if (fromto or tofrom) {
				if (fromId < toId) {
					stackConstraints.push_back(StackConstraint(fromId, toId, off[0], off[1], -1));
				} else if (toId < fromId) {
					stackConstraints.push_back(StackConstraint(toId, fromId, off[1], off[0], -1));
				}
			}
		}
	}
	sort(stackConstraints.begin(), stackConstraints.end());

	// Add the new contact constraints
	for (auto route = routes.begin(); route != routes.end(); route++) {
		int routingMode = Layout::MERGENET;
		if (route->net < 0 or route->net == net) {
			continue;
		}

		for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
			array<int, 2> off={0,0};
			bool pinct = minOffset(&off[0], 0, from.layout, 0, ct->layout, 0, Layout::IGNORE, routingMode);
			bool ctpin = minOffset(&off[1], 0, ct->layout, 0, from.layout, 0, Layout::IGNORE, routingMode);

			if (pinct or ctpin) {
				ct->constraints.push_back(ContactConstraint(fromId, off[0], off[1], -1));
				sort(ct->constraints.begin(), ct->constraints.end());
			}
		}
	}
	return fromId;
}

void Router::alignVirtualPin(Index idx) {
	// TODO(edward.bingham) Find a list of potential ranges for each pin. This is
	// determined by the other pins and their hi and lo values. I also need to
	// think about routes.  Ranges should be defined in terms of pins... but
	// there are two layers of pins where the pin alignments between the two
	// change and so pin ranges don't continue to mean the same thing. So, what
	// if I just define the ranges as an absolute measure? Then if the pin
	// placements change... There is a cyclic dependency

	// TODO(edward.bingham) I need to compare virtual pins against eachother as well
	// blocked intervals: offset -> index into stack constraints
	// left side of interval
	map<int, vector<int> > left;
	// right side of interval
	map<int, vector<int> > right;
	for (auto cnst = stackConstraints.begin(); cnst != stackConstraints.end(); cnst++) {
		if (cnst->pins[0] == idx or cnst->pins[1] == idx) {
			int i = cnst->pins[0] == idx ? 0 : 1;
			int j = 1-i;

			Pin &pi = pin(cnst->pins[i]);
			Pin &pj = pin(cnst->pins[j]);
			if (pi.lo <= pj.hi and pj.lo <= pi.hi) {
				// left constraint is from this to j
				auto lpos = left.insert(pair<int, vector<int> >(pj.offset[0]-cnst->off[i], vector<int>()));
				// right constraint is from j to this
				auto rpos = right.insert(pair<int, vector<int> >(pj.offset[0]+cnst->off[j], vector<int>()));
				lpos.first->second.push_back(cnst-stackConstraints.begin());
				rpos.first->second.push_back(cnst-stackConstraints.begin());
			}
		}
	}

	// merge overlapping intervals
	auto lpos = std::next(left.begin());
	auto rpos = right.begin();
	while (lpos != left.end() and rpos != right.end()) {
		auto plpos = std::prev(lpos);
		auto nrpos = std::next(rpos);
		if (lpos->first <= rpos->first) {
			// these intervals overlap
			plpos->second.insert(plpos->second.end(), lpos->second.begin(), lpos->second.end());
			nrpos->second.insert(nrpos->second.end(), rpos->second.begin(), rpos->second.end());
			lpos = left.erase(lpos);
			rpos = right.erase(rpos);
		} else {
			lpos++;
			rpos++;
		}
	}

	// identify viable ranges for virtual pin placement
	// cost -> [left constraints, right constraints]
	vector<int> lbest;
	vector<int> rbest;
	int best = -1;

	lpos = left.begin();
	rpos = right.end();
	while (true) {
		vector<int> lcnst;
		vector<int> rcnst;
		int lbnd = std::numeric_limits<int>::min();
		int rbnd = std::numeric_limits<int>::max();
		if (rpos != right.end()) {
			lbnd = rpos->first;
			lcnst = rpos->second;
		}
		if (lpos != left.end()) {
			rbnd = lpos->first;
			rcnst = lpos->second;
		}

		int cost = 0;
		for (auto route = routes.begin(); route != routes.end(); route++) {
			if (route->hasPin(this, idx)) {
				if (route->left > rbnd) {
					cost += route->left - rbnd;
				}
				if (route->right < lbnd) {
					cost += lbnd - route->right;
				}
			}
		}

		if (best < 0 or cost < best) {
			lbest = lcnst;
			rbest = rcnst;
			best = cost;
			break;
		}

		if (lpos == left.end()) {
			break;
		} else {
			lpos++;
		}

		if (rpos == right.end()) {
			rpos = right.begin();
		} else {
			rpos++;
		}
	}

	if (best < 0) {
		return;
	}

	vector<Index> from;
	vector<Index> to;
	for (auto i = lbest.begin(); i != lbest.end(); i++) {
		auto cnst = stackConstraints.begin() + *i;
		if (cnst->pins[0] == idx) {
			cnst->select = 1;
		} else if (cnst->pins[1] == idx) {
			cnst->select = 0;
		}
		from.push_back(cnst->pins[cnst->select]);
		to.push_back(cnst->pins[1-cnst->select]);
	}

	for (auto i = rbest.begin(); i != rbest.end(); i++) {
		auto cnst = stackConstraints.begin() + *i;
		if (cnst->pins[0] == idx) {
			cnst->select = 0;
		} else if (cnst->pins[1] == idx) {
			cnst->select = 1;
		}
		from.push_back(cnst->pins[cnst->select]);
		to.push_back(cnst->pins[1-cnst->select]);
	}

	buildPinOffsets(0, from);
	buildPinOffsets(1, to);

	// TODO(edward.bingham) create pin constraints
}

void Router::buildContacts() {
	for (int i = 0; i < (int)routes.size(); i++) {
		routes[i].buildContacts(this);
	}
}

void Router::buildStackConstraints(bool reset) {
	vector<StackConstraint> oldStack;
	std::swap(oldStack, stackConstraints);
	vector<vector<vector<ContactConstraint> > > oldCt;
	oldCt.resize(routes.size());
	for (int i = 0; i < (int)routes.size(); i++) {
		oldCt[i].resize(routes[i].pins.size());
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			std::swap(routes[i].pins[j].constraints, oldCt[i][j]);
		}
	}

	// TODO(edward.bingham) create layout connecting routes, then check spacing
	// from that instead of checking spacing from whole pin. The pin might be
	// quite a bit larger than the connective tissue between the vias across
	// routes.
	
	for (int t0 = 0; t0 < (int)stack.size(); t0++) {
		for (int i0 = 0; i0 < (int)stack[t0].pins.size(); i0++) {
			Pin &from = stack[t0].pins[i0];
			for (int t1 = t0; t1 < (int)stack.size(); t1++) {
				for (int i1 = (t0 == t1 ? i0+1 : 0); i1 < (int)stack[t1].pins.size(); i1++) {
					Pin &to = stack[t1].pins[i1];
					int substrateMode = Layout::IGNORE;
					int routingMode = Layout::MERGENET;
					if (t0 == t1) {
						substrateMode = Layout::DEFAULT;
						routingMode = Layout::DEFAULT;
						if (from.isGate() or to.isGate()) {
							substrateMode = Layout::MERGENET;
						}
					}
					array<int, 2> off={0,0};

					bool fromto = minOffset(&off[0], 0, from.layout, 0, to.layout, 0, substrateMode, routingMode, false);
					bool tofrom = minOffset(&off[1], 0, to.layout, 0, from.layout, 0, substrateMode, routingMode, false);
					if (fromto or tofrom) {
						stackConstraints.push_back(StackConstraint(Index(t0, i0), Index(t1, i1), off[0], off[1], t0 == t1 ? 0 : -1));
					}
				}
			}

			for (auto route = routes.begin(); route != routes.end(); route++) {
				int routingMode = Layout::MERGENET;
				if (route->net < 0 or route->hasPin(this, Index(t0, i0))) {
					continue;
				}

				for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
					array<int, 2> off={0,0};
					bool pinct = minOffset(&off[0], 0, from.layout, 0, ct->layout, 0, Layout::IGNORE, routingMode);
					bool ctpin = minOffset(&off[1], 0, ct->layout, 0, from.layout, 0, Layout::IGNORE, routingMode);

					if (pinct or ctpin) {
						ct->constraints.push_back(ContactConstraint(Index(t0, i0), off[0], off[1], -1));
					}
				}
			}
		}
	}

	if (not reset) {
		// TODO(edward.bingham) Do I need to think about constraints that have been
		// eliminated as a function of the layers?
		int i = (int)stackConstraints.size()-1;
		int j = (int)oldStack.size()-1;
		while (i >= 0 and j >= 0) {
			if (stackConstraints[i] == oldStack[j]) {
				stackConstraints[i].select = oldStack[j].select;
				i--;
				j--;
			} else if (stackConstraints[i] < oldStack[j]) {
				j--;
			} else if (oldStack[j] < stackConstraints[i]) {
				i--;
			}
		}

		for (int i = 0; i < (int)routes.size(); i++) {
			for (int j = 0; j < (int)routes[i].pins.size(); j++) {
				int k = (int)routes[i].pins[j].constraints.size()-1;
				int l = (int)oldCt[i][j].size()-1;
				while (k >= 0 and l >= 0) {
					if (routes[i].pins[j].constraints[k] == oldCt[i][j][l]) {
						routes[i].pins[j].constraints[k].select = oldCt[i][j][l].select;
						k--;
						l--;
					} else if (routes[i].pins[j].constraints[k] < oldCt[i][j][l]) {
						l--;
					} else if (oldCt[i][j][l] < routes[i].pins[j].constraints[k]) {
						k--;
					}
				}
			}
		}
	}
}

// depends on:
// buildStackConstraints() - these constraints determine the
//                           position of the pins
bool Router::buildPinOffsets(int type, vector<Index> start, bool reset) {
	bool change = false;
	if (reset) {
		for (int t0 = 0; t0 < (int)stack.size(); t0++) {
			for (auto pin = stack[t0].pins.begin(); pin != stack[t0].pins.end(); pin++) {
				pin->offset[type] = 0;
				pin->bound[type] = numeric_limits<int>::max();
			}
		}

		for (auto route = routes.begin(); route != routes.end(); route++) {
			for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
				ct->offset[type] = numeric_limits<int>::min();
				ct->bound[type] = numeric_limits<int>::max();
			}
		}
	} else {
		for (int t0 = 0; t0 < (int)stack.size(); t0++) {
			for (auto pin = stack[t0].pins.begin(); pin != stack[t0].pins.end(); pin++) {
				pin->bound[type] = numeric_limits<int>::max();
			}
		}

		for (auto route = routes.begin(); route != routes.end(); route++) {
			for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
				ct->bound[type] = numeric_limits<int>::max();
			}
		}
	}

	vector<vector<Index> > tokens;
	sort(start.begin(), start.end());
	start.erase(unique(start.begin(), start.end()), start.end());
	for (auto i = start.begin(); i != start.end(); i++) {
		tokens.push_back(vector<Index>(1, *i));
	}
	if (tokens.empty()) {
		for (int i = 0; i < (int)stack.size(); i++) {
			for (int j = 0; j < (int)stack[i].pins.size(); j++) {
				tokens.push_back(vector<Index>(1, Index(i,j)));
			}
		}
	}
	while (not tokens.empty()) {
		vector<Index> currIdx = tokens.back();
		tokens.pop_back();

		/*printf("curr {");
		for (int i = 0; i < (int)currIdx.size(); i++) {
			printf("(%d %d) ", currIdx[i].type, currIdx[i].pin);
		}
		printf("}\n");*/

		Pin &curr = pin(currIdx.back());

		// Loop through stack constraints
		for (auto cnst = stackConstraints.begin(); cnst != stackConstraints.end(); cnst++) {
			if (cnst->select < 0) {
				continue;
			}

			int select = type == 0 ? cnst->select : 1-cnst->select;
			if (cnst->pins[select] == currIdx.back()) {
				Index nextIdx = cnst->pins[1-select];
				auto pos = find(currIdx.begin(), currIdx.end(), nextIdx);
				if (pos == currIdx.end()) {
					Pin &next = pin(nextIdx);

					int weight = curr.offset[type] + cnst->off[cnst->select];
					if (next.offset[type] < weight) {
						next.offset[type] = weight;
						bool found = false;
						for (auto tst = tokens.begin(); tst != tokens.end() and not found; tst++) {
							found = (tst->back() == nextIdx);
						}
						if (not found) {
							tokens.push_back(currIdx);
							tokens.back().push_back(nextIdx);
						}
						change = true;
					}
				} else {
					unresolvedPinCycle[type] = true;
					//if (debug) {
						printf("error: buildPinOffsets found cycle {");
						for (int j = 0; j < (int)currIdx.size(); j++) {
							printf("(%d %d) ", currIdx[j].type, currIdx[j].pin);
						}
						printf("(%d %d)}\n", nextIdx.type, nextIdx.pin);
						printf("stack constraint (%d %d) %s (%d %d)\n", cnst->pins[0].type, cnst->pins[0].pin, (cnst->select == 0 ? "->" : "<-"), cnst->pins[1].type, cnst->pins[1].pin);
					//}
				}
			}
		}

		// Loop through via constraints
		for (auto route = routes.begin(); route != routes.end(); route++) {
			for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
				bool fromContact = false;
				for (auto cnst = ct->constraints.begin(); cnst != ct->constraints.end(); cnst++) {
					if (cnst->select == type and cnst->pin == currIdx.back()) {
						int weight = curr.offset[type] + cnst->off[cnst->select];
						if (ct->offset[type] < weight) {
							ct->offset[type] = weight;
							change = true;
						}
						fromContact = true;
					}
				}
				if (not fromContact) {
					continue;
				}

				for (auto cnst = ct->constraints.begin(); cnst != ct->constraints.end(); cnst++) {
					if (cnst->select == 1-type) {
						auto pos = find(currIdx.begin(), currIdx.end(), cnst->pin);
						if (pos == currIdx.end()) {
							Pin &next = pin(cnst->pin);
							int weight = ct->offset[type] + cnst->off[cnst->select];
							if (next.offset[type] < weight) {
								next.offset[type] = weight;
								bool found = false;
								for (auto tst = tokens.begin(); tst != tokens.end() and not found; tst++) {
									found = (tst->back() == cnst->pin);
								}
								if (not found) {
									tokens.push_back(currIdx);
									tokens.back().push_back(cnst->pin);
								}
								change = true;
							}
						} else {
							unresolvedPinCycle[type] = true;
							//if (debug) {
								printf("error: buildPinOffsets found cycle {");
								for (int j = 0; j < (int)currIdx.size(); j++) {
									printf("(%d %d) ", currIdx[j].type, currIdx[j].pin);
								}
								printf("(%d %d)}\n", cnst->pin.type, cnst->pin.pin);
								printf("contact constraint (%d %d) %s (%d %d)\n", cnst->pin.type, cnst->pin.pin, (cnst->select == 0 ? "->" : "<-"), ct->idx.type, ct->idx.pin);
							//}
						}
					}
				}
			}
		}
	}

	for (auto cnst = stackConstraints.begin(); cnst != stackConstraints.end(); cnst++) {
		if (cnst->select >= 0) {
			int select = type == 0 ? cnst->select : 1-cnst->select;
			Pin &from = pin(cnst->pins[select]);
			Pin &to = pin(cnst->pins[1-select]);

			int weight = to.offset[type] - cnst->off[cnst->select];
			if (weight < from.bound[type]) {
				from.bound[type] = weight;
			}
		}
	}

	for (auto route = routes.begin(); route != routes.end(); route++) {
		for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
			for (auto cnst = ct->constraints.begin(); cnst != ct->constraints.end(); cnst++) {
				if (cnst->select == type) {
					Pin &from = pin(cnst->pin);
					int weight = ct->offset[type] - cnst->off[cnst->select];
					if (weight < from.bound[type]) {
						from.bound[type] = weight;
					}
				} else if (cnst->select == 1-type) {
					Pin &to = pin(cnst->pin);
					int weight = to.offset[type] - cnst->off[cnst->select];
					if (weight < ct->bound[type]) {
						ct->bound[type] = weight;
					}
				}
			}
		}
	}

	for (auto route = routes.begin(); route != routes.end(); route++) {
		if (route->net >= 0) {
			route->resortPins(this);
		}
	}
	return change;
}

// depends on:
// buildPinOffsets() - contacts on the route need an up-to-date
//                  position before we draw them
void Router::drawRoutes() {
	for (int i = 0; i < (int)routes.size(); i++) {
		routes[i].layout.clear();
	}

	// Draw the routes
	for (int i = 0; i < (int)routes.size(); i++) {
		if (routes[i].net >= 0) {
			drawWire(routes[i].layout, *this, routes[i]);
		} else {
			drawStack(routes[i].layout, *ckt, this->stack[flip(routes[i].net)]);
		}
	}
}

void Router::createRouteConstraint(int i, int j) {
	if (i > j) {
		swap(i, j);
	}

	RouteConstraint result(i, j);

	//printf("checkout route %d:%d and %d:%d\n", i, routes[i].net, j, routes[j].net);
	int routingMode = (
		(routes[i].net < 0 and routes[j].net >= 0) or
		(routes[i].net >= 0 and routes[j].net < 0)
	) ? Layout::MERGENET : Layout::DEFAULT;
	
	bool fromto = minOffset(&result.off[0], 1, routes[i].layout, 0, routes[j].layout, 0, Layout::DEFAULT, routingMode);
	bool tofrom = minOffset(&result.off[1], 1, routes[j].layout, 0, routes[i].layout, 0, Layout::DEFAULT, routingMode);

	if ((allowOverCell or (routes[i].net >= 0 and routes[j].net >= 0)) and not fromto and not tofrom) {
		return;
	}

	int select = -1;
	array<vector<bool>, 2> hasType = {routes[i].pinTypes(), routes[j].pinTypes()};
	if ((not allowOverCell and (routes[i].net < 0 or routes[j].net < 0))
		or ((fromto or tofrom)
			and ((routes[i].net < 0 and routes[j].net < 0)
				or (routes[i].net < 0 and hasType[1][1-flip(routes[i].net)])
				or (routes[j].net < 0 and hasType[0][1-flip(routes[j].net)])
			)
		))  {
		select = (flip(routes[j].net) == Model::PMOS or flip(routes[i].net) == Model::NMOS);
	}

	auto pos = lower_bound(routeConstraints.begin(), routeConstraints.end(), result);
	int idx = pos - routeConstraints.begin();
	if (pos == routeConstraints.end() or !(*pos == result)) {
		routeConstraints.insert(pos, result);
	} else {
		pos->off[0] = max(pos->off[0], result.off[0]);
		pos->off[1] = max(pos->off[1], result.off[1]);
	}

	if (routeConstraints[idx].select < 0 and select >= 0) {
		routeConstraints[idx].select = select;
	}
}

bool Router::buildRouteConstraints(bool resetSpacing, bool resetOrder) {
	// Compute route constraints
	bool change = false;
	vector<RouteConstraint> old;
	old.swap(routeConstraints);
	for (int i = 0; i < (int)routes.size(); i++) {
		for (int j = i+1; j < (int)routes.size(); j++) {
			createRouteConstraint(i, j);
		}
	}

	if (not resetOrder or not resetSpacing) {
		vector<set<int> > Ak = createAdjacencyList();

		/*printf("buildRouteConstraints\n");
		for (int i = 0; i < (int)Ak.size(); i++) {
			printf("%d:{", i);
			for (auto w = Ak[i].begin(); w != Ak[i].end(); w++) {
				printf("%d ", *w);
			}
			printf("}\n");
		}*/

		int i = 0, j = 0;
		while (i < (int)routeConstraints.size() and j < (int)old.size()) {
			auto c = routeConstraints.begin()+i;

			if (c->wires[0] == old[j].wires[0]
				and c->wires[1] == old[j].wires[1]) {
				if (c->select == -1 and old[j].select >= 0 and not resetOrder) {
					int from = c->wires[old[j].select];
					int to = c->wires[1-old[j].select];
					auto pos = Ak[from].insert(to);
					if (pos.second and not findCycle(from, Ak)) {
						pos.second = false;
					}
					if (not pos.second) {
						c->select = old[j].select;
					} else {
						Ak[from].erase(pos.first);
					}
				}
				if (c->off[0] < old[j].off[0] and not resetSpacing) {
					c->off[0] = old[j].off[0];
				}
				if (c->off[1] < old[j].off[1] and not resetSpacing) {
					c->off[1] = old[j].off[1];
				}
				change = change or (c->select != old[j].select or c->off[0] > old[j].off[0] or c->off[1] > old[j].off[1]);
				i++;
				j++;
			} else if (c->wires[0] == old[j].wires[1]
				and c->wires[1] == old[j].wires[0]) {
				if (c->select == -1 and old[j].select >= 0 and not resetOrder) {
					int from = c->wires[1-old[j].select];
					int to = c->wires[old[j].select];
					auto pos = Ak[from].insert(to);
					if (pos.second and not findCycle(from, Ak)) {
						pos.second = false;
					}
					if (not pos.second) {
						c->select = 1-old[j].select;
					} else {
						Ak[from].erase(pos.first);
					}
				}
				if (c->off[0] < old[j].off[1] and not resetSpacing) {
					c->off[0] = old[j].off[1];
				}
				if (c->off[1] < old[j].off[0] and not resetSpacing) {
					c->off[1] = old[j].off[0];
				}
				change = change or (c->select != 1-old[j].select or c->off[0] > old[j].off[1] or c->off[1] > old[j].off[0]);
				i++;
				j++;
			} else if (c->wires[0] < old[j].wires[0]
				or (c->wires[0] == old[j].wires[0]
					and c->wires[1] < old[j].wires[1])) {
				change = true;
				i++;
			} else {
				if (not resetSpacing) {
					if (old[j].select < 0) {
						routeConstraints.insert(c, old[j]);
						i++;
					} else {
						int from = old[j].wires[1-old[j].select];
						int to = old[j].wires[old[j].select];
						auto pos = Ak[from].insert(to);
						if (pos.second and not findCycle(from, Ak)) {
							pos.second = false;
						}
						if (not pos.second) {
							routeConstraints.insert(c, old[j]);
							i++;
						} else {
							Ak[from].erase(pos.first);
							change = true;
						}
					}
				} else {
					change = true;
				}
				j++;
			}
		}
	}
	return change;
}

void Router::buildGroupConstraints() {
	groupConstraints.clear();

	for (int i = 0; i < (int)routes.size(); i++) {
		for (int type = 0; type < (int)this->stack.size(); type++) {
			for (int j = 0; j < (int)this->stack[type].pins.size(); j++) {
				int off = 0;
				if (minOffset(&off, 1, routes[i].layout, 0,
				  stack[type].pins[j].layout, stack[type].pins[j].offset[0],
					Layout::IGNORE, Layout::MERGENET)) {
					groupConstraints.push_back(RouteGroupConstraint(i, Index(type, j)));
				}
			}
		}
	}
}

set<int> Router::propagateRouteConstraint(int idx) {
	set<int> result;
	if (routeConstraints[idx].select < 0) {
		return result;
	}

	// DESIGN(edward.bingham) If a route constraint implies some ordering that
	// involves a route that participates in a group constraint, then it also
	// implies the same ordering with all other routes that participate in that
	// group constraint.
	//
	//  ----- ^ ^
	//        | |
	//  --O-- | v route constraint
	//    |  <-   group constraint
	//  --O--
	for (int i = 0; i < (int)groupConstraints.size(); i++) {
		int from[2] = {-1,-1};
		if (groupConstraints[i].wire == routeConstraints[idx].wires[0]) {
			from[0] = 0;
		} else if (groupConstraints[i].wire == routeConstraints[idx].wires[1]) {
			from[0] = 1;
		}

		if (from[0] >= 0 and routes[routeConstraints[idx].wires[1-from[0]]].hasPin(this, groupConstraints[i].pin)) {
			for (int j = 0; j < (int)routeConstraints.size(); j++) {
				if (j == idx or routeConstraints[j].select >= 0) {
					continue;
				}

				from[1] = -1;
				if (routeConstraints[j].wires[0] == groupConstraints[i].wire) {
					from[1] = 0;
				} else if (routeConstraints[j].wires[1] == groupConstraints[i].wire) {
					from[1] = 1;
				}

				if (from[1] >= 0 and routes[routeConstraints[j].wires[1-from[1]]].hasPin(this, groupConstraints[i].pin)) {
					routeConstraints[j].select = (from[0] == from[1] ? routeConstraints[idx].select : 1-routeConstraints[idx].select);
					result.insert(j);
				}
			}
		}
	}
	return result;
}

void Router::zeroWeights() {
	cellHeight = 0;
	for (int i = 0; i < (int)routes.size(); i++) {
		routes[i].offset[Model::PMOS] = 0;
		routes[i].offset[Model::NMOS] = 0;
	}
}

bool Router::buildPinBounds(bool reset) {
	bool change = false;
	if (reset) {
		for (int type = 0; type < (int)this->stack.size(); type++) {
			for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
				this->stack[type].pins[i].lo = numeric_limits<int>::max();
				this->stack[type].pins[i].hi = numeric_limits<int>::min();
			}
		}
	}

	for (int i = 0; i < (int)routes.size(); i++) {
		int lo = routes[i].offset[Model::PMOS];

		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			Pin &pin = this->pin(routes[i].pins[j].idx);
			int hi = lo + (routes[i].net < 0 ? pin.height : tech->getWidth(tech->at(pin.layer).draw));
			if (lo < pin.lo) {
				pin.lo = lo;
				change = true;
			}
			if (hi > pin.hi) {
				pin.hi = hi;
				change = true;
			}
		}
	}
	return change;
}

bool Router::buildRouteOffsets(int type, vector<int> start) {
	bool change = false;
	unresolvedCycle[type] = false;
	if (start.empty()) {
		vector<bitset> prev = routeOrderMap(type);
		for (int i = 0; i < (int)prev.size(); i++) {
			if (prev[i].empty()) {
				start.push_back(i);
			}
		}
	}

	// TODO(edward.bingham) for routes that are on the wrong side of the
	// PMOS stack, this fails to update their poffset.

	vector<vector<int> > tokens;
	sort(start.begin(), start.end());
	start.erase(unique(start.begin(), start.end()), start.end());
	for (auto i = start.begin(); i != start.end(); i++) {
		tokens.push_back(vector<int>(1, *i));
	}
	while (not tokens.empty()) {
		vector<int> curr = tokens.back();
		tokens.pop_back();

		map<int, int> n = type == Model::PMOS ? next(curr.back()) : prev(curr.back());
		for (auto i = n.begin(); i != n.end(); i++) {
			auto pos = find(curr.begin(), curr.end(), i->first);
			if (pos == curr.end()) {
				int weight = routes[curr.back()].offset[type] + i->second;
				if (routes[i->first].offset[type] < weight) {
					change = true;
					routes[i->first].offset[type] = weight;

					tokens.push_back(curr);
					tokens.back().push_back(i->first);
				}
			} else {
				unresolvedCycle[type] = true;
				if (debug) {
					printf("error: buildRouteOffsets found cycle {");
					for (int j = 0; j < (int)curr.size(); j++) {
						printf("%d:%s(%d) ", curr[j], curr[j] >= 0 ? ckt->nets[routes[curr[j]].net].name.c_str() : "", routes[curr[j]].net);
					}
					printf("%d:%s(%d)}\n", i->first, i->first >= 0 ? ckt->nets[routes[i->first].net].name.c_str() : "", routes[i->first].net);
				}
			}
		}
	}
	
	buildPinBounds(true);
	return change;
}

void Router::resetGraph() {
	zeroWeights();
	buildRouteOffsets(Model::PMOS);
	buildRouteOffsets(Model::NMOS);
}

void Router::alignPins() {
	array<int, 3> offset = {0, (int)stack[0].pins.size(),
		(int)stack[0].pins.size()+(int)stack[1].pins.size()};

	int rightOfCell = std::numeric_limits<int>::min();
	for (int i = 0; i < (int)stack.size(); i++) {
		if (not stack[i].pins.empty() and rightOfCell < stack[i].pins.back().offset[0]) {
			rightOfCell = stack[i].pins.back().offset[0];
		}
	}
	vector<bitset> prev = pinOrderMap();
	
	/*array<map<int, vector<int> >, 3> index;
	for (int = 0; i < (int)stackConstraints.size(); i++) {
		auto pos = index[stackConstraints[i].pins[0].type].insert(pair<int, vector<int> >(stackConstraints[i].pins[0].pin, vector<int>()));
		pos.first->second.push_back(i);
		pos = index[stackConstraints[i].pins[1].type].insert(pair<int, vector<int> >(stackConstraints[i].pins[1].pin, vector<int>()));
		pos.first->second.push_back(i);
	}

	// Align pins (prefer gates first)
	while (true) {
		array<Index, 2> mos = {Index(0,-1),Index(1,-1)};
		int score = -1;
		for (int i = 0; i < (int)stack[Model::PMOS].pins.size(); i++) {
			Pin &pini = stack[Model::PMOS].pins[i];
			for (int j = 0; j < (int)stack[Model::NMOS].pins.size(); j++) {
				Pin &pinj = stack[Model::NMOS].pins[j];
				if (pini.outNet != pinj.outNet) {
					continue;
				}

				int li = pini.offset[0];
				int ri = std::numeric_limits<int>::max();
				if (pini.offset[1] != std::numeric_limits<int>::min()) {
					ri = rightOfCell-pini.offset[1];
				}
				int lj = pinj.offset[0];
				int rj = std::numeric_limits<int>::max();
				if (pinj.offset[1] != std::numeric_limits<int>::min()) {
					rj = rightOfCell-pinj.offset[1];
				}

				int cost = max(li-rj, lj-ri);
				if (mos[0].pin < 0 or cost < score) {
					mos[Model::PMOS].pin = i;
					mos[Model::NMOS].pin = j;
				}
			}
		}

		if (mos[0].pin < 0 or score > 0) {
			break;
		}

		set<int> todo;
		vector<Index> from;
		vector<Index> to;
		auto pcon = index[Model::PMOS].find(mos[Model::PMOS]);
		auto ncon = index[Model::NMOS].find(mos[Model::NMOS]);
		if (pcon == index[Model::PMOS].end() or ncon == index[Model::NMOS].end()) {
			break;
		}

		for 

		for (auto i = stackConstraints.begin(); i != stackConstraints.end(); i++) {
			for (int type = 0; type < 2; type++) {
				for (int k = 0; k < 2; k++) {
					if (i->select >= 0 and i->pins[k] == mos[type]) {
						for (auto j = stackConstraints.begin(); j != stackConstraints.end(); j++) {
							if (j->pins[k] == mos[1-type] and j->pins[1-k] == i->pins[1-k]) {
								if (j->select < 0) {
									j->select = i->select;

									from.push_back(j->pins[j->select]);
									to.push_back(j->pins[1-j->select]);
									int aIdx = offset[j->pins[j->select].type]+j->pins[j->select].pin;
									int bIdx = offset[j->pins[1-j->select].type]+j->pins[1-j->select].pin;
									prev[bIdx].set(aIdx, true);
									prev[bIdx] |= prev[aIdx];
									todo.insert(bIdx);
								}
								break;
							} else if (j->pins[k] == i->pins[1-k] and j->pins[1-k] == mos[1-type]) {
								if (j->select < 0) {
									j->select = 1-i->select;

									from.push_back(j->pins[j->select]);
									to.push_back(j->pins[1-j->select]);
									int aIdx = offset[j->pins[j->select].type]+j->pins[j->select].pin;
									int bIdx = offset[j->pins[1-j->select].type]+j->pins[1-j->select].pin;
									prev[bIdx].set(aIdx, true);
									prev[bIdx] |= prev[aIdx];
									todo.insert(bIdx);
								}
								break;
							}
						}
						break;
					} 
				}
			}
		}

		buildPinOffsets(0, from);
		buildPinOffsets(1, to);
		propagateOrderMap(prev, todo);
		
		rightOfCell = std::numeric_limits<int>::min();
		for (int i = 0; i < (int)stack.size(); i++) {
			if (not stack[i].pins.empty() and rightOfCell < stack[i].pins.back().offset[0]) {
				rightOfCell = stack[i].pins.back().offset[0];
			}
		}
	}*/


	// Interleave routes
	while (true) {
		if (debug) print();
		int score = -1;
		map<int, int> best;

		array<int, 2> x = {0,0};

		for (auto r0 = routes.begin(); r0 != routes.end(); r0++) {
			vector<bool> types0 = r0->pinTypes();
			if (r0->net < 0 or not types0[Model::PMOS] or not types0[Model::NMOS]) {
				continue;
			}

			for (auto r1 = routes.begin(); r1 != routes.end(); r1++) {
				vector<bool> types1 = r1->pinTypes();
				if (r1->net < 0 or not types1[Model::PMOS] or not types1[Model::NMOS]) {
					continue;
				}
				// do these routes overlap?

				vector<bitset> test = prev;

				int total = 0;
				map<int, int> assignments;
				bool error = false;
				for (auto i = r0->pins.begin(); i != r0->pins.end() and not error; i++) {
					Pin &pini = pin(i->idx);
					for (auto j = r1->pins.begin(); j != r1->pins.end() and not error; j++) {
						int off = 0;
						Pin &pinj = pin(j->idx);

						// TODO(edward.bingham) I'll need to update the pin positions as I go...
						if (i->idx.type != j->idx.type and pini.outNet != pinj.outNet and 
							minOffset(&off, 1, pini.layout, pini.offset[0],
																 pinj.layout, pinj.offset[0],
											Layout::IGNORE, Layout::MERGENET)) {
							// these two pins conflict with eachother
							auto cnst = stackConstraints.end();
							for (auto k = stackConstraints.begin(); k != stackConstraints.end(); k++) {
								if ((k->pins[0] == i->idx and k->pins[1] == j->idx)
									or (k->pins[0] == j->idx and k->pins[1] == i->idx)) {
									cnst = k;
									break;
								}
							}

							if (cnst->select >= 0) {
								// these pins have already been deconflicted
								continue;
							}
							
							int minCost = std::numeric_limits<int>::max();
							int select = -1;
							for (int s = 0; s < 2; s++) {
								Index from = cnst->pins[s];
								Index to = cnst->pins[1-s];

								int fromIdx = offset[from.type]+from.pin;
								int toIdx = offset[to.type]+to.pin;
								if (test[fromIdx].get(toIdx)) {
									// can we always overlap these pins without introducing a cycle?
									continue;
								}
								error = false;

								Pin &a = pin(from);
								Pin &b = pin(to);

								int amin = a.offset[0];
								int bmax = std::numeric_limits<int>::max();
								if (b.offset[1] != std::numeric_limits<int>::min()) {
									bmax = rightOfCell - b.offset[1];
								}

								// what is the cost to overlap these two pins?
								int cost = amin - bmax + cnst->off[s];
								if (cost < minCost) {
									minCost = cost;
									select = s;
								}
							}

							if (select < 0) {
								error = true;
							} else {
								total += max(0, minCost);
								assignments.insert(pair<int, int>(cnst-stackConstraints.begin(), select));
							}
						}
					}
				}

				// What is the size cost to interleave these routes?
				// is it better than the current best option?
				if (not error and not assignments.empty() and (best.empty() or total < score)) {
					score = total;
					best = assignments;
					x[0] = r0-routes.begin();
					x[1] = r1-routes.begin();
				}
			}
		}

		if (best.empty() or score > 0) {
			break;
		}

		set<int> todo;
		vector<Index> from;
		vector<Index> to;
		if (debug) printf("Interleaving Routes %d and %d\n", x[0], x[1]);
		for (auto m = best.begin(); m != best.end(); m++) {
			auto cnst = stackConstraints.begin()+m->first;
			cnst->select = m->second;
			if (debug) printf("\tsetting (%d %d) -> (%d %d)\n", cnst->pins[cnst->select].type, cnst->pins[cnst->select].pin, cnst->pins[1-cnst->select].type, cnst->pins[1-cnst->select].pin);

			from.push_back(cnst->pins[cnst->select]);
			to.push_back(cnst->pins[1-cnst->select]);
			int aIdx = offset[cnst->pins[cnst->select].type]+cnst->pins[cnst->select].pin;
			int bIdx = offset[cnst->pins[1-cnst->select].type]+cnst->pins[1-cnst->select].pin;
			prev[bIdx].set(aIdx, true);
			prev[bIdx] |= prev[aIdx];
			todo.insert(bIdx);
		}

		buildPinOffsets(0, from);
		buildPinOffsets(1, to);
		propagateOrderMap(prev, todo);

		rightOfCell = std::numeric_limits<int>::min();
		for (int i = 0; i < (int)stack.size(); i++) {
			if (not stack[i].pins.empty() and rightOfCell < stack[i].pins.back().offset[0]) {
				rightOfCell = stack[i].pins.back().offset[0];
			}
		}
	}
	if (debug) print();

	// Order remaining pins
	if (debug) printf("Checking remaining constraints\n");
	while (true) {
		auto cnst = stackConstraints.end();
		int select = -1;
		int best = 0;
		
		for (auto i = stackConstraints.begin(); i != stackConstraints.end(); i++) {
			if (i->select >= 0) {
				continue;
			}

			for (int s = 0; s < 2; s++) {
				int aIdx = offset[i->pins[s].type]+i->pins[s].pin;
				int bIdx = offset[i->pins[1-s].type]+i->pins[1-s].pin;
				if (prev[aIdx].get(bIdx)) {
					continue;
				}
				const Pin &a = pin(i->pins[s]);
				const Pin &b = pin(i->pins[1-s]);

				int amin = a.offset[0];
				int bmax = std::numeric_limits<int>::max();
				if (b.offset[1] != std::numeric_limits<int>::min()) {
					bmax = rightOfCell - b.offset[1];
				}

				int cost = amin-bmax + i->off[s];
				if (cost > 0) {
					continue;
				}

				int saved = 0;
				vector<int> hasA;
				vector<int> hasB;
				for (int j = 0; j < (int)routes.size(); j++) {
					if (not routes[j].pins.empty() and routes[j].hasGate(this)) {
						if (routes[j].pins.back().idx == i->pins[s]) {
							hasA.push_back(j);
						}
						if (routes[j].pins[0].idx == i->pins[1-s]) {
							hasB.push_back(j);
						}
					}
				}
				for (int j = 0; j < (int)hasA.size(); j++) {
					for (int k = 0; k < (int)hasB.size(); k++) {
						saved += (hasA[j] != hasB[k]);
					}
				}
				if (saved == 0) {
					continue;
				}

				if (saved > best) {
					best = saved;
					select = s;
					cnst = i;
				}
			}
		}

		if (best <= 0) {
			break;
		}

		if (debug) printf("setting (%d %d) -> (%d %d)\n", cnst->pins[select].type, cnst->pins[select].pin, cnst->pins[1-select].type, cnst->pins[1-select].pin);
		cnst->select = select;
		buildPinOffsets(0, vector<Index>(1, cnst->pins[select]));
		buildPinOffsets(1, vector<Index>(1, cnst->pins[1-select]));

		int aIdx = offset[cnst->pins[select].type]+cnst->pins[select].pin;
		int bIdx = offset[cnst->pins[1-select].type]+cnst->pins[1-select].pin;
		prev[bIdx].set(aIdx, true);
		prev[bIdx] |= prev[aIdx];
		propagateOrderMap(prev, {bIdx});

		rightOfCell = std::numeric_limits<int>::min();
		for (int i = 0; i < (int)stack.size(); i++) {
			if (not stack[i].pins.empty() and rightOfCell < stack[i].pins.back().offset[0]) {
				rightOfCell = stack[i].pins.back().offset[0];
			}
		}
	}
}

bool Router::assignStackConstraints() {
	bool change = false;
	for (auto cnst = stackConstraints.begin(); cnst != stackConstraints.end(); cnst++) {
		if (cnst->select < 0) {
			Pin &a = pin(cnst->pins[0]);
			Pin &b = pin(cnst->pins[1]);
			if (b.lo <= a.hi and a.lo <= b.hi) {
				// TODO(edward.bingham) if the acceptable ranges overlap, then pick the one that damages the cell width the least
				if (a.offset[0] < b.offset[0]) {
					cnst->select = 0;
					change = true;
				} else if (b.offset[0] < a.offset[0]) {
					cnst->select = 1;
					change = true;
				}
			}
		}
	}

	for (auto route = routes.begin(); route != routes.end(); route++) {
		for (auto ct = route->pins.begin(); ct != route->pins.end(); ct++) {
			Pin &b = pin(ct->idx);
			for (auto cnst = ct->constraints.begin(); cnst != ct->constraints.end(); cnst++) {
				if (cnst->select < 0) {
					Pin &a = pin(cnst->pin);

					if (a.lo <= route->offset[Model::PMOS] and route->offset[Model::PMOS] <= a.hi) {
						if (a.offset[0] < b.offset[0]) {
							cnst->select = 0;
							change = true;
						} else if (b.offset[0] < a.offset[0]) {
							cnst->select = 1;
							change = true;
						}
					}
				}
			}
		}
	}
	return change;
}

bool Router::assignRouteConstraints(bool reset) {
	bool change = false;
	if (reset) {
		resetGraph();
	}

	vector<int> inTokens, outTokens;
	for (int i = 0; i < (int)routeConstraints.size(); i++) {
		if (routeConstraints[i].select >= 0) {
			set<int> prop = propagateRouteConstraint(i);
			change = change or not prop.empty();
			for (auto j = prop.begin(); j != prop.end(); j++) {
				int from = routeConstraints[*j].select;
				inTokens.push_back(routeConstraints[*j].wires[from]);
				outTokens.push_back(routeConstraints[*j].wires[1-from]);
			}
		}
	}
	if (inTokens.size() + outTokens.size() > 0) {
		change = buildRouteOffsets(Model::PMOS, inTokens) or change;
		change = buildRouteOffsets(Model::NMOS, outTokens) or change;
	}

	vector<int> unassigned;
	unassigned.reserve(routeConstraints.size());
	for (int i = 0; i < (int)routeConstraints.size(); i++) {
		if (routeConstraints[i].select < 0) {
			unassigned.push_back(i);
		}
	}

	vector<bitset> prev = routeOrderMap(Model::PMOS);

	while (unassigned.size() > 0) {
		// handle critical constraints, that would create cycles if assigned the wrong direction.
		inTokens.clear();
		outTokens.clear();
		for (int u = (int)unassigned.size()-1; u >= 0; u--) {
			if (routeConstraints[unassigned[u]].select >= 0) {
				unassigned.erase(unassigned.begin()+u);
				continue;
			}
			// TODO(edward.bingham) check via constraints. If this is a critical
			// constraint that participates in a via constraint, and as a results
			// both directions create a cycle, then we need to expand the via
			// constraint. If this is not a critical constraint, but it participates
			// in a via constraint, and setting a particular direction would create a
			// cycle as a result of the via constraint, then either we need to set
			// the other direction or we need to expand the via constraint. The final
			// way we could deal with a via constraint cycle is to break up the route
			// to allow a horizontal path

			int i = unassigned[u];
			int a = routeConstraints[i].wires[0];
			int b = routeConstraints[i].wires[1];

			if (a >= 0 and prev[a].get(b)) {
				change = true;
				routeConstraints[i].select = 1;
				inTokens.push_back(b);
				outTokens.push_back(a);
				unassigned.erase(unassigned.begin()+u);
			} else if (b >= 0 and prev[b].get(a)) {
				change = true;
				routeConstraints[i].select = 0;
				inTokens.push_back(a);
				outTokens.push_back(b);
				unassigned.erase(unassigned.begin()+u);
			}

			// Propagate order decision through the group constraints
			if (routeConstraints[i].select >= 0) {
				set<int> prop = propagateRouteConstraint(i);
				change = change or not prop.empty();
				for (auto j = prop.begin(); j != prop.end(); j++) {
					int from = routeConstraints[*j].select;
					inTokens.push_back(routeConstraints[*j].wires[from]);
					outTokens.push_back(routeConstraints[*j].wires[1-from]);
					//unassigned.erase(remove(unassigned.begin(), unassigned.end(), *j), unassigned.end());
				}
			}

			// TODO(edward.bingham) Did doing this create a cycle when we include
			// violated via constraints? If so, we resolve that cycle by pushing the
			// associated pins out as much as needed.

			// TODO(edward.bingham) In more advanced nodes, pushing the pin out may
			// not be possible because poly routes are only allowed on a regular
			// grid. So if we pushed the poly at all, we'd have to push it an entire
			// grid unit. We may need to account for this in our placement algorithm.
			// We may also be able to single out this pin from the route and route it
			// separately. In that case, all of our previous understandings about the
			// route direction assignments will change. So, this would have to be
			// identified before running this algorithm.
		}
		if (inTokens.size() + outTokens.size() > 0) {
			change = buildRouteOffsets(Model::PMOS, inTokens) or change;
			change = buildRouteOffsets(Model::NMOS, outTokens) or change;
			continue;
		}

		// find the largest label
		int maxLabel = -1;
		int index = -1;
		int uindex = -1;
		for (int u = (int)unassigned.size()-1; u >= 0; u--) {
			// TODO(edward.bingham) If there is a direction that violates a via
			// constraint and a direction that doesn't, then we pre-emptively chose
			// the direction that doesn't. If both directions violate the via
			// constraint, then we need to resolve that conflict by pushing the
			// associated pins out to make space for the via.
			int i = unassigned[u];
			int label = max(
				routes[routeConstraints[i].wires[0]].offset[Model::PMOS] + routes[routeConstraints[i].wires[1]].offset[Model::NMOS] + routeConstraints[i].off[0], 
				routes[routeConstraints[i].wires[1]].offset[Model::PMOS] + routes[routeConstraints[i].wires[0]].offset[Model::NMOS] + routeConstraints[i].off[1]
			);

			if (label > maxLabel) {
				maxLabel = label;
				index = i;
				uindex = u;
			}
		}

		if (index >= 0) {
			int label0 = routes[routeConstraints[index].wires[0]].offset[Model::PMOS] + routes[routeConstraints[index].wires[1]].offset[Model::NMOS] + routeConstraints[index].off[0];
			int label1 = routes[routeConstraints[index].wires[1]].offset[Model::PMOS] + routes[routeConstraints[index].wires[0]].offset[Model::NMOS] + routeConstraints[index].off[1];

			// DESIGN(edward.bingham) this randomization implements gradient descent
			// for the route lowering algorithm
			if (label0 < label1 or (label0 == label1 and (rand()&1))) {
				change = true;
				routeConstraints[index].select = 0;
				inTokens.push_back(routeConstraints[index].wires[0]);
				outTokens.push_back(routeConstraints[index].wires[1]);
			} else {
				change = true;
				routeConstraints[index].select = 1;
				inTokens.push_back(routeConstraints[index].wires[1]);
				outTokens.push_back(routeConstraints[index].wires[0]);
			}
			unassigned.erase(unassigned.begin()+uindex);
	
			// Propagate order decision through the group constraints
			if (routeConstraints[index].select >= 0) {
				set<int> prop = propagateRouteConstraint(index);
				change = change or not prop.empty();
				for (auto j = prop.begin(); j != prop.end(); j++) {
					int from = routeConstraints[*j].select;
					inTokens.push_back(routeConstraints[*j].wires[from]);
					outTokens.push_back(routeConstraints[*j].wires[1-from]);
					//unassigned.erase(remove(unassigned.begin(), unassigned.end(), *j), unassigned.end());
				}
			}

			change = buildRouteOffsets(Model::PMOS, inTokens) or change;
			change = buildRouteOffsets(Model::NMOS, outTokens) or change;
			continue;
		}
	}
	return change;
}

// The `window` attempts to prevent too many vias across a route by smoothing the transition
void Router::lowerRoutes(int window) {
	// TODO(edward.bingham) There's still an interaction between route lowering
	// and via merging where it ends up creating a double route for two close
	// pins, causing DRC violations

	// TODO(edward.bingham) For routes that cross over one of the two transitor
	// stacks, I can do one of two things. 1. I could not lower that route,
	// keeping track of pin levels. 2. I could reduce the width of wide vias to
	// provide space for local interconnect and keep the higher layers open.
	// Manual layouts seem to prefer the second option.
	
	// int pinLevel = 1;

	// indexed by [route][pin]
	vector<vector<set<Level> > > blockedLevels(routes.size(), vector<set<Level> >());
	for (int i = 0; i < (int)routes.size(); i++) {
		Rect box = routes[i].layout.box;
		for (int type = 0; type < (int)this->stack.size(); type++) {
			for (int j = 0; j < (int)this->stack[type].pins.size(); j++) {
				const Pin &p0 = this->pin(Index(type, j));
				if (routes[i].offset[Model::PMOS]+box.ur[1] >= this->stack[type].pins[j].lo and routes[i].offset[Model::PMOS]+box.ll[1] <= this->stack[type].pins[j].hi and not routes[i].pins.empty()) {
					auto pos = lower_bound(routes[i].pins.begin(), routes[i].pins.end(), Index(type, j), CompareIndex(this, false));
					if (((pos == routes[i].pins.begin() and this->pin(routes[i].pins[0].idx).offset[0] - p0.offset[0] <= p0.width) or
					    (pos != routes[i].pins.begin() and pos != routes[i].pins.end())) and not routes[i].hasPin(this, Index(type, j))) {
						if (i >= (int)blockedLevels.size()) {
							blockedLevels.resize(i+1);
						}
						int index = max(0, (int)(pos-routes[i].pins.begin())-1);
						if (index >= (int)blockedLevels[i].size()) {
							blockedLevels[i].resize(index+1);
						}
						blockedLevels[i][index].insert(this->stack[type].pins[j].layer);
					}
				}
			}
		}
	}

	/*for (int i = 0; i < (int)blockedLevels.size(); i++) {
		printf("blocked[%d] {", i);
		for (int j = 0; j < (int)blockedLevels[i].size(); j++) {
			printf("(");
			for (auto l = blockedLevels[i][j].begin(); l != blockedLevels[i][j].end(); l++) {
				printf("%d ", *l);
			}
			printf(") ");
		}
		printf("}\n");
	}*/

	for (int i = 0; i < (int)routes.size(); i++) {
		vector<bool> types = routes[i].pinTypes();
		if ((not types[Model::PMOS] or not types[Model::NMOS]) and not routes[i].hasGate(this) and routes[i].numSourceDrain(this) > 1) {
			continue;
		}
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			Level level = this->pin(routes[i].pins[j].idx).layer;
			if (j+1 < (int)routes[i].pins.size()) {
				level = min(level, this->pin(routes[i].pins[j+1].idx).layer);
			}
			level = max(Level(Level::ROUTE, 1), level);
			for (; level < Level(Level::ROUTE, (int)tech->wires.size()); level.idx++) {
				bool found = false;
				for (int k = max(0, j-window); not found and k < min(j+window+1, (int)blockedLevels[i].size()); k++) {
					found = found or (blockedLevels[i][k].find(level) != blockedLevels[i][k].end());
				}
				if (not found) {
					break;
				}
			}
			if (j >= (int)routes[i].level.size()) {
				routes[i].level.resize(j+1, 2);
			}
			routes[i].level[j] = level.idx;
		}
	}

	buildContacts();
}

int Router::computeCost() {
	// TODO(edward.bingham) This may be useful for a second placement round where
	// we use full cell area as the cost function. So, start with the simpler
	// cost function, do an initial layout, then use the layout size as the new
	// layout size as the cost function and each iteration does a full layout.
	// This may be like the detail step of the placement algorithm.
	int left = 1000000000;
	int right = -1000000000;
	for (int type = 0; type < 2; type++) {
		if (this->stack[type].pins.size() > 0 and this->stack[type].pins[0].offset[0] < left) {
			left = this->stack[type].pins[0].offset[0];
		}
		if (this->stack[type].pins.size() > 0 and this->stack[type].pins.back().offset[0] > right) {
			right = this->stack[type].pins.back().offset[0];
		}
	}

	//int cellHeightOverhead = 10;
	cost = cellHeight;//(cellHeightOverhead+right-left)*cellHeight*(int)(1+aStar.size());
	return cost;
}

// Load the pin order from the placer and then compute everything that is
// invariant across different routing solutions
void Router::load(const Placement &place, bool createIO) {
	// Save the resulting placement to the Subckt
	for (int type = 0; type < (int)stack.size(); type++) {
		stack[type].type = type;
		stack[type].pins.clear();
		if (type < (int)place.stack.size()) {
			for (auto pin = place.stack[type].begin(); pin != place.stack[type].end(); pin++) {
				stack[type].push(*tech, *ckt, pin->device, pin->flip);
			}
			if (not place.stack[type].empty() and place.stack[type].back().device >= 0) {
				stack[type].push(*tech, *ckt, -1, false);
			}
		}
	}

	// Draw the pins
	for (int type = 0; type < (int)stack.size(); type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			Pin &pin = this->stack[type].pins[i];
			if (type < 2) {
				pin.width = this->pinWidth(Index(type, i));
				pin.height = this->pinHeight(Index(type, i));
			}

			pin.layout.clear();
			drawPin(pin.layout, *ckt, this->stack[type], i);
		}
	}

	// Create cell-edge IO pins if desired
	if (createIO) {
		for (int i = 0; i < (int)ckt->nets.size(); i++) {
			if (ckt->nets[i].isIO) {
				/*bool found = false;
				for (int j = 0; not found and j < (int)this->stack.size(); j++) {
					for (int k = 0; not found and k < (int)this->stack[j].pins.size(); k++) {
						found = this->stack[j].pins[k].outNet == i and this->stack[j].pins[k].layer >= 1;
					}
				}
				if (not found) {*/
					Index ioPin(2, (int)this->stack[2].pins.size());
					this->stack[2].pins.push_back(Pin(*tech, i, -1));
					this->stack[2].pins.back().offset[0] = -50;
					this->stack[2].pins.back().layer = Level(Level::ROUTE, 2);
				//}
			}
		}
	}

	// Create initial routes
	routes.clear();
	routes.reserve(ckt->nets.size()+2);
	for (int i = 0; i < (int)ckt->nets.size(); i++) {
		routes.push_back(Wire(*tech, i));
	}
	for (int type = 0; type < (int)this->stack.size(); type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			if (this->stack[type].pins[i].outNet >= 0 and this->stack[type].pins[i].outNet < (int)ckt->nets.size()) {
				routes[this->stack[type].pins[i].outNet].addPin(this, Contact(*tech, Index(type, i)));
			} else {
				printf("outNet is out of bounds\n");
			}
		}
	}
	// Delete degenerative routes
	for (int i = (int)routes.size()-1; i >= 0; i--) {
		if (routes[i].pins.size() < 2 and not ckt->nets[routes[i].net].isIO) {
			delRoute(i);
		}
	}

	// Create the two routes for the stacks
	for (int type = 0; type < 2; type++) {
		this->stack[type].route = (int)routes.size();
		routes.push_back(Wire(*tech, flip(type)));
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			routes.back().addPin(this, Contact(*tech, Index(type, i)));
		}
	}

	// Draw the contacts
	buildContacts();

	// Determine the horizontal constraints between pins and contacts
	buildStackConstraints(true);
}

bool Router::solve() {
	buildPinOffsets(0, vector<Index>(), true);
	buildPinOffsets(1, vector<Index>(), true);
	// TODO(edward.bingham) does alignment depend on the pin constraints?
	alignPins();

	buildPinConstraints(0, true);
	lockPinConstraints();
	breakCycles();
	drawRoutes();
	buildRouteConstraints(true, true);
	assignRouteConstraints();
	for (int i = 0; i < (int)stack[2].pins.size(); i++) {
		alignVirtualPin(Index(2, i));
	}

	lowerRoutes();

	buildStackConstraints();
	assignStackConstraints();
	buildPinOffsets(0, vector<Index>(), true);
	buildPinOffsets(1, vector<Index>(), true);
	drawRoutes();

	buildGroupConstraints();
	buildRouteConstraints(false, false);
	assignRouteConstraints();

	assignStackConstraints();
	buildPinOffsets(0, vector<Index>(), true);
	buildPinOffsets(1, vector<Index>(), true);
	drawRoutes();

	if (debug and unresolvedCycle[0]) {
		printf("error: unresolved route constraint cycle from nmos to pmos\n");
	}
	if (debug and unresolvedCycle[1]) {
		printf("error: unresolved route constraint cycle from pmos to nmos\n");
	}
	if (debug and unresolvedPinCycle[0]) {
		printf("error: unresolved constraint cycle in nmos stack\n");
	}
	if (debug and unresolvedPinCycle[1]) {
		printf("error: unresolved constraint cycle in pmos stack\n");
	}

	// TODO(edward.bingham) Assigning the route constraints affects where
	// contacts are relative to each other vertically. This changes the pin
	// spacing required to keep them DRC clean. So we updatePinPos(), but then
	// that will change the pin constraints, but if we update the pin
	// constraints, then that will change the route order.

	// TODO(edward.bingham) There's a bug in the group constraints functionality
	// that's exposed by multiple iterations of this.

	// TODO(edward.bingham) I need to compute cell height from the assignment
	// results so that it can be used to run more placements.

	// TODO(edward.bingham) The route placement should start at the center and
	// work it's way toward the bottom and top of the cell instead of starting at
	// the bottom and working it's way to the top. This would make the cell more
	// dense overall, but give more space for overcell routing. I might want to
	// create directed routing constraints for power and ground that keeps them
	// at the bottom and top of the cell routing so that the two sources can be
	// easily routed in the larger context. Using offset[Model::PMOS] and offset[Model::NMOS]
	// alternatively didn't really work.
	/*
	int minOff = -1;
	int pOff = 0;
	int nOff = 0;
	for (int i = 0; i < (int)routes.size(); i++) {
		if (routes[i].pins.size() != 0) {
			int off = (routes[i].offset[Model::PMOS] - routes[i].offset[Model::NMOS]);
			off = off < 0 ? -off : off;
			if (minOff < 0 or off < minOff) {
				printf("route %d is center\n", i);
				minOff = off;
				pOff = routes[i].offset[Model::PMOS];
				nOff = routes[i].offset[Model::NMOS];
			}
		}
	}

	printf("off %d %d %d\n", minOff, pOff, nOff);
	for (int i = 0; i < (int)routes.size(); i++) {
		if (routes[i].offset[Model::PMOS] < cellHeight/2) {
			routes[i].pos = cellHeight - routes[i].offset[Model::NMOS];
		} else {
			routes[i].pos = routes[i].offset[Model::PMOS];
		}
	}*/

	// TODO(edward.bingham) I may need to create the straps for power and ground
	// depending on the cell placement and global and local routing engine that
	// these cells are interfacing with.

	//print();
	return not unresolvedCycle[0] and not unresolvedCycle[1] and not unresolvedPinCycle[0] and not unresolvedPinCycle[1];
}

void Router::annotateAreaPerim(Subckt &ckt) {
	int poly = tech->wires[0].draw;
	for (int type = 0; type < 2; type++) {
		if (stack[type].pins.empty()) {
			// there are no pins in the stack
			continue;
		}

		auto pin = stack[type].pins.begin();
		for (; pin != stack[type].pins.end() and pin->device < 0; pin++);
		while (pin != stack[type].pins.end()) {
			// DESIGN(edward.bingham) pin->device >= 0
			auto mos = ckt.mos.begin()+pin->device;
			int leftTerm = pin->leftNet == mos->drain ? 0 : 1;
			int rightTerm = pin->rightNet == mos->source ? 1 : 0;

			Level diffLevel = tech->models[mos->model].diff;
			int diff = tech->at(diffLevel).draw;
			vector<int> vias = tech->via(diffLevel, Level(Level::ROUTE, 1));
			if (not vias.empty()) {
				int via = tech->vias[vias[0]].draw;
				vec2i diffOverPoly = max(tech->getEnclosing(diff, poly), 0).swap(0,1);
				vec2i diffOverVia = max(tech->getEnclosing(diff, via), 0).swap(0,1);

				int area = 0;
				int perim = pin->height;
				auto curr = pin;
				int gateCount = 1;
				while (true) {
					auto prev = std::prev(curr);
					if (curr == stack[type].pins.begin() or (curr->device < 0 and prev->device < 0)) {
						int l = (curr->device < 0 ? diffOverVia[0] : diffOverPoly[0]);
						int h = curr->height;

						area += l*h;
						perim += l*2 + h;
						break;
					} else {
						int split;
						if (prev->height < curr->height) {
							split = curr->offset[0] - (curr->device < 0 ? diffOverVia[0] : diffOverPoly[0]);
						} else {
							split = prev->offset[0] + prev->width + (prev->device < 0 ? diffOverVia[0] : diffOverPoly[0]);
						}

						int l1 = curr->offset[0] - split;
						int h1 = curr->height;
						int l0 = split - (prev->offset[0] + (prev->device < 0 ? 0 : prev->width));
						int h0 = prev->height;

						area += l1*h1 + l0*h0;
						perim += 2*l1 + (h1-h0) + 2*l0;
						if (prev->device >= 0) {
							perim += h0;
							gateCount++;
							break;
						}
					}
					curr = prev;
				}
				mos->area[leftTerm] = area/gateCount;
				mos->perim[leftTerm] = perim/gateCount;

				area = 0;
				perim = pin->height;
				curr = pin;
				gateCount = 1;
				while (true) {
					auto next = std::next(curr);
					if (curr == std::prev(stack[type].pins.end()) or (curr->device < 0 and next->device < 0)) {
						int l = (curr->device < 0 ? diffOverVia[0] : diffOverPoly[0]);
						int h = curr->height;

						area += l*h;
						perim += l*2 + h;
						break;
					} else {
						int split;
						if (curr->height < next->height) {
							split = next->offset[0] - (next->device < 0 ? diffOverVia[0] : diffOverPoly[0]);
						} else {
							split = curr->offset[0] + curr->width + (curr->device < 0 ? diffOverVia[0] : diffOverPoly[0]);
						}

						int l0 = split - (curr->offset[0] + (curr->device < 0 ? 0 : curr->width));
						int h0 = curr->height;
						int l1 = next->offset[0] - split;
						int h1 = next->height;

						area += l1*h1 + l0*h0;
						perim += 2*l1 + (h1-h0) + 2*l0;
						if (next->device >= 0) {
							perim += h1;
							gateCount++;
							break;
						}
					}
					curr = next;
				}
				mos->area[rightTerm] = area/gateCount;
				mos->perim[rightTerm] = perim/gateCount;
			}

			for (pin = std::next(pin); pin != stack[type].pins.end() and pin->device < 0; pin++);
		}
	}
}

void Router::print() {
	printf("NMOS\n");
	for (int i = 0; i < (int)this->stack[0].pins.size(); i++) {
		const Pin &pin = this->stack[0].pins[i];
		printf("pin[%d] dev=%d nets=%s(%d) -> %s(%d) -> %s(%d) size=%dx%d pos=%d,%d bound=%d,%d lo=%d hi=%d\n", i, pin.device, ckt->netName(pin.leftNet).c_str(), pin.leftNet, ckt->netName(pin.outNet).c_str(), pin.outNet, ckt->netName(pin.rightNet).c_str(), pin.rightNet, pin.width, pin.height, pin.offset[0], pin.offset[1], pin.bound[0], pin.bound[1], pin.lo, pin.hi);
	}

	printf("\nPMOS\n");
	for (int i = 0; i < (int)this->stack[1].pins.size(); i++) {
		const Pin &pin = this->stack[1].pins[i];
		printf("pin[%d] dev=%d nets=%s(%d) -> %s(%d) -> %s(%d) size=%dx%d pos=%d,%d bound=%d,%d lo=%d hi=%d\n", i, pin.device, ckt->netName(pin.leftNet).c_str(), pin.leftNet, ckt->netName(pin.outNet).c_str(), pin.outNet, ckt->netName(pin.rightNet).c_str(), pin.rightNet, pin.width, pin.height, pin.offset[0], pin.offset[1], pin.bound[0], pin.bound[1], pin.lo, pin.hi);
	}

	printf("\nRoutes\n");
	for (int i = 0; i < (int)routes.size(); i++) {
		printf("wire[%d] %s(%d) %d->%d in:%d out:%d: ", i, (routes[i].net >= 0 and routes[i].net < (int)ckt->nets.size() ? ckt->nets[routes[i].net].name.c_str() : ""), routes[i].net, routes[i].left, routes[i].right, routes[i].offset[Model::PMOS], routes[i].offset[Model::NMOS]);
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			printf("(%d,%d):%d,%d:%d,%d ", routes[i].pins[j].idx.type, routes[i].pins[j].idx.pin, routes[i].pins[j].offset[0], routes[i].pins[j].offset[1], routes[i].pins[j].bound[0], routes[i].pins[j].bound[1]);
		}
		printf("\n");
	}

	printf("\nStack Constraints\n");
	for (int i = 0; i < (int)stackConstraints.size(); i++) {
		auto c = stackConstraints.begin() + i;
		printf("stack[%d] (%d %d) %s (%d %d): %d,%d\n", i, c->pins[0].type, c->pins[0].pin, (c->select == 0 ? "->" : (c->select == 1 ? "<-" : "--")), c->pins[1].type, c->pins[1].pin, c->off[0], c->off[1]);
	}

	for (int i = 0; i < (int)routes.size(); i++) {
		auto route = routes.begin()+i;

		printf("route %d\n", i);
		for (int j = 0; j < (int)route->pins.size(); j++) {
			auto ct = route->pins.begin()+j;
			printf("\t(%d,%d) {", ct->idx.type, ct->idx.pin);
			for (auto c = ct->constraints.begin(); c != ct->constraints.end(); c++) {
				printf("%s(%d,%d):%d,%d ", (c->select == 0 ? "<-" : (c->select == 1 ? "->" : "--")), c->pin.type, c->pin.pin, c->off[0], c->off[1]);
			}
			printf("}\n");
		}
		printf("\n");
	}

	printf("\nRouting Constraints\n");
	for (auto i = pinConstraints.begin(); i != pinConstraints.end(); i++) {
		printf("pin %d -> %d\n", i->from, i->to);
	}
	for (int i = 0; i < (int)routeConstraints.size(); i++) {
		auto c = routeConstraints.begin() + i;
		printf("route[%d] %d %s %d: %d,%d\n", i, c->wires[0], (c->select == 0 ? "->" : (c->select == 1 ? "<-" : "--")), c->wires[1], c->off[0], c->off[1]);
	}
	for (int i = 0; i < (int)viaConstraints.size(); i++) {
		printf("via[%d] {", i);
		for (auto j = viaConstraints[i].side[0].begin(); j != viaConstraints[i].side[0].end(); j++) {
			printf("(%d %d):%d ", j->idx.type, j->idx.pin, j->off);
		}
		printf("} -> (%d %d) -> {", viaConstraints[i].idx.type, viaConstraints[i].idx.pin);
		for (auto j = viaConstraints[i].side[1].begin(); j != viaConstraints[i].side[1].end(); j++) {
			printf("(%d %d):%d ", j->idx.type, j->idx.pin, j->off);
		}
		printf("}\n");
	}

	printf("\n");
}

}
