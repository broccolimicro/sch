#pragma once

#include <map>
#include <string>
#include <vector>
#include <unordered_set>

#include <phy/Tech.h>
#include <phy/Layout.h>
#include <phy/vector.h>

using namespace phy;
using namespace std;

namespace sch {

struct Subckt;

// This structure represents a single transistor (Metal Oxide Semiconductor (MOS)) in the cell.
struct Mos {
	Mos();
	Mos(int model, int type);
	~Mos();

	// Technologies often have multiple NMOS or PMOS transistor models. This
	// points to the specific model for this transistor
	// index into Tech::models
	int model;

	// derived from model
	// Model::NMOS or Model::PMOS
	int type;

	// A transistor is a four terminal device. These integers reference specific
	// nets, index into Subckt::nets.
	int gate;
	vector<int> ports; // source, drain
	int bulk;

	// loaded in from spice
	// name of parameter -> list of values for that parameter
	map<string, vector<double> > params;
	vec2i size; // [length, width] of the transistor
};

// This structure represents a single variable/net.
struct Net {
	Net();
	Net(string name, bool isIO=false);
	~Net();

	string name;

	// Number of [NMOS, PMOS] gates that this net is connected to
	array<int, 2> gates;

	// Number of [NMOS, PMOS] sources or drains that this net is connected to
	array<int, 2> ports;

	// Is this net an input or output to the cell? If it is, then we need to draw
	// an IO pin and hook it up to the rest of the net.
	bool isIO;
};

// This structure points to a single pin in the cell. See the Pin structure
// below.
struct Index {
	Index();
	Index(int type, int pin);
	~Index();

	// index into Subckt::stack (Model::NMOS or Model::PMOS)
	int type;

	// index into Subckt::stack[type].pins, pin number from left to right
	int pin;
};

bool operator<(const Index &i0, const Index &i1);
bool operator==(const Index &i0, const Index &i1);
bool operator!=(const Index &i0, const Index &i1);

// A Pin represents either a gate of a transistor or a source/drain connection
// (called contacts). The list of Pins in a cell is given to us by the placer.
// Pins are vertical paths through the layout, and Wires are horizontal paths
// through the layout.
struct Pin {
	Pin(const Tech &tech);
	// Construct a "contact" pin.
	Pin(const Tech &tech, int outNet);
	// Construct a "gate" pin
	Pin(const Tech &tech, int device, int outNet, int leftNet, int rightNet);
	~Pin();

	// inNet == outNet == gateNet for contacts
	// inNet and outNet represent source and drain depending on Placer::Device::flip
	// These index into Subckt::nets
	int leftNet;
	int outNet;
	int rightNet;

	// index into Subckt::mos for gates of transistors
	// equal to -1 for contacts
	int device;

	//-------------------------------
	// Layout Information
	//-------------------------------
	// This is the actual geometry for this pin. See phy/Layout.h
	// This is generated by Draw::drawPin()
	Layout layout;
	// Routing level (this is mis-named, should be "level")
	// index into Tech::wires, See phy/Tech.h
	int layer;

	// dimensions of the pin in dbunits. See phy/Tech.h
	int width;
	int height;

	// index of the pin in the opposite stack that this pin is aligned to.
	// index into Subckt::stack[1-Subckt::mos[this->device].type].pins
	int align;

	// minimum offset from other pins following spacing rules
	// <from, offset>
	// |==|==|
	//  ---->
	//  ->
	map<Index, int> toPin;
	// current absolute position, computed from off, pin alignment, via constraints
	int pos;

	// These help detect minimum spacing violations between pins and vias on
	// other wires.
	// minimum and maximum Y coordinate (vertical) to which this pin extends in
	// the current iteration of the layout
	int lo;
	int hi;

	void offsetToPin(Index pin, int value);

	bool isGate() const;
	bool isContact() const;
};

// This structure represents a via connection between a Pin and a Wire. It is
// stored in Wire::pins.
struct Contact {
	Contact(const Tech &tech);
	Contact(const Tech &tech, Index idx);
	~Contact();

	// index of the pin this contact connects to	
	Index idx;
	// the minimum and maximum X coordinate (horizontal) of this pin in the
	// layout
	int left;
	int right;

	// This is the actual geometry for this via. See phy/Layout.h
	// This is generated by Draw::drawViaStack()
	Layout layout;

	// Minimum required spacing from other pins to the left of
	// this via to this via. "from" indexes into Subckt::stack, "offset" is
	// distance in dbunits. This is computed by offsetFromPin()
	// <from, offset>
	// |==|==|
	//       O
	//  ---->
	//     ->
	map<Index, int> fromPin;

	// Minimum required spacing from other pins to the left of
	// this via to this via. "from" indexes into Subckt::stack, "offset" is
	// distance in dbunits. This is computed by offsetToPin()
	// <to, offset>
	// |==|==|
	// O
	//  ---->
	//  ->
	map<Index, int> toPin;

