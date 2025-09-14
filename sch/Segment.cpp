#include <algorithm>
#include <string>

#include "Segment.h"
#include "Subckt.h"

using namespace std;

namespace sch {

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

// Return a mapping from the dst to the src that will be used to instantiate the dst in the src.
Mapping<int> Segment::generate(Subckt &dst, const Subckt &src) const {
	vector<int> dstNets;
	for (auto i = mos.begin(); i != mos.end(); i++) {
		dstNets.push_back(src.mos[*i].drain);
		dstNets.push_back(src.mos[*i].gate);
		dstNets.push_back(src.mos[*i].source);
		dstNets.push_back(src.mos[*i].base);
	}
	sort(dstNets.begin(), dstNets.end());
	dstNets.erase(unique(dstNets.begin(), dstNets.end()), dstNets.end());

	Mapping<int> srcToDst(-1, false);
	for (auto i = dstNets.begin(); i != dstNets.end(); i++) {
		auto n = src.nets.begin()+*i;

		bool isIO = n->isIO or not n->portOf.empty();
		for (int type = 0; type < 2 and not isIO; type++) {
			for (auto j = n->gateOf[type].begin(); j != n->gateOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos == mos.end() or *pos != *j);
			}
			for (auto j = n->sourceOf[type].begin(); j != n->sourceOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos == mos.end() or *pos != *j);
			}
			for (auto j = n->drainOf[type].begin(); j != n->drainOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos == mos.end() or *pos != *j);
			}
			for (auto j = n->baseOf[type].begin(); j != n->baseOf[type].end() and not isIO; j++) {
				auto pos = lower_bound(mos.begin(), mos.end(), *j);
				isIO = (pos == mos.end() or *pos != *j);
			}
		}

		srcToDst.set(*i, dst.push(Net(n->name, isIO)));
	}

	for (auto i = mos.begin(); i != mos.end(); i++) {
		auto d = src.mos.begin()+*i;
		int gate = srcToDst.map(d->gate);
		int source = srcToDst.map(d->source);
		int drain = srcToDst.map(d->drain);
		int base = srcToDst.map(d->base);

		if (gate == srcToDst.undef
			or source == srcToDst.undef
			or drain == srcToDst.undef
			or base == srcToDst.undef) {
			printf("internal %s:%d: cell net map missing nets\n", __FILE__, __LINE__);
		}

		dst.push(Mos(d->model, d->type, drain, gate, source, base));
		dst.mos.back().size = d->size;
		dst.mos.back().area = d->area;
		dst.mos.back().perim = d->perim;
		dst.mos.back().params = d->params;
	}

	for (int i = 0; i < (int)dst.nets.size(); i++) {
		if (dst.nets[i].isIO and dst.nets[i].isOutput()) {
			dst.nets[i].name = "o" + to_string(i);
		} else if (dst.nets[i].isIO and dst.nets[i].isInput()) {
			dst.nets[i].name = "i" + to_string(i);
		} else if (not dst.nets[i].isIO) {
			dst.nets[i].name = "_" + to_string(i);
		}
	}

	return srcToDst;
}

bool Segment::contains(int i) const {
	return find(mos.begin(), mos.end(), i) != mos.end();
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
