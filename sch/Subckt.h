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

#include "Segment.h"
#include "Isomorph.h"

#include <common/mapping.h>

using namespace phy;
using namespace std;

namespace sch {

struct Subckt;

// Mos represents one four-terminal Metal Oxide Semiconductor (MOS) transistor in a Subckt.
// It stores the technology model, terminal connections, and device geometry.
// Terminal connections use local net indices for direct access to Subckt::nets.
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
	// nets, index into Subckt::nets. A value of -1 marks an unset terminal. The
	// base terminal is the bulk connection.
	int gate;
	int source;
	int drain;
	int base;

	// SPICE parameter values grouped by parameter name.
	// name of parameter -> list of values for that parameter
	map<string, vector<double> > params;

	vec2i size;  // Channel length and width, in that order.
	vec2i area;  // Diffusion area in drain, source order.
	vec2i perim; // Diffusion perimeter in drain, source order.

	// Optional device comment. Subckt::printMos() prints it before the device.
	string comment;

	// Set the channel size and estimate the diffusion area and perimeter
	// for both sides from the technology rules.
	void setSize(const Tech &tech, vec2i size);

	// Return the net index on the selected side. The default orientation
	// puts the source on the left and the drain on the right.
	// Pass true to use the opposite orientation.
	int left(bool flip = false) const;
	int right(bool flip = false) const;

	// Add m's width, area, and perimeter to this device if the model,
	// gate, base, parameters, and channel length match. The source and
	// drain nets can match in either order. Return true if merged.
	bool combineParallel(const Mos &m);

	// Return a copy with the source and drain net indices exchanged.
	Mos flip() const;
};

bool operator<(const Mos &m0, const Mos &m1);

// Net represents one electrical net in a Subckt. It stores reverse
// connection lists so a traversal can find attached transistors and
// child instances without scanning the full netlist.
struct Net {
	Net();
	Net(string name, bool isIO=false);
	~Net();

	string name;

	// Indices into Subckt::mos, grouped by terminal and transistor type.
	// Slot 0 holds NMOS indices. Slot 1 holds PMOS indices.
	array<vector<int>, 2> gateOf;
	array<vector<int>, 2> sourceOf;
	array<vector<int>, 2> drainOf;
	array<vector<int>, 2> baseOf;

	// Indices into Subckt::inst for child instances with a port on this net.
	vector<int> portOf;

	// Indices into Subckt::nets for this net and nets explicitly linked
	// to it by connectRemote().
	vector<int> remote;

	// Is this net an input or output to the cell? If it is, then we
	// need to draw an IO pin and hook it up to the rest of the net.
	bool isIO; // True if this net is itself an external port of the subcircuit.

	// External-port flag shared by connectRemote(). It starts at isIO.
	// Linking two nets sets both flags if either flag was true.
	bool remoteIO;

	// Count source and drain connections for the specified transistor type.
	int ports(int type) const;

	// Return true if this net needs a diffusion contact for the given type.
	// External I/O, any gate connection, any opposite-type source or drain,
	// or more than two same-type source and drain connections require one.
	bool hasContact(int type) const;

	// Return true if the net connects to one NMOS gate and one PMOS gate.
	bool isPairedGate() const;

	// Return true if each transistor type has one source or drain
	// connection to the net.
	bool isPairedDriver() const;

	// Return true if the net connects to at least one drain.
	bool isOutput() const;

	// Return true if gate connections outnumber source connections.
	bool isInput() const;

	// Return true if the given net index is in remote.
	bool connectedTo(int net) const;

	// Return true if no transistor terminal or child instance port is
	// attached. By default, remoteIO protects the net from this check.
	// Pass true to ignore remoteIO.
	bool dangling(bool remIO=false) const;

	// Return true for an empty name or a name that starts with '#' or '_'
	// and has only decimal digits after that character, if any.
	bool isAnonymous() const;
};

