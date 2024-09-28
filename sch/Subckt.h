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
	Mos(int model, int type, int drain, int gate, int source, int base=-1, vec2i size=vec2i(1.0,1.0));
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
	int source;
	int drain;
	int base;

	// loaded in from spice
	// name of parameter -> list of values for that parameter
	map<string, vector<double> > params;
	vec2i size; // [length, width] of the transistor

	int left(bool flip = false) const;
	int right(bool flip = false) const;
};

bool operator==(const Mos &m0, const Mos &m1);
bool operator!=(const Mos &m0, const Mos &m1);

// This structure represents a single variable/net.
struct Net {
	Net();
	Net(string name, bool isIO=false);
	~Net();

	string name;

	// Number of [NMOS, PMOS] gates that this net is connected to
	array<vector<int>, 2> gateOf;

	// Number of [NMOS, PMOS] sources or drains that this net is connected to
	array<vector<int>, 2> sourceOf;
	array<vector<int>, 2> drainOf;

	// index into Subckt::inst
	vector<int> portOf;

	// Is this net an input or output to the cell? If it is, then we need to draw
	// an IO pin and hook it up to the rest of the net.
	bool isIO;

	int ports(int type) const;

	bool hasContact(int type) const;
	bool isPairedGate() const;
	bool isPairedDriver() const;

	bool dangling(bool remIO=false) const;
	bool isAnonymous() const;
};

struct Instance {
	int subckt;
	vector<int> ports;
};

struct Mapping {
	Mapping();
	Mapping(const Subckt *cell, int index);
	~Mapping();

	int index;
	const Subckt *cell;
	vector<int> cellToThis; // nets
	vector<int> devices; // devs in this

	Subckt generate(const Subckt &main);

	bool apply(const Mapping &m);
	Instance instance() const;

	bool overlapsWith(const Mapping &m) const;
};

struct Subckt {
	Subckt();
	~Subckt();

	// Name of this cell
	string name;
	bool isCell;

	vector<int> ports;

	// These are loaded directly from the spice file It's the list of nets and
	// their connections to transistors.
	vector<Net> nets;
	vector<Mos> mos;
	vector<Instance> inst;

	int findNet(string name, bool create=false);
	string netName(int net) const;

	int pushNet(string name, bool isIO=false);
	void popNet(int index);
	int pushMos(int model, int type, int drain, int gate, int source, int base=-1, vec2i size=vec2i(1.0,1.0));
	void popMos(int index);

	vector<Mapping> find(const Subckt &cell, int index);
	void apply(const Mapping &m);
	void cleanDangling(bool remIO=false);

	Mapping segment(int net);
	vector<Subckt> generateCells(int start);
};

}
