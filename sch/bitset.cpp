#include "bitset.h"

namespace sch {

void bitset::set(int i, bool v) {
	int idx = ((i+63)/64);
	int bit = i%64;
	if (idx >= (int)data.size()) {
		if (not v) {
			return;
		}
		data.resize(idx+1, 0);
	}

	if (v) {
		data[idx] |= (1<<bit);
	} else {
		data[idx] &= ~(1<<bit);
	}
}

bool bitset::get(int i) {
	int idx = ((i+63)/64);
	int bit = i%64;
	if (idx >= (int)data.size()) {
		return false;
	}

	return (data[idx]>>bit)&1;
}

bitset &bitset::operator=(const bitset &b) {
	data = b.data;
	return *this;
}

bitset &bitset::operator|=(const bitset &b) {
	if (b.data.size() > data.size()) {
		data.resize(b.data.size(), 0);
	}

	for (int i = 0; i < (int)b.data.size(); i++) {
		data[i] |= b.data[i];
	}
	return *this;
}

bitset &bitset::operator&=(const bitset &b) {
	if (data.size() > b.data.size()) {
		data.resize(b.data.size());
	}

	for (int i = 0; i < (int)data.size(); i++) {
		data[i] &= b.data[i];
	}
	return *this;
}

}