	void offsetFromPin(Index Pin, int value);
	void offsetToPin(Index Pin, int value);
};

bool operator<(const Contact &c0, const Contact &c1);
bool operator==(const Contact &c0, const Contact &c1);
bool operator!=(const Contact &c0, const Contact &c1);

// DESIGN(edward.bingham) use this to keep Wire::pins sorted
// This isn't a particularly integral structure in the Router or Placer. This
// is just a helper to keep the vias in Wire::pins sorted from left to right.
struct CompareIndex {
	CompareIndex(const Subckt *s, bool orderIndex = true);
	~CompareIndex();

	const Subckt *s;
	bool orderIndex;

	bool operator()(const Index &i0, const Index &i1);
	bool operator()(const Contact &c0, const Index &i1);
	bool operator()(const Contact &c0, const Contact &c1);
};

// This structure represents a horizontal path connecting some number of pins.
// Pins represent vertical paths connecting some number of wires. Stacks of
// transistors (pull up or pull down stacks) are also represented by this
// structure to abstract away special cases.
struct Wire {
	Wire(const Tech &tech);
	Wire(const Tech &tech, int net);
	~Wire();

	// If this positive, then this indexes into Subckt::nets
	// If this is negative, then flip(net) indexes into Subckt::stack to represent a stack.
	int net;

	// index into Subckt::stack
	// DESIGN(edward.bingham) We should always keep this array sorted based on
	// horizontal location of the pin in the cell from left to right. This helps
	// us pick pins to dogleg when breaking cycles. See CompareIndex
	vector<Contact> pins;

	// Indexes into Tech::wires, see phy/Tech.h
	// This represents the routing level (poly, local interconnect, metal 1,
	// metal 2, etc) on which the segment of this wire between two pins is
	// routed. For level[i], this is the segment between pins[i] and pins[i+1].
	// This is largely computed by lowerRoutes() in the Router.
	vector<int> level;

	// The minimum and maximum X coordinate (horizontal) of this wire in the
	// layout. This is used to choose how to break a route to fix a cycle
	int left;
	int right;

	//-------------------------------
	// Layout Information
	//-------------------------------
	// This is the actual geometry for this wire. See phy/Layout.h
	// This is generated by Draw::drawWire()
	Layout layout;

	// The layout of a cell is drawn such that the PMOS stack is on bottom and
	// the NMOS stack is on top. The coordinate system for pOffset starts from
	// the bottom edge of the PMOS stack at 0 and increases as we approach the
	// NMOS stack. The coordinate system for the nOffset starts from the top edge
	// of the NMOS stack at 0 and increases as we approach the PMOS stack. When
	// the drawing is complete at the end, we flip the geometry vertically before
	// saving it to the GDS file.

	// The vertical distance from the PMOS stack to this wire.
	int pOffset;
	// The vertical distance from the NMOS stack to this wire.
	int nOffset;

	// This is used to detect cycles in the routing graph.
	// index into Subckt::routes
	unordered_set<int> prevNodes;

	void addPin(const Subckt *s, Index pin);
	bool hasPin(const Subckt *s, Index pin, vector<Contact>::iterator *out = nullptr);
	void resortPins(const Subckt *s);
	int getLevel(int i) const;
	bool hasPrev(int r) const;
	bool hasGate(const Subckt *s) const;
	vector<bool> pinTypes() const;
};

// This structure keeps track of all of the pins in the pull up or pull down
// stack. It is also used to represent new vertical paths that need to be
// created to resolve cyclic dependencies in the routing graph.
struct Stack {
	Stack();
	Stack(int type);
	~Stack();

	// The type of devices in this stack.
	// Model::NMOS (0), Model::PMOS (1), or 2 for virtual pins
	int type;
	// The list of pins in this stack.
	vector<Pin> pins;
	// index into Subckt::routes. Which wire represents this stack in the
	// routing problem
	int route;
	
	void push(const Subckt *ckt, int device, bool flip);
	void draw(const Tech &tech, const Subckt *base, Layout &dst);
};

struct Subckt {
	Subckt(const Tech &tech);
	~Subckt();

	const Tech &tech;

	// Name of this cell
	string name;

	// These are loaded directly from the spice file It's the list of nets and
	// their connections to transistors.
	vector<Net> nets;
	vector<Mos> mos;

	// Computed by the placement system
	// stack[0] is Model::NMOS
	// stack[1] is Model::PMOS
	// stack[2] is used for virtual pins that facilitate dogleg routing
	array<Stack, 3> stack;

	// Computed by the routing system
	vector<Wire> routes;

	// Final computed cell height for potential further iterations. (not currently used)
	int cellHeight;

	int findNet(string name, bool create=false);
	string netName(int net) const;
	const Pin &pin(Index i) const;
	Pin &pin(Index i);

	int pinWidth(Index i) const;
	int pinHeight(Index i) const;

	void draw(Layout &dst);
};

}