#include <limits>
#include <algorithm>
#include <string>
#include <set>

#include "Mapping.h"
#include "Subckt.h"

using namespace std;

namespace sch {

Mapping::Mapping() {
}

Mapping::Mapping(const Subckt &ckt) {
	identity(ckt);
}

Mapping::Mapping(vector<int> nets) {
	this->nets = nets;
}

Mapping::~Mapping() {
}

void Mapping::identity(const Subckt &ckt) {
	nets.clear();

	nets.reserve(ckt.nets.size());
	for (int i = 0; i < (int)ckt.nets.size(); i++) {
		nets.push_back(i);
	}
}

void Mapping::apply(const Mapping &m) {
	// this: cell -> main
	// m: canon -> cell
	// want: canon -> main

	vector<int> updated;
	updated.reserve(m.nets.size());
	for (int i = 0; i < (int)m.nets.size(); i++) {
		updated.push_back(nets[m.nets[i]]);
	}
	nets = updated;
}

void Mapping::print() const {
	printf("map{");
	for (int i = 0; i < (int)nets.size(); i++) {
		if (i != 0) {
			printf(", ");
		}
		printf("%d -> %d", i, nets[i]);
	}
	printf("}\n");
}

Segment::Segment() {
}

Segment::Segment(const Subckt &ckt) {
	identity(ckt);
}

Segment::Segment(vector<int> mos) {
	this->mos = mos;
}

Segment::~Segment() {
}

void Segment::identity(const Subckt &ckt) {
	mos.clear();

	mos.reserve(ckt.mos.size());
	for (int i = 0; i < (int)ckt.mos.size(); i++) {
		mos.push_back(i);
	}
}

bool Segment::extract(const Segment &seg) {
	bool success = true;
	vector<int> remove = seg.mos;
	if (not is_sorted(remove.begin(), remove.end()) or
		not is_sorted(mos.begin(), mos.end())) {
		printf("violated sorting assumption\n");
		sort(remove.begin(), remove.end());
		sort(mos.begin(), mos.end());
	}
	for (int i = (int)remove.size()-1; i >= 0; i--) {
		for (int j = (int)mos.size()-1; j >= 0 and mos[j] >= remove[i]; j--) {
			if (mos[j] == remove[i]) {
				mos.erase(mos.begin()+j);
				success = false;
			} else {
				mos[j]--;
			}
		}
	}

	return success;
}

void Segment::merge(const Segment &seg) {
	mos.insert(mos.end(), seg.mos.begin(), seg.mos.end());
	sort(mos.begin(), mos.end());
	mos.erase(unique(mos.begin(), mos.end()), mos.end());
}

bool Segment::overlapsWith(const Segment &seg) const {
	int i = 0, j = 0;
	while (i < (int)mos.size() and j < (int)seg.mos.size()) {
		if (mos[i] == seg.mos[j]) {
			return true;
		} else if (mos[i] < seg.mos[j]) {
			i++;
		} else {
			j++;
		}
	}
	return false;
}

Mapping Segment::map(const Subckt &ckt) const {
	Mapping result;
	for (auto i = mos.begin(); i != mos.end(); i++) {
		result.nets.push_back(ckt.mos[*i].drain);
		result.nets.push_back(ckt.mos[*i].gate);
		result.nets.push_back(ckt.mos[*i].source);
		result.nets.push_back(ckt.mos[*i].base);
	}
	sort(result.nets.begin(), result.nets.end());
	result.nets.erase(unique(result.nets.begin(), result.nets.end()), result.nets.end());
	return result;
}

Mapping Segment::generate(Subckt &dst, const Subckt &src) const {
	Mapping m0 = map(src), m1;
	for (auto i = m0.nets.begin(); i != m0.nets.end(); i++) {
		auto n = src.nets.begin()+*i;

		bool isIO = n->isIO or not n->portOf.empty();
		for (int type = 0; type < 2 and not isIO; type++) {
			for (auto j = n->gateOf[type].begin(); j != n->gateOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos != mos.end() and *pos == *j);
			}
			for (auto j = n->sourceOf[type].begin(); j != n->sourceOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos != mos.end() and *pos == *j);
			}
			for (auto j = n->drainOf[type].begin(); j != n->drainOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos != mos.end() and *pos == *j);
			}
		}

		int j = dst.pushNet(n->name, isIO);
		if (j >= (int)m1.nets.size()) {
			m1.nets.resize(j+1, -1);
		}
		m1.nets[j] = *i;
	}

	for (auto i = mos.begin(); i != mos.end(); i++) {
		auto d = src.mos.begin()+*i;
		int gate = -1, source = -1, drain = -1, base = -1;
		for (int j = 0; j < (int)m1.nets.size(); j++) {
			if (d->gate == m1.nets[j]) {
				gate = j;
			}
			if (d->source == m1.nets[j]) {
				source = j;
			}
			if (d->drain == m1.nets[j]) {
				drain = j;
			}
			if (d->base == m1.nets[j]) {
				base = j;
			}
		}

		if (gate < 0 or source < 0 or drain < 0 or base < 0) {
			printf("internal %s:%d: cell net map missing nets\n", __FILE__, __LINE__);
		}

		dst.pushMos(d->model, d->type, drain, gate, source, base);
		dst.mos.back().size = d->size;
		dst.mos.back().area = d->area;
		dst.mos.back().perim = d->perim;
		dst.mos.back().params = d->params;
	}

	for (int i = 0; i < (int)m1.nets.size(); i++) {
		if (m1.nets[i] >= 0) {
			if (dst.nets[i].isIO and dst.nets[i].isOutput()) {
				dst.nets[i].name = "o" + to_string(i);
			} else if (dst.nets[i].isIO and dst.nets[i].isInput()) {
				dst.nets[i].name = "i" + to_string(i);
			} else if (not dst.nets[i].isIO) {
				dst.nets[i].name = "_" + to_string(i);
			}
		}
	}

	return m1;
}

void Segment::print() const {
	printf("segment{");
	for (int i = 0; i < (int)mos.size(); i++) {
		if (i != 0) {
			printf(", ");
		}
		printf("%d", mos[i]);
	}
	printf("}\n\n");
}

}
