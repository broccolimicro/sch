#pragma once

#include <map>
#include <string>
#include <vector>
#include <unordered_set>
#include <limits>

using namespace std;

namespace sch {

struct Subckt;

struct Mapping {
	Mapping();
	Mapping(const Subckt &cell);
	~Mapping();

	// cellToThis[n0] = n1
	// n0 is index of net in new subckt
	// n1 is index of net in old subckt
	vector<int> cellToThis; // nets

	// list of devices from old subckt to include in new subckt
	vector<int> devices; // devs in this
	
	int pushNet(int net);

	void identity(const Subckt &from);
	void canonical(const Subckt &from);
	bool extract(const Mapping &m);
	void apply(const Mapping &m);
	void merge(const Mapping &m);

	bool overlapsWith(const Mapping &m) const;

	Subckt generate(const Subckt &from, string name, bool isCell=true) const;

	void print() const;
};

}
