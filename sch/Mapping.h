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
	Mapping(const Subckt &ckt);
	Mapping(vector<int> nets);
	~Mapping();

	// list of nets from old subckt to include in new subckt
	vector<int> nets;
	
	void identity(const Subckt &ckt);
	void apply(const Mapping &m);

	void print() const;
};

struct Segment {
	Segment();
	Segment(const Subckt &ckt);
	Segment(vector<int> mos);
	~Segment();

	// list of devices from old subckt to include in new subckt
	vector<int> mos;

	void identity(const Subckt &ckt);
	bool extract(const Segment &seg);
	void merge(const Segment &seg);

	bool overlapsWith(const Segment &seg) const;

	Mapping map(const Subckt &ckt) const;	
	Mapping generate(Subckt &dst, const Subckt &src) const;

	void print() const;
};

}
