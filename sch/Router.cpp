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
	
	align = -1;

	layer = 0;
	width = 0;
	height = 0;
	pos = 0;
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::Pin(const Tech &tech, int outNet, int baseNet) : layout(tech) {
	this->device = -1;
	this->outNet = outNet;
	this->leftNet = outNet;
	this->rightNet = outNet;
	this->baseNet = baseNet;

	align = -1;

	layer = 1;
	width = 0;
	height = 0;
	pos = 0;
	lo = numeric_limits<int>::max();
	hi = numeric_limits<int>::min();
}

Pin::Pin(const Tech &tech, int device, int outNet, int leftNet, int rightNet, int baseNet) : layout(tech) {
	this->device = device;
	this->outNet = outNet;
	this->leftNet = leftNet;
	this->rightNet = rightNet;
	this->baseNet = baseNet;

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

bool Pin::offsetToPin(Index pin, int value) {
	//printf("offsetToPin %d\n", value);
	auto result = toPin.insert(pair<Index, int>(pin, value));
	if (not result.second and value > result.first->second) {
		result.first->second = value;
		return true;
	}
	return result.second;
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

bool Contact::offsetFromPin(Index pin, int value) {
	auto result = fromPin.insert(pair<Index, int>(pin, value));
	if (not result.second and value > result.first->second) {
		result.first->second = value;
		return true;
	}
	return result.second;
}

bool Contact::offsetToPin(Index pin, int value) {
	auto result = toPin.insert(pair<Index, int>(pin, value));
	if (not result.second and value > result.first->second) {
		result.first->second = value;
		return true;
	}
	return result.second;
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
	return p0.pos < p1.pos or (orderIndex and p0.pos == p1.pos and i0 < i1);
}

bool CompareIndex::operator()(const Contact &c0, const Index &i1) {
	const Pin &p0 = rt->pin(c0.idx);
	const Pin &p1 = rt->pin(i1);
	return p0.pos < p1.pos or (orderIndex and p0.pos == p1.pos and c0.idx < i1);
}

bool CompareIndex::operator()(const Contact &c0, const Contact &c1) {
	const Pin &p0 = rt->pin(c0.idx);
	const Pin &p1 = rt->pin(c1.idx);
	return p0.pos < p1.pos or (orderIndex and p0.pos == p1.pos and c0.idx < c1.idx);
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
	auto stack = rt->stack.begin()+ct.idx.type;
	auto pin = stack->pins.begin()+ct.idx.pin;
	if (left < 0 or pin->pos < left) {
		left = pin->pos;
	}
	if (right < 0 or pin->pos+pin->width > right) {
		right = pin->pos + pin->width;
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
		left = rt->pin(pins[0].idx).pos;
		right = rt->pin(pins.back().idx).pos+rt->pin(pins.back().idx).width;
	} else {
		left = -1;
		right = -1;
	}
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

bool Wire::hasGate(const Router *rt) const {
	for (int i = 0; i < (int)pins.size(); i++) {
		if (rt->pin(pins[i].idx).device >= 0) {
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

	if (device >= 0) {
		fromNet = ckt.mos[device].left(flip);
		toNet = ckt.mos[device].right(flip);
		gateNet = ckt.mos[device].gate;
		baseNet = ckt.mos[device].base;
	}

	// Get information about the previous transistor on the stack. First if
	// statement in the funtion guarantees that there is at least one transistor
	// already on the stack.
	bool link = (pins.size() > 0 and gateNet >= 0 and fromNet == pins.back().rightNet and baseNet == pins.back().baseNet);

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
	this->ckt = &place.ckt;
	this->cycleCount = 0;
	this->cellHeight = 0;
	this->cost = 0;
	this->progress = progress;
	this->debug = debug;
	this->allowOverCell = true;
	this->unresolvedCycle[0] = false;
	this->unresolvedCycle[1] = false;
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
		return tech->paint[tech->wires[0].draw].minWidth;
		//return ckt->mos[device].size[0];
	}
	// this pin is a contact
	return tech->paint[tech->wires[1].draw].minWidth;
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

vector<bitset> Router::routeOrderMap(int type) {
	vector<bitset> result;
	result.resize(routes.size());
	for (int i = 0; i < (int)routes.size(); i++) {
		auto n = type == Model::PMOS ? next(i) : prev(i);
		for (auto j = n.begin(); j != n.end(); j++) {
			result[j->first].set(i, true);
			result[j->first] |= result[i];
		}
	}
	return result;
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
// updatePinPos() - this determines what the pin constraints are
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
					minOffset(&off, 1, pmos.layout, pmos.pos,
														 nmos.layout, nmos.pos,
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
							minOffset(&off, 1, playout, pmos.pos,
																 nlayout, nmos.pos,
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
								if (minOffset(&off, 1, c0->layout, pmos.pos,
								                       c1->layout, nmos.pos,
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

void Router::buildRoutes() {
	routes.clear();

	// Create initial routes
	routes.reserve(ckt->nets.size()+2);
	for (int i = 0; i < (int)ckt->nets.size(); i++) {
		routes.push_back(Wire(*tech, i));
	}
	for (int type = 0; type < (int)this->stack.size(); type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			if (this->stack[type].pins[i].outNet < (int)ckt->nets.size()) {
				routes[this->stack[type].pins[i].outNet].addPin(this, Contact(*tech, Index(type, i)));
			} else {
				printf("outNet is out of bounds\n");
			}
		}
	}
	for (int i = (int)routes.size()-1; i >= 0; i--) {
		if (routes[i].pins.size() < 2 and not ckt->nets[routes[i].net].isIO) {
			delRoute(i);
		}
	}

	for (int type = 0; type < 2; type++) {
		this->stack[type].route = (int)routes.size();
		routes.push_back(Wire(*tech, flip(type)));
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			routes.back().addPin(this, Contact(*tech, Index(type, i)));
		}
	}

	buildContacts();
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

bool Router::findCycle(int s, const vector<set<int> > &Ak, vector<vector<int> > &cycles) {
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
		if (stack.back().searched) {
			if (found) {
				// Unblock vertex v
				vector<int> unblock(1, v);
				while (not unblock.empty()) {
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
					cycles.push_back(path);
					found = true;
				} else if (not blocked[*w]) {
					stack.push_back(frame(path));
					stack.back().path.push_back(*w);
				}
			}
		}
	}

	return found;
}

bool Router::findCycles(vector<vector<int> > &cycles) {
	// DESIGN(edward.bingham) There can be multiple cycles with the same set of
	// nodes as a result of multiple pin constraints. This function does not
	// differentiate between those cycles. Doing so could introduce an
	// exponential blow up, and we can ensure that we split those cycles by
	// splitting on the node in the cycle that has the most pin constraints
	// (maximising min(in.size(), out.size()))
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

	bool found = false;
	for (int s = 0; s < (int)Ak.size(); s++) {
		if (not Ak[s].empty()) {
			found = findCycle(s, Ak, cycles) or found;
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
	printf("cycles %d\n", (int)cycles.size());

	return found;
}

void Router::breakRoute(int route, set<int> cycleRoutes) {
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

	int left = min(this->stack[0].pins[0].pos, this->stack[1].pins[0].pos);
	int right = max(this->stack[0].pins.back().pos, this->stack[1].pins.back().pos);
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
			count.erase(count.begin()+fromIdx);
			routes[route].pins.erase(from);
		} else if (toIdx != -1) {
			wn.addPin(this, *to);
			wnHasGate = wnHasGate or this->pin(to->idx).isGate();
			count.erase(count.begin()+toIdx);
			routes[route].pins.erase(to);
		}
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
		const Pin &pin = this->pin(routes[route].pins[i].idx);
		int distanceFromCenter = abs(pin.pos-center);

		if ((sharedCount < 0 or count[i] < sharedCount) or
		    (count[i] == sharedCount and ((sharedIsGate and pin.isContact()) or
		    distanceFromCenter > sharedDistanceFromCenter))) {
		//if (sharedCount < 0 or (count[i] < sharedCount or
		//    (count[i] == sharedCount and ((sharedIsGate and pin.isContact()) or
		//    (sharedIsGate == (pin.isGate()) and distanceFromCenter > sharedDistanceFromCenter))))) {
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
		routes[route].pins.erase(routes[route].pins.begin()+sharedPin);
		count.erase(count.begin()+sharedPin);
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
		Index idx(2, (int)this->stack[2].pins.size());
		this->stack[2].pins.push_back(Pin(*tech, routes[route].net, -1));
		drawPin(this->stack[2].pins.back().layout, *ckt, this->stack[2], idx.pin);
		this->stack[2].pins.back().pos = -50;
		this->stack[2].pins.back().lo = routes[route].offset[Model::PMOS];
		this->stack[2].pins.back().hi = routes[route].offset[Model::PMOS];
		Contact virtPin(*tech, idx);

		// TODO(edward.bingham) draw contacts for virtual pins
		// TODO(edward.bingham) add horizontal constraints for virtual pins
		wp.addPin(this, virtPin);
		wn.addPin(this, virtPin);
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
			auto ct = routes[route].pins.begin()+i;

			bool isGate = this->pin(ct->idx).isContact();
			if (ct->idx.type == Model::PMOS and not isGate) {
				wp.addPin(this, *ct);
			} else {
				wn.addPin(this, *ct);
				wnHasGate = wnHasGate or isGate;
			}
			routes[route].pins.pop_back();
			count.pop_back();
		}
	}	else if (not wnHasGate) {
		// Put all non-gate NMOS pins into wn and all remaining pins into wp
		for (int i = (int)routes[route].pins.size()-1; i >= 0; i--) {
			auto ct = routes[route].pins.begin()+i;
			bool isGate = this->pin(ct->idx).isContact();
			if (ct->idx.type == Model::NMOS and not isGate) {
				wn.addPin(this, *ct);
			} else {
				wp.addPin(this, *ct);
				wpHasGate = wpHasGate or isGate;
			}
			routes[route].pins.pop_back();
			count.pop_back();
		}
	} else {
		for (int i = (int)routes[route].pins.size()-1; i >= 0; i--) {
			auto ct = routes[route].pins.begin()+i;
			// Determine horizontal location of wp and wn relative to sharedPin, then
			// place all pins on same side of sharedPin as wp or wn into that wp or wn
			// respectively.
			bool isGate = this->pin(ct->idx).isContact();
			int pos = this->pin(ct->idx).pos;
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
			routes[route].pins.pop_back();
			count.pop_back();
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
}

void Router::breakCycles(vector<vector<int> > cycles) {
	//int startingRoutes = (int)routes.size();

	// count up cycle participation for heuristic
	vector<vector<int> > cycleCount(routes.size(), vector<int>());
	for (int i = 0; i < (int)cycles.size(); i++) {
		for (int j = 0; j < (int)cycles[i].size(); j++) {
			cycleCount[cycles[i][j]].push_back(i);
		}
	}

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

	//printf("Starting Cycles\n");
	//for (int i = 0; i < (int)cycles.size(); i++) {
	//	printf("cycle {");
	//	for (int j = 0; j < (int)cycles[i].size(); j++) {
	//		if (j != 0) {
	//			printf(" ");
	//		}
	//		printf("%d", cycles[i][j]);
	//	}
	//	printf("}\n");
	//}

	//printf("NMOS\n");
	//for (int i = 0; i < (int)this->stack[0].pins.size(); i++) {
	//	printf("pin %d %d->%d->%d: %dx%d %d %d\n", this->stack[0].pins[i].device, this->stack[0].pins[i].leftNet, this->stack[0].pins[i].outNet, this->stack[0].pins[i].rightNet, this->stack[0].pins[i].width, this->stack[0].pins[i].height, this->stack[0].pins[i].off, this->stack[0].pins[i].pos);
	//}

	//printf("\nPMOS\n");
	//for (int i = 0; i < (int)this->stack[1].pins.size(); i++) {
	//	printf("pin %d %d->%d->%d: %dx%d %d %d\n", this->stack[1].pins[i].device, this->stack[1].pins[i].leftNet, this->stack[1].pins[i].outNet, this->stack[1].pins[i].rightNet, this->stack[1].pins[i].width, this->stack[1].pins[i].height, this->stack[1].pins[i].off, this->stack[1].pins[i].pos);
	//}

	//printf("\nRoutes\n");
	//for (int i = 0; i < (int)routes.size(); i++) {
	//	printf("wire %d %d->%d: ", routes[i].net, routes[i].left, routes[i].right);
	//	for (int j = 0; j < (int)routes[i].pins.size(); j++) {
	//		printf("(%d,%d) ", routes[i].pins[j].type, routes[i].pins[j].pin);
	//	}
	//	printf("\n");
	//}

	//printf("\nConstraints\n");
	//for (int i = 0; i < (int)pinConstraints.size(); i++) {
	//	printf("vert %d -> %d: %d\n", pinConstraints[i].from, pinConstraints[i].to, pinConstraints[i].off);
	//}
	//for (int i = 0; i < (int)routeConstraints.size(); i++) {
	//	printf("horiz %d -- %d: %d\n", routeConstraints[i].wires[0], routeConstraints[i].wires[1], routeConstraints[i].off);
	//}

	while (cycles.size() > 0) {
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
		int maxCycleCount = -1;
		int maxDensity = -1;
		int route = -1;
		for (int i = 0; i < (int)cycleCount.size(); i++) {
			int density = min(numIn[i], numOut[i]);
			if (routes[i].net >= 0 and (((int)cycleCount[i].size() > maxCycleCount or
					((int)cycleCount[i].size() == maxCycleCount and density > maxDensity)))) {
				route = i;
				maxCycleCount = cycleCount[i].size();
				maxDensity = density;
			}
		}

		set<int> cycleRoutes;
		for (int i = 0; i < (int)cycleCount[route].size(); i++) {
			cycleRoutes.insert(cycles[cycleCount[route][i]].begin(), cycles[cycleCount[route][i]].end());
		}
		cycleRoutes.erase(route);

		//printf("Breaking Route %d: ", route);
		//for (auto i = cycleRoutes.begin(); i != cycleRoutes.end(); i++) {
		//	printf("%d ", *i);
		//}
		//printf("\n");
		breakRoute(route, cycleRoutes);
		//printf("wire %d %d->%d: ", routes[route].net, routes[route].left, routes[route].right);
		//for (int j = 0; j < (int)routes[route].pins.size(); j++) {
		//	printf("(%d,%d) ", routes[route].pins[j].type, routes[route].pins[j].pin);
		//}
		//printf("\n");
		//printf("wire %d %d->%d: ", routes.back().net, routes.back().left, routes.back().right);
		//for (int j = 0; j < (int)routes.back().pins.size(); j++) {
		//	printf("(%d,%d) ", routes.back().pins[j].type, routes.back().pins[j].pin);
		//}
		//printf("\n");

		// recompute cycles and cycleCount
		while (cycleCount[route].size() > 0) {		
			int cycle = cycleCount[route].back();
			cycles.erase(cycles.begin()+cycle);
			for (int i = 0; i < (int)cycleCount.size(); i++) {
				for (int j = (int)cycleCount[i].size()-1; j >= 0; j--) {
					if (cycleCount[i][j] > cycle) {
						cycleCount[i][j]--;
					} else if (cycleCount[i][j] == cycle) {
						cycleCount[i].erase(cycleCount[i].begin()+j);
					}
				}
			}
		}
	}

	/*cycles = findCycles();
	if (cycles.size() > 0) {
		printf("error: cycles not broken %d -> %d\n", startingRoutes, (int)routes.size());
		for (int i = 0; i < (int)cycles.size(); i++) {
			printf("cycle {");
			for (int j = 0; j < (int)cycles[i].size(); j++) {
				if (j != 0) {
					printf(" ");
				}
				printf("%d", cycles[i][j]);
			}
			printf("}\n");
		}

		printf("\n\n");
	}*/
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

struct Alignment {
	Alignment() {}
	Alignment(const Router *rt, int pmos, int nmos) {
		this->rt = rt;
		this->pin[Model::PMOS] = pmos;
		this->pin[Model::NMOS] = nmos;
	}
	~Alignment() {}

	const Router *rt;
	array<int, 2> pin;

	int dist() const {
		array<bool, 2> startOfStack = {
			(pin[0] == 0 or rt->stack[0].pins[1-pin[0]].isContact()) and rt->stack[0].pins[pin[0]].isContact(),
			(pin[1] == 0 or rt->stack[1].pins[1-pin[1]].isContact()) and rt->stack[1].pins[pin[1]].isContact()
		};

		int result = rt->stack[1].pins[pin[1]].pos - rt->stack[0].pins[pin[0]].pos;
		if ((result < 0 and startOfStack[1]) or (result > 0 and startOfStack[0])) {
			// We can move the start of the stack around as much as we want
			result = 1;
		}
		result = (result < 0 ? -result : result) + 1;

		int coeff = 1;
		// If the two pins aren't on the same layer, we don't care as much
		if (rt->stack[1].pins[pin[1]].layer != rt->stack[1].pins[pin[1]].layer) {
			coeff = 3;
		// Prefer matching up gates rather than source or drain
		} else if (rt->stack[1].pins[pin[1]].layer > 0) {
			coeff = 2;
		}

		return result * coeff;
	}

	bool conflictsWith(const Alignment &a0) {
		return pin[1] == a0.pin[1] or pin[0] == a0.pin[0] or
			    (pin[1] < a0.pin[1] and pin[0] > a0.pin[0]) or
			    (pin[1] > a0.pin[1] and pin[0] < a0.pin[0]);
	}
};

bool operator>(const Alignment &a0, const Alignment &a1) {
	return a0.dist() > a1.dist();
}

void Router::alignVirtualPins() {
	// TODO(edward.bingham) Find a list of potential ranges for each pin. This is
	// determined by the other pins and their hi and lo values. I also need to
	// think about routes.  Ranges should be defined in terms of pins... but
	// there are two layers of pins where the pin alignments between the two
	// change and so pin ranges don't continue to mean the same thing. So, what
	// if I just define the ranges as an absolute measure? Then if the pin
	// placements change... There is a cyclic dependency

	// TODO(edward.bingham) I need to compare virtual pins against eachother as well
	vector<map<pair<int, Index>, array<int, 2> > > constraints;
	constraints.resize(stack[2].pins.size());
	for (int i = 0; i < (int)stack[2].pins.size(); i++) {
		for (int type = 0; type < 2; type++) {
			for (int j = 0; j < (int)stack[type].pins.size(); j++) {
				if (j == i) {
					continue;
				}

				array<int, 2> off;
				bool fromto = minOffset(&off[0], 0, stack[2].pins[i].layout, 0, stack[type].pins[j].layout, 0, Layout::DEFAULT, Layout::DEFAULT);
				bool tofrom = minOffset(&off[1], 0, stack[type].pins[j].layout, 0, stack[2].pins[i].layout, 0, Layout::DEFAULT, Layout::DEFAULT);

				// TODO(edward.bingham) lo and hi values don't include route or contact height
				if ((fromto or tofrom) and stack[2].pins[i].lo < stack[type].pins[j].hi and stack[type].pins[j].lo < stack[2].pins[i].hi) {
					constraints[i].insert(pair<pair<int, Index>, array<int, 2> >(pair<int, Index>(stack[type].pins[j].pos, Index(type, j)), off));
				}
			}
		}
	}

	vector<map<int, array<int, 2> > > ranges;
	ranges.resize(constraints.size());
	for (int i = 0; i < (int)constraints.size(); i++) {
		auto k = constraints[i].begin();
		int cost = 0;
		for (auto route = routes.begin(); route != routes.end(); route++) {
			if (route->hasPin(this, Index(2, i))) {
				if (route->left > k->first.first) {
					cost += route->left - k->first.first;
				}
			}
		}
		ranges[i].insert(pair<int, array<int, 2> >(cost, array<int, 2>({std::numeric_limits<int>::min(), k->first.first})));
		for (auto j = constraints[i].begin(); j != constraints[i].end(); j++) {
			auto k = j;
			k++;
			if (k == constraints[i].end()) {
				int cost = 0;
				for (auto route = routes.begin(); route != routes.end(); route++) {
					if (route->hasPin(this, Index(2, i))) {
						if (route->right < j->first.first) {
							cost += j->first.first - route->right;
						}
					}
				}
				ranges[i].insert(pair<int, array<int, 2> >(cost, array<int, 2>({j->first.first, std::numeric_limits<int>::max()})));

				continue;
			} else {
				int space = k->first.first - j->first.first;
				int blocked = k->second[0] + j->second[1];
				if (blocked < space) {
					int cost = 0;
					for (auto route = routes.begin(); route != routes.end(); route++) {
						if (route->hasPin(this, Index(2, i))) {
							if (route->left > k->first.first) {
								cost += route->left - k->first.first;
							} else if (route->right < j->first.first) {
								cost += j->first.first - route->right;
							}
						}
					}

					ranges[i].insert(pair<int, array<int, 2> >(cost, array<int, 2>({j->first.first, k->first.first})));
				}
			}
		}
	}

	for (int i = 0; i < (int)ranges.size(); i++) {
		auto j = ranges[i].begin();
		stack[2].pins[i].pos = j->second[0];
		if (stack[2].pins[i].pos == std::numeric_limits<int>::min()) {
			stack[2].pins[i].pos = j->second[1];
		}
	}
}

void Router::addIOPins() {
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
				this->stack[2].pins.back().pos = -50;
				this->stack[2].pins.back().layer = 2;
			//}
		}
	}
}

void Router::buildPins() {
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

	buildHorizConstraints();
	updatePinPos();
	alignPins(200, true);
}

void Router::buildContacts() {
	for (int i = 0; i < (int)routes.size(); i++) {
		if (routes[i].net < 0) {
			continue;
		}

		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			Pin &pin = this->pin(routes[i].pins[j].idx);
			int prevLevel = routes[i].getLevel(j-1);
			int nextLevel = routes[i].getLevel(j);
			int maxLevel = max(pin.layer, max(nextLevel, prevLevel));
			int minLevel = min(pin.layer, min(nextLevel, prevLevel));

			routes[i].pins[j].layout.clear();
			drawViaStack(routes[i].pins[j].layout, routes[i].net, pin.baseNet, minLevel, maxLevel, vec2i(0, 0), vec2i(0,0), vec2i(0,0));
		}
	}
}

bool Router::buildHorizConstraints(bool reset) {
	bool change = false;
	if (reset) {
		for (int type = 0; type < (int)this->stack[type].pins.size(); type++) {
			for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
				this->stack[type].pins[i].toPin.clear();
			}
		}

		for (int i = 0; i < (int)routes.size(); i++) {
			for (int j = 0; j < (int)routes[i].pins.size(); j++) {
				routes[i].pins[j].fromPin.clear();
				routes[i].pins[j].toPin.clear();
			}
		}
	}


	// TODO(edward.bingham) create layout connecting routes, then check spacing
	// from that instead of checking spacing from whole pin. The pin might be
	// quite a bit larger than the connective tissue between the vias across
	// routes.
	for (int type = 0; type < 2; type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			Pin &pin = this->stack[type].pins[i];

			int off = 0;
			if (i+1 < (int)this->stack[type].pins.size()) {
				Pin &next = this->stack[type].pins[i+1];
				int substrateMode = (pin.isGate() or next.isGate()) ? Layout::MERGENET : Layout::DEFAULT;
				if (minOffset(&off, 0, pin.layout, 0, next.layout, 0, substrateMode, Layout::DEFAULT, false)) {
					change = pin.offsetToPin(Index(type, i+1), off) or change;
				} else if (debug) {
					printf("error: no offset found at pin (%d,%d)\n", type, i+1);
				}
			}

			for (int j = 0; j < (int)routes.size(); j++) {
				// TODO(edward.bingham) might be preferable to set routingMode to
				// Layout::MERGENET and to re-enable this skip condition, then add
				// rectangles into the layout to fill in nets with minimum spacing
				// violations.
				int routingMode = Layout::MERGENET;
				if (routes[j].net < 0 or routes[j].hasPin(this, Index(type, i))) {
					continue;
				}

				for (int k = 0; k < (int)routes[j].pins.size(); k++) {
					int off = 0;
					if ((routes[j].pins[k].idx.type != type or i < routes[j].pins[k].idx.pin) and
					    minOffset(&off, 0, pin.layout, 0, routes[j].pins[k].layout, 0, Layout::IGNORE, routingMode)) {
						change = routes[j].pins[k].offsetFromPin(Index(type, i), off) or change;
					}

					off = 0;
					if ((routes[j].pins[k].idx.type != type or routes[j].pins[k].idx.pin < i) and
					    minOffset(&off, 0, routes[j].pins[k].layout, 0, pin.layout, 0, Layout::IGNORE, routingMode)) {
						change = routes[j].pins[k].offsetToPin(Index(type, i), off) or change;
					}
				}
			}
		}
	}
	return change;
}

// depends on:
// buildHorizConstraints() - these constraints determine the
//                           position of the pins
bool Router::updatePinPos(bool reset) {
	bool change = false;
	if (reset) {
		change = true;
		for (int type = 0; type < (int)this->stack.size(); type++) {
			for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
				this->stack[type].pins[i].pos = 0;
			}
		}

		for (int i = 0; i < (int)routes.size(); i++) {
			for (int j = 0; j < (int)routes[i].pins.size(); j++) {
				routes[i].pins[j].left = numeric_limits<int>::min();
				routes[i].pins[j].right = numeric_limits<int>::max();
			}
		}
	}

	vector<Index> stack;
	stack.reserve(20);
	if (this->stack[0].pins.size() > 0 and this->stack[1].pins.size() > 0) {
		if (this->stack[0].pins[0].align < 0) {
			stack.push_back(Index(0, 0));
			stack.push_back(Index(1, 0));
		} else {
			stack.push_back(Index(1, 0));
			stack.push_back(Index(0, 0));
		}
	} else if (this->stack[0].pins.size() > 0) {
		stack.push_back(Index(0, 0));
	} else if (this->stack[1].pins.size() > 0) {
		stack.push_back(Index(1, 0));
	}

	while (not stack.empty()) {
		//printf("stack: %d\n", (int)stack.size());
		// handle pin to pin constraints and alignment constraints
		Index curr = stack.back();
		stack.pop_back();
		Pin &pin = this->pin(curr);

		for (auto off = pin.toPin.begin(); off != pin.toPin.end(); off++) {
			Pin &next = this->pin(off->first);
			int pos = pin.pos+off->second;
			if (next.pos < pos) {
				//printf("pinToPin (%d,%d)->%d->(%d,%d) %d->%d\n", curr.type, curr.pin, off->second, off->first.type, off->first.pin, next.pos, pos);
				change = true;
				next.pos = pos;
				stack.push_back(off->first);
			}
		}
		if (stack.empty() and curr.pin+1 < (int)this->stack[curr.type].pins.size() and this->stack[curr.type].pins[curr.pin+1].pos == 0) {
			stack.push_back(Index(curr.type, curr.pin+1));
		}

		if (not stack.empty()) {
			sort(stack.rbegin(), stack.rend());
			stack.erase(unique(stack.begin(), stack.end()), stack.end());
			continue;
		}

		// handle pin alignments
		for (int type = 0; type < 2; type++) {
			for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
				Pin &pin = this->stack[type].pins[i];
				if (pin.align >= 0) {
					Pin &align = this->stack[1-type].pins[pin.align];
					if (pin.pos < align.pos) {
						//printf("pinAlign (%d,%d)--(%d,%d) %d->%d\n", type, i, 1-type, pin.align, pin.pos, align.pos);
						change = true;
						pin.pos = align.pos;
						stack.push_back(Index(type, i));
					} else if (align.pos < pin.pos) {
						//printf("pinAlign (%d,%d)--(%d,%d) %d->%d\n", 1-type, pin.align, type, i, align.pos, pin.pos);
						change = true;
						align.pos = pin.pos;
						stack.push_back(Index(1-type, pin.align));
					}
				}
			}
		}
		if (not stack.empty()) {
			sort(stack.rbegin(), stack.rend());
			stack.erase(unique(stack.begin(), stack.end()), stack.end());
			continue;
		}

		// handle pin to via and via to pin constraints
		for (int i = 0; i < (int)routes.size(); i++) {
			if (routes[i].net < 0) {
				continue;
			}

			for (int j = 0; j < (int)routes[i].pins.size(); j++) {
				Pin &pin = this->pin(routes[i].pins[j].idx);

				for (auto off = routes[i].pins[j].fromPin.begin(); off != routes[i].pins[j].fromPin.end(); off++) {
					Pin &prev = this->pin(off->first);
					if (prev.pos < pin.pos and routes[i].offset[Model::PMOS] >= prev.lo and routes[i].offset[Model::PMOS] <= prev.hi) {
						int pos = prev.pos+off->second;
						if (routes[i].pins[j].left < pos) {
							change = true;
							//printf("pinToVia (%d,%d)->%d->(%d,%d) %d->%d\n", off->first.type, off->first.pin, off->second, i, j, routes[i].pins[j].left, pos);
							routes[i].pins[j].left = pos;
						}
					}
				}

				for (auto off = routes[i].pins[j].toPin.begin(); off != routes[i].pins[j].toPin.end(); off++) {
					Pin &next = this->pin(off->first);
					if (pin.pos < next.pos and routes[i].offset[Model::PMOS] >= next.lo and routes[i].offset[Model::PMOS] <= next.hi) {
						int pos = routes[i].pins[j].left+off->second;
						if (next.pos < pos) {
							change = true;
							//printf("viaToPin (%d,%d)->%d->(%d,%d) %d->%d\n", i, j, off->second, off->first.type, off->first.pin, next.pos, pos);
							next.pos = pos;
							stack.push_back(off->first);
						}
					}
				}
			}
		}
		sort(stack.rbegin(), stack.rend());
		stack.erase(unique(stack.begin(), stack.end()), stack.end());
	}

	for (int i = 0; i < (int)routes.size(); i++) {
		if (routes[i].net < 0) {
			continue;
		}

		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			Pin &pin = this->pin(routes[i].pins[j].idx);
			for (auto off = routes[i].pins[j].toPin.begin(); off != routes[i].pins[j].toPin.end(); off++) {
				Pin &next = this->pin(off->first);
				if (pin.pos < next.pos and routes[i].offset[Model::PMOS] >= next.lo and routes[i].offset[Model::PMOS] <= next.hi) {
					int pos = next.pos-off->second;
					if (routes[i].pins[j].right > pos) {
						change = true;
						routes[i].pins[j].right = pos;
					}
				}
			}
		}
	}

	for (int i = 0; i < (int)routes.size(); i++) {
		if (routes[i].net >= 0) {
			routes[i].resortPins(this);
		}
	}
	return change;
}

bool Router::alignPins(int maxDist, bool reset) {
	// TODO(edward.bingham) setting reset to false breaks this function
	bool change = false;
	if (reset) {
		for (int type = 0; type < (int)this->stack.size(); type++) {
			for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
				this->stack[type].pins[i].align = -1;
			}
		}
	}

	vector<Alignment> align;
	if (routes.empty()) {
		for (int i = 0; i < (int)ckt->nets.size(); i++) {
			if (ckt->nets[i].isPairedGate()) {
				array<int, 2> ports;
				for (int type = 0; type < 2; type++) {
					for (ports[type] = 0; ports[type] < (int)this->stack[type].pins.size() and (this->stack[type].pins[ports[type]].isContact() or this->stack[type].pins[ports[type]].outNet != i); ports[type]++);
				}

				if (ports[0] < (int)this->stack[0].pins.size() and ports[1] < (int)this->stack[1].pins.size()) {
					align.push_back(Alignment(this, ports[Model::PMOS], ports[Model::NMOS]));
				}
			}

			if (ckt->nets[i].isPairedDriver()) {
				array<int, 2> ports;
				for (int type = 0; type < 2; type++) {
					for (ports[type] = 0; ports[type] < (int)this->stack[type].pins.size() and (this->stack[type].pins[ports[type]].isGate() or this->stack[type].pins[ports[type]].outNet != i); ports[type]++);
				}

				if (ports[0] < (int)this->stack[0].pins.size() and ports[1] < (int)this->stack[1].pins.size()) {
					align.push_back(Alignment(this, ports[Model::PMOS], ports[Model::NMOS]));
				}
			}
		}
	} else {
		array<vector<int>, 2> ends;
		for (int i = 0; i < (int)routes.size(); i++) {
			if (routes[i].net < 0) {
				continue;
			}

			// TODO(edward.bingham) optimize this
			ends[0].clear();
			ends[1].clear();
			for (int j = 0; j < (int)routes[i].pins.size(); j++) {
				const Index &pin = routes[i].pins[j].idx;

				if (pin.type >= 2) {
					continue;
				}

				if ((int)ends[pin.type].size() < 2) {
					ends[pin.type].push_back(pin.pin);
				} else {
					ends[pin.type][1] = pin.pin;
				}
			}

			if ((int)ends[Model::PMOS].size() == 1 and (int)ends[Model::NMOS].size() == 1) {
				align.push_back(Alignment(this, ends[Model::PMOS][0], ends[Model::NMOS][0]));
			}
		}
	}

	int matches = 0;
	while (not align.empty()) {
		sort(align.begin(), align.end(), std::greater<>{});
		bool found = false;
		Alignment curr;
		for (int i = (int)align.size()-1; i >= 0; i--) {
			if (maxDist < 0 or align[i].dist() <= maxDist) {
				curr = align[i];
				align.erase(align.begin()+i);
				found = true;
				break;
			}
		}
		if (not found) {
			break;
		}

		for (int type = 0; type < 2; type++) {
			this->stack[type].pins[curr.pin[type]].align = curr.pin[1-type];
		}
		matches++;

		change = updatePinPos(reset) or change;
		for (int i = (int)align.size()-1; i >= 0; i--) {
			if (align[i].conflictsWith(curr)) {
				align.erase(align.begin()+i);
			}
		}
	}

	return change;
}

// depends on:
// updatePinPos() - contacts on the route need an up-to-date
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
		int i = 0, j = 0;
		while (i < (int)routeConstraints.size() and j < (int)old.size()) {
			auto c = routeConstraints.begin()+i;

			if (c->wires[0] == old[j].wires[0]
				and c->wires[1] == old[j].wires[1]) {
				if (c->select == -1 and not resetOrder) {
					c->select = old[j].select;
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
				if (c->select == -1 and not resetOrder) {
					c->select = 1-old[j].select;
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
					routeConstraints.insert(routeConstraints.begin()+i, old[j]);
					i++;
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
				auto pos = lower_bound(routes[i].pins.begin(), routes[i].pins.end(), Index(type, j), CompareIndex(this));
				if (pos != routes[i].pins.end() and pos != routes[i].pins.begin() and pos->idx != Index(type, j)) {
					pos--;
					int level = routes[i].getLevel(pos-routes[i].pins.begin());
					if (level == this->stack[type].pins[j].layer) {
						groupConstraints.push_back(RouteGroupConstraint(i, Index(type, j)));
					}
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
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			Pin &pin = this->pin(routes[i].pins[j].idx);
			if (routes[i].offset[Model::PMOS] < pin.lo) {
				pin.lo = routes[i].offset[Model::PMOS];
				change = true;
			}
			if (routes[i].offset[Model::PMOS] > pin.hi) {
				pin.hi = routes[i].offset[Model::PMOS];
				change = true;
			}
		}
	}
	return change;
}

bool Router::buildOffsets(int type, vector<int> start) {
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
	for (int i = 0; i < (int)start.size(); i++) {
		tokens.push_back(vector<int>(1, start[i]));
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
					printf("error: buildOffset found cycle {");
					for (int j = 0; j < (int)curr.size(); j++) {
						printf("%d:%s(%d) ", curr[j], curr[j] >= 0 ? ckt->nets[routes[curr[j]].net].name.c_str() : "", routes[curr[j]].net);
					}
					printf("%d:%s(%d)}\n", i->first, i->first >= 0 ? ckt->nets[routes[i->first].net].name.c_str() : "", routes[i->first].net);
				}
			}
		}
	}
	
	buildPinBounds(true);

	/*if (debug and unresolvedCycle[type]) {
		print();
	}*/
	return change;
}

void Router::resetGraph() {
	zeroWeights();
	buildOffsets(Model::PMOS);
	buildOffsets(Model::NMOS);
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
		change = buildOffsets(Model::PMOS, inTokens) or change;
		change = buildOffsets(Model::NMOS, outTokens) or change;
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
			change = buildOffsets(Model::PMOS, inTokens) or change;
			change = buildOffsets(Model::NMOS, outTokens) or change;
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

			change = buildOffsets(Model::PMOS, inTokens) or change;
			change = buildOffsets(Model::NMOS, outTokens) or change;
			continue;
		}
	}
	return change;
}