// Instance represents one use of a child subcircuit inside a parent Subckt.
// It refers to the child by name and connects its external ports to parent
// nets by position. Multiple instances can use the same child definition.
struct Instance {
	Instance();
	Instance(std::string type, vector<int> ports=vector<int>());
	Instance(const Subckt &ckt, const Mapping<int> &cktToThis);
	~Instance();

	std::string type; // Name of the child subcircuit definition.
	string name;      // Name of this instance in the parent.

	// Indices into the parent's nets, in the order of the child's
	// external ports. ports[i] connects to the child's port at position i.
	vector<int> ports;

	// Optional comment. Subckt::printInst() prints it before the instance.
	string comment;
};

// Subckt represents the transistor-level netlist of a circuit or cell.
// It stores nets, MOS transistors, and child instances in separate vectors.
// Transistor terminals, child instance pins, and ordered external ports
// refer to local nets by index.
//
// Each net also stores the indices of the transistors and child instances
// connected to it. This lets a traversal start at a net and find its
// connections without scanning all devices and instances.
//
// Canonicalization relabels nets, puts source and drain in a consistent
// order, and sorts the transistors. This gives transistor networks a
// common form for comparison and hashing.
struct Subckt {
	struct PartitionKey {
		// TODO(edward.bingham) need to handle "base"

		// These categories describe how a MOS transistor connects a reference
		// net to a target partition cell. The first letter identifies the
		// terminal on the reference net. Each comment lists the remaining
		// terminals whose nets are in the target cell.
		//
		// S starts at either the source or drain. S2 means the opposite
		// terminal is in the target cell. S1 means it is outside that cell.
		// G starts at the gate. B starts at the base (bulk). For G and B,
		// the digit counts source and drain terminals in the target cell.
		//
		// Scores are summed by category and transistor model. Each S entry
		// adds (1000 * width) / length with integer division. Each G or B
		// entry adds width * length. If no remaining terminal connects to
		// the target cell, the reference terminal contributes no score.

		// For a given transistor model
		enum {
			S2 = 0,   // Opposite source or drain terminal only.
			S2G = 1,  // Opposite source or drain terminal and gate.
			S2B = 2,  // Opposite source or drain terminal and base.
			S2GB = 3, // Opposite source or drain terminal, gate, and base.
			S1G = 4,  // Gate only.
			S1B = 5,  // Base only.
			S1GB = 6, // Gate and base only.
			G1 = 7,   // One source or drain terminal only.
			G1B = 8,  // One source or drain terminal and base.
			G2 = 9,   // Both source and drain only.
			G2B = 10, // Source, drain, and base.
			GB = 11,  // Base only.
			BG = 12,  // Gate only.
			B1G = 13, // One source or drain terminal and gate.
			B2G = 14, // Source, drain, and gate.
			B1 = 15,  // One source or drain terminal only.
			B2 = 16   // Both source and drain only.
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

	string name;    // Subcircuit name. Child instances of this subcircuit use it as their type.
	string comment; // Optional comment for the subcircuit. print() writes it before the netlist.
	bool isCell;    // Cell flag supplied by the caller. The constructor defaults it to false.

	// Hash of the transistor network computed by canonicalize().
	// The constructor initializes it to size_t(-1).
	size_t id;

	// Ordered indices into nets for the external ports. Each position
	// corresponds to the same position in an instance's port list.
	vector<int> ports;

	// Local nets. Transistor terminals, instance pins, and external ports
	// refer to these nets by index.
	vector<Net> nets;

	// MOS transistors in this subcircuit. Each net lists the indices of its
	// connected transistors by terminal and transistor type.
	vector<Mos> mos;

	// Child subcircuit instances. Each instance stores its type name and
	// the local net index for each pin, in port order.
	vector<Instance> inst;

	int createNet(string name);      // Return the index of the named net. Add the net if the name is new.
	int netIndex(string name) const; // Return the first net index with this name, or -1 if no net matches.
	string netAt(int net) const;     // Return the name at a valid net index. Return "_" for a negative index.

