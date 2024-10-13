#pragma once

#include <vector>
#include <stdint.h>

using namespace std;

namespace sch {

struct bitset {
	vector<uint64_t> data;

	void set(int i, bool v);
	bool get(int i) const;

	bitset &operator=(const bitset &b);
	bitset &operator|=(const bitset &b);
	bitset &operator&=(const bitset &b);

	bool empty() const;
};

}