bool Router::findAndBreakPinCycles() {
	bool change = false;
	vector<vector<int> > cycles;
	while (findCycles(cycles)) {
		breakCycles(cycles);
		cycles.clear();
		change = true;
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
	vector<vector<set<int> > > blockedLevels(routes.size(), vector<set<int> >());
	for (int i = 0; i < (int)routes.size(); i++) {
		for (int type = 0; type < (int)this->stack.size(); type++) {
			for (int j = 0; j < (int)this->stack[type].pins.size(); j++) {
				const Pin &p0 = this->pin(Index(type, j));
				if (routes[i].offset[Model::PMOS] >= this->stack[type].pins[j].lo and routes[i].offset[Model::PMOS] <= this->stack[type].pins[j].hi and not routes[i].pins.empty()) {
					auto pos = lower_bound(routes[i].pins.begin(), routes[i].pins.end(), Index(type, j), CompareIndex(this, false));
					if (((pos == routes[i].pins.begin() and this->pin(routes[i].pins[0].idx).pos - p0.pos <= p0.width) or
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
		/*vector<bool> types = routes[i].pinTypes();
		if ((not types[Model::PMOS] or not types[Model::NMOS]) and not routes[i].hasGate(this)) {
			continue;
		}*/
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			int level = this->pin(routes[i].pins[j].idx).layer;
			if (j+1 < (int)routes[i].pins.size()) {
				level = min(level, this->pin(routes[i].pins[j+1].idx).layer);
			}
			level = max(1, level);
			for (; level < (int)tech->wires.size(); level++) {
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
			routes[i].level[j] = level;
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
		if (this->stack[type].pins.size() > 0 and this->stack[type].pins[0].pos < left) {
			left = this->stack[type].pins[0].pos;
		}
		if (this->stack[type].pins.size() > 0 and this->stack[type].pins.back().pos > right) {
			right = this->stack[type].pins.back().pos;
		}
	}

	//int cellHeightOverhead = 10;
	cost = cellHeight;//(cellHeightOverhead+right-left)*cellHeight*(int)(1+aStar.size());
	return cost;
}

void Router::load(const Placement &place) {
	// Save the resulting placement to the Subckt
	for (int type = 0; type < 2; type++) {
		stack[type].type = type;
		for (auto pin = place.stack[type].begin(); pin != place.stack[type].end(); pin++) {
			stack[type].push(*tech, *ckt, pin->device, pin->flip);
		}
		if (not place.stack[type].empty() and place.stack[type].back().device >= 0) {
			stack[type].push(*tech, *ckt, -1, false);
		}
	}
}

bool Router::solve() {
	buildPins();
	//addIOPins();
	buildRoutes();

	buildPinConstraints(0, true);
	findAndBreakPinCycles();
	drawRoutes();
	buildRouteConstraints(true, true);
	assignRouteConstraints();

	buildHorizConstraints();
	updatePinPos(true);
	alignPins(200, true);

	buildPinConstraints(0, true);
	findAndBreakPinCycles();
	drawRoutes();
	buildRouteConstraints(true);
	assignRouteConstraints();

	alignVirtualPins();
	drawRoutes();
	buildRouteConstraints(true);
	assignRouteConstraints();

	lowerRoutes();
	buildGroupConstraints();

	drawRoutes();
	buildRouteConstraints();
	assignRouteConstraints();
	buildHorizConstraints(true);

	bool change = true;
	for (int i = 0; i < 10 and change; i++) {
		change = false;
		if (updatePinPos(true)) {
			if (debug) printf("updatePinPos()\n");
			//change = true;
		}
		//if (alignPins(200, true)) {
		//	if (debug) printf("alignPins()\n");
		//	change = true;
		//}
		if (buildPinConstraints(0)) {
			if (debug) printf("buildPinConstraints()\n");
			change = true;
		}
		if (findAndBreakPinCycles()) {
			if (debug) printf("findAndBreakPinCycles()\n");
			change = true;
		}
		alignVirtualPins();
		drawRoutes();
		if (buildRouteConstraints()) {
			if (debug) printf("buildRouteConstraints()\n");
			change = true;
		}
		if (assignRouteConstraints()) {
			if (debug) printf("assignRouteConstraints()\n");
			change = true;
		}
		if (buildHorizConstraints()) {
			if (debug) printf("buildHorizConstraints()\n");
			change = true;
		}
	}
	if (change) {
		printf("error: unstable layout\n");
	}
	if (unresolvedCycle[0]) {
		printf("error: unresolved cycle 0\n");
	}
	if (unresolvedCycle[1]) {
		printf("error: unresolved cycle 1\n");
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
	return not change and not unresolvedCycle[0] and not unresolvedCycle[1];
}

void Router::print() {
	printf("NMOS\n");
	for (int i = 0; i < (int)this->stack[0].pins.size(); i++) {
		const Pin &pin = this->stack[0].pins[i];
		printf("pin[%d] dev=%d nets=%s(%d) -> %s(%d) -> %s(%d) size=%dx%d pos=%d align=%d lo=%d hi=%d\n", i, pin.device, ckt->netName(pin.leftNet).c_str(), pin.leftNet, ckt->netName(pin.outNet).c_str(), pin.outNet, ckt->netName(pin.rightNet).c_str(), pin.rightNet, pin.width, pin.height, pin.pos, pin.align, pin.lo, pin.hi);
	}

	printf("\nPMOS\n");
	for (int i = 0; i < (int)this->stack[1].pins.size(); i++) {
		const Pin &pin = this->stack[1].pins[i];
		printf("pin[%d] dev=%d nets=%s(%d) -> %s(%d) -> %s(%d) size=%dx%d pos=%d align=%d lo=%d hi=%d\n", i, pin.device, ckt->netName(pin.leftNet).c_str(), pin.leftNet, ckt->netName(pin.outNet).c_str(), pin.outNet, ckt->netName(pin.rightNet).c_str(), pin.rightNet, pin.width, pin.height, pin.pos, pin.align, pin.lo, pin.hi);
	}

	printf("\nRoutes\n");
	for (int i = 0; i < (int)routes.size(); i++) {
		printf("wire[%d] %s(%d) %d->%d in:%d out:%d: ", i, (routes[i].net >= 0 and routes[i].net < (int)ckt->nets.size() ? ckt->nets[routes[i].net].name.c_str() : ""), routes[i].net, routes[i].left, routes[i].right, routes[i].offset[Model::PMOS], routes[i].offset[Model::NMOS]);
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			printf("(%d,%d) ", routes[i].pins[j].idx.type, routes[i].pins[j].idx.pin);
		}
		printf("\n");
	}

	printf("\nStack Constraints\n");
	for (int type = 0; type < 2; type++) {
		for (int i = 0; i < (int)this->stack[type].pins.size(); i++) {
			for (auto o = this->stack[type].pins[i].toPin.begin(); o != this->stack[type].pins[i].toPin.end(); o++) {
				printf("toPin (%d,%d) -> %d -> (%d,%d)\n", type, i, o->second, o->first.type, o->first.pin);
			}
		}
	}

	for (int i = 0; i < (int)routes.size(); i++) {
		printf("route %d\n", i);
		for (int j = 0; j < (int)routes[i].pins.size(); j++) {
			printf("\t(%d,%d) {", routes[i].pins[j].idx.type, routes[i].pins[j].idx.pin);
			for (auto k = routes[i].pins[j].fromPin.begin(); k != routes[i].pins[j].fromPin.end(); k++) {
				printf("(%d,%d)->%d ", k->first.type, k->first.pin, k->second);
			}
			for (auto k = routes[i].pins[j].toPin.begin(); k != routes[i].pins[j].toPin.end(); k++) {
				printf("%d->(%d,%d) ", k->second, k->first.type, k->first.pin);
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
		printf("route[%d] %d %s %d: %d,%d\n", i, routeConstraints[i].wires[0], (routeConstraints[i].select == 0 ? "->" : (routeConstraints[i].select == 1 ? "<-" : "--")), routeConstraints[i].wires[1], routeConstraints[i].off[0], routeConstraints[i].off[1]);
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
