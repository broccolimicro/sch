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
	int source;
	int drain;
	int bulk;

	// loaded in from spice
	// name of parameter -> list of values for that parameter
	map<string, vector<double> > params;
	vec2i size; // [length, width] of the transistor

	int left(bool flip = false) const;
	int right(bool flip = false) const;
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

struct Subckt {
	Subckt();
	~Subckt();

	// Name of this cell
	string name;

	// These are loaded directly from the spice file It's the list of nets and
	// their connections to transistors.
	vector<Net> nets;
	vector<Mos> mos;

	int findNet(string name, bool create=false);
	string netName(int net) const;
};

}
