#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <unordered_set>
#include <limits>
#include <algorithm>

#include <phy/Tech.h>
#include <phy/Layout.h>
#include <phy/vector.h>

#include "Mapping.h"
#include "Isomorph.h"

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

	vector<int> remote;

	// Is this net an input or output to the cell? If it is, then we need to draw
	// an IO pin and hook it up to the rest of the net.
	bool isIO;
	bool remoteIO;

	int ports(int type) const;

	bool hasContact(int type) const;
	bool isPairedGate() const;
	bool isPairedDriver() const;
	bool isOutput() const;
	bool isInput() const;
	bool connectedTo(int net) const;

	bool dangling(bool remIO=false) const;
	bool isAnonymous() const;
};

struct Instance {
	Instance();
	Instance(const Subckt &ckt, const Mapping &m, int subckt);
	~Instance();

	int subckt;
	vector<int> ports;
};

struct Subckt {
	Subckt(bool isCell=false);
	~Subckt();

	// Name of this cell
	string name;
	bool isCell;
	size_t id;

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
	void connectRemote(int n0, int n1);
	int pushMos(int model, int type, int drain, int gate, int source, int base=-1, vec2i size=vec2i(1.0,1.0));
	void popMos(int index);
	void pushInst(Instance ckt);

	void extract(const Segment &m);
	void cleanDangling(bool remIO=false);

	Segment segment(int net, set<int> *covered=nullptr);
	vector<Segment> segment();
	bool areCoupled(const Segment &m0, const Segment &m1) const;

	void apply(const Mapping &m);
	Mapping canonicalize();
	int compare(const Subckt &ckt) const;


	vector<vector<int> > createPartitionKey(int v, const Partition &beta) const;
	array<int, 4> lambda(const Partition::Cell &c0, const Partition::Cell &c1) const;
	vector<array<int, 4> > lambda(Partition pi) const;
	int comparePartitions(const Partition &pi0, const Partition &pi1) const;
	int verts() const;


	void printNet(int i) const;
	void printMos(int i) const;
	void print() const;
};

bool operator==(const Subckt &c0, const Subckt &c1);
bool operator!=(const Subckt &c0, const Subckt &c1);
bool operator<(const Subckt &c0, const Subckt &c1);
bool operator>(const Subckt &c0, const Subckt &c1);
bool operator<=(const Subckt &c0, const Subckt &c1);
bool operator>=(const Subckt &c0, const Subckt &c1);

}

template<>
struct std::hash<sch::Subckt> {
	void appendHash(std::size_t &h0, std::size_t h1) const {
    h0 ^= (h1 + 0x9e3779b9 + (h0<<6) + (h0>>2));
	}

	std::size_t operator()(const sch::Subckt &ckt) const noexcept {
		std::size_t result = std::hash<size_t>{}(ckt.nets.size());

		for (int i = 0; i < (int)ckt.nets.size(); i++) {
			appendHash(result, std::hash<int>{}(i));
			for (int type = 0; type < 2; type++) {
				appendHash(result, std::hash<int>{}(type));
				appendHash(result, std::hash<size_t>{}(ckt.nets[i].sourceOf[type].size()));

				vector<int> g0;
				for (auto j = ckt.nets[i].sourceOf[type].begin(); j != ckt.nets[i].sourceOf[type].end(); j++) {
					g0.push_back(ckt.mos[*j].drain);
				}
				std::sort(g0.begin(), g0.end());

				for (int j = 0; j < (int)g0.size(); j++) {
					appendHash(result, std::hash<int>{}(g0[j]));
				}
			}
		}
		return result;
	}
};