	// Append a net, add its own index to its remote list, and register it
	// as an external port if isIO is true. Return the new net index.
	int push(Net n);

	// Append a transistor and register its terminals in the reverse
	// connection lists of their nets and linked nets. Return its index.
	int push(Mos m);

	// Append a child instance, assign its name from its index, and register
	// it in the reverse connection lists of its port nets and linked nets.
	void push(Instance ckt);

	// Remove the net and shift references to later net indices. Remove its
	// external port and remote entries. Set transistor terminals and
	// instance pins that referred to it to -1.
	void popNet(int index);

	// Remove the transistor and update the reverse connection lists.
	void popMos(int index);

	// Replace the type name of each child instance whose type matches from.
	void renameType(std::string from, std::string to);

	// Link the two nets in both directions. Give both nets the union of
	// their reverse connection lists and set remoteIO if either flag is true.
	void connectRemote(int n0, int n1);

	// Remove the segment's transistors from this subcircuit. Supply their
	// indices in ascending order so removal can start at the highest index.
	void extract(const Segment &m);

	// Remove nets with no transistor terminal or child instance port
	// connections. Pass true to include nets protected by remoteIO.
	void cleanDangling(bool remIO=false);

	// Starting at net, collect transistors from drain to source. Continue
	// through anonymous source nets. Return sorted, unique transistor
	// indices and add each visited net to covered if supplied.
	Segment segment(int net, set<int> *covered=nullptr);

	// Build segments from nets with drain connections. Start with named
	// nets, then use uncovered nets without source connections, then any
	// remaining uncovered nets with drain connections. Merge overlapping
	// or cross-coupled segments, then try to merge similar segments.
	vector<Segment> segment();

	// Return true if each segment has a drain net that connects to a
	// gate, source, or base terminal in the other segment.
	bool areCoupled(const Segment &m0, const Segment &m1) const;

	// Return true if a and b connect to the same set of other segments
	// and both have the same external-port status.
	bool areSimilar(const vector<Segment> &segs, int a, int b) const;

	// Merge compatible parallel transistors and remove the redundant entries.
	void combineDevices();

	// Split transistors that exceed the width limit of their model bin
	// into parallel devices. Use the technology rules to select split widths.
	void splitDevices(const Tech &tech);

	// Reorder nets by the old-to-new mapping and update net references
	// in external ports, transistor terminals, remote links, and instance pins.
	void apply(const Mapping<int> &m);

	// Apply canonical net labels, order each transistor's source and drain
	// by net index, and sort the transistors. Rebuild their reverse
	// connection lists, update id, and return the old-to-new net mapping.
	Mapping<int> canonicalize();

	// Compare net counts first, then compare the transistor lists in net
	// index order. Return -1, 0, or 1. Canonicalize both circuits first
	// when the comparison must use canonical labels.
	int compare(const Subckt &ckt) const;

	// Map subcircuit net indices to layout net indices by matching names.
	Mapping<int> mapToLayout(const Layout &layout) const;

	// Return one key per cell in beta, in partition order. Each key scores
	// the transistor connections from v to that cell.
	vector<PartitionKey> createPartitionKey(int v, const Partition &beta) const;

	// Score the transistor connections from the nets in c0 to the nets
	// in c1. Return an empty key if both cells contain the same single net.
	PartitionKey lambda(const Partition::Cell &c0, const Partition::Cell &c1) const;

	// Sort a copy of the partition cells by size, then by first net index.
	// Return keys for each cell paired with itself, followed by keys for
	// all ordered pairs of distinct cells.
	vector<PartitionKey> lambda(Partition pi) const;

	// Compare the transistor order under two complete partitions with one
	// net per cell. Use cell positions as net labels. Return -1, 0, or 1.
	int comparePartitions(const Partition &pi0, const Partition &pi1) const;

	// Return the number of net vertices used for canonical labeling.
	int verts() const;


	void printNet(int i) const;
	void printMos(int i) const;
	void printInst(int i) const;
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
