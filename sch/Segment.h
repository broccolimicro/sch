#pragma once

#include <string>
#include <vector>

#include <common/mapping.h>

using namespace std;

namespace sch {

struct Subckt;

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

	Mapping<int> generate(Subckt &dst, const Subckt &src) const;
	bool contains(int i) const;

	void print() const;
};

}
