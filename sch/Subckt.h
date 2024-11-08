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
	Mos(int model, int type, int drain, int gate, int source, int base=-1);
	Mos(const Tech &tech, int model, int type, int drain, int gate, int source, int base, vec2i size);
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
	vec2i area; // [drain, source]
	vec2i perim; // [drain, source]

	void setSize(const Tech &tech, vec2i size);
	int left(bool flip = false) const;
	int right(bool flip = false) const;

	bool combineParallel(const Mos &m);

	Mos flip() const;
};

bool operator<(const Mos &m0, const Mos &m1);

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
	array<vector<int>, 2> baseOf;

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
	struct PartitionKey {
		// TODO(edward.bingham) need to handle "base"

		// For a given transistor model
		enum {
			S2=0, // total gate width/length from net to cell for gates not in cell
			S2G=1, // total gate width/length from net to cell for gates in cell
			S2B=2,
			S2GB=3,
			S1G=4,
			S1B=5,
			S1GB=6,
			G1=7,
			G1B=8,
			G2=9, // total area from net to gates in cell
			G2B=10,
			GB=11,
			BG=12,
			B1G=13,
			B2G=14,
			B1=15,
			B2=16
		};

		// key type, model -> score
		map<pair<int, int>, int> scores;

		void add(int key, int model, int score);
		void addS(bool hasOther, bool hasGate, bool hasBase, int model, int score);
		void addG(int sdCount, bool hasBase, int model, int score);
		void addB(int sdCount, bool hasGate, int model, int score);
	};

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

	int push(Net n);
	int push(Mos m);
	void push(Instance ckt);
	void popNet(int index);
	void popMos(int index);
	
	void connectRemote(int n0, int n1);

	void extract(const Segment &m);
	void cleanDangling(bool remIO=false);

	Segment segment(int net, set<int> *covered=nullptr);
	vector<Segment> segment();
	bool areCoupled(const Segment &m0, const Segment &m1) const;

	void combineDevices();
	void splitDevices(const Tech &tech);

	void apply(const Mapping &m);
	Mapping canonicalize();
	int compare(const Subckt &ckt) const;


	vector<PartitionKey> createPartitionKey(int v, const Partition &beta) const;
	PartitionKey lambda(const Partition::Cell &c0, const Partition::Cell &c1) const;
	vector<PartitionKey> lambda(Partition pi) const;
	int comparePartitions(const Partition &pi0, const Partition &pi1) const;
	int verts() const;


	void printNet(int i) const;
	void printMos(int i) const;
	void print() const;
};

bool operator==(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1);
bool operator!=(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1);
bool operator<(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1);
bool operator>(const Subckt::PartitionKey &k0, const Subckt::PartitionKey &k1);

bool operator==(const Subckt &c0, const Subckt &c1);
bool operator!=(const Subckt &c0, const Subckt &c1);
bool operator<(const Subckt &c0, const Subckt &c1);
bool operator>(const Subckt &c0, const Subckt &c1);
bool operator<=(const Subckt &c0, const Subckt &c1);
bool operator>=(const Subckt &c0, const Subckt &c1);

}

template<>
struct std::hash<sch::Mos> {
	void appendHash(std::size_t &h0, std::size_t h1) const {
    h0 ^= (h1 + 0x9e3779b9 + (h0<<6) + (h0>>2));
	}

	std::size_t operator()(const sch::Mos &mos) const noexcept {
		std::size_t result = std::hash<size_t>{}(mos.model);
		appendHash(result, std::hash<int>{}(mos.gate));
		appendHash(result, std::hash<int>{}(mos.source));
		appendHash(result, std::hash<int>{}(mos.drain));
		appendHash(result, std::hash<int>{}(mos.base));
		appendHash(result, std::hash<int>{}(mos.size[0]));
		appendHash(result, std::hash<int>{}(mos.size[1]));
		return result;
	}
};

template<>
struct std::hash<sch::Subckt> {
	void appendHash(std::size_t &h0, std::size_t h1) const {
    h0 ^= (h1 + 0x9e3779b9 + (h0<<6) + (h0>>2));
	}

	std::size_t operator()(const sch::Subckt &ckt) const noexcept {
		std::size_t result = std::hash<size_t>{}(ckt.nets.size());

		set<int> s0;
		vector<sch::Mos> m0;
		for (int i = 0; i < (int)ckt.nets.size(); i++) {
			appendHash(result, std::hash<int>{}(i));
			for (int type = 0; type < 2; type++) {
				for (auto j = ckt.nets[i].sourceOf[type].begin(); j != ckt.nets[i].sourceOf[type].end(); j++) {
					if (s0.find(*j) == s0.end()) {
						s0.insert(*j);
						m0.push_back(ckt.mos[*j]);
					}
				}
				for (auto j = ckt.nets[i].drainOf[type].begin(); j != ckt.nets[i].drainOf[type].end(); j++) {
					if (s0.find(*j) == s0.end()) {
						s0.insert(*j);
						m0.push_back(ckt.mos[*j]);
						std::swap(m0.back().source, m0.back().drain);
					}
				}
				std::sort(m0.begin(), m0.end());

				appendHash(result, std::hash<int>{}(type));
				appendHash(result, std::hash<size_t>{}(m0.size()));
				for (int j = 0; j < (int)m0.size(); j++) {
					appendHash(result, std::hash<sch::Mos>{}(m0[j]));
				}
				m0.clear();
			}
		}
		return result;
	}
};
