#include "Netlist.h"
#include "Draw.h"
#include "Placer.h"
#include "Router.h"

#include <chrono>
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
using namespace std::chrono;

using namespace std;

namespace sch {

Netlist::Netlist(const Tech &tech) : tech(tech) {
}

Netlist::~Netlist() {
}

bool Netlist::mapCells(Subckt &ckt) {
	vector<pair<Mapping, int> > options;
	for (int i = 0; i < (int)subckts.size(); i++) {
		if (ckt.mos.empty()) {
			return true;
		}

		if (subckts[i].isCell) {
			auto found = ckt.find(subckts[i]);
			for (auto m = found.begin(); m != found.end(); m++) {
				options.push_back(pair<Mapping, int>(*m, i));
			}
		}
	}

	// Now we have a set of possible mappings. Those mappings may overlap. We
	// want to find a selection of mappings that minimizes the total number of
	// cells used to cover the maximum possible number of devices in this subckt
	
	// This is equivalent to the problem of identifying the maximal clique in the
	// graph that minimizes number of nodes while maximizing coverage. The graph
	// is constructed using the subsets in "options" as vertices and creating
	// edges between each pair that doesn't overlap. This is an NP-complete
	// problem and we are solving it using the Bron–Kerbosch algorithm.
	int covered = 0;
	vector<int> result;

	// TODO(edward.bingham) maybe want to cache the results of the overlapsWith computation
	
	struct BronKerboschFrame {
		vector<int> R, P, X;
	};

	vector<BronKerboschFrame> frames;
	frames.push_back(BronKerboschFrame());
	frames.back().P.reserve(options.size());
	for (int i = 0; i < (int)options.size(); i++) {
		frames.back().P.push_back(i);
	}

	while (not frames.empty()) {
		auto frame = frames.back();
		frames.pop_back();

		if (frame.P.empty() and frame.X.empty()) {
			int newCovered = 0;
			for (auto r = frame.R.begin(); r != frame.R.end(); r++) {
				newCovered += (int)options[*r].first.devices.size();
			}

			// Then we've found a maximal clique
			if (newCovered > covered or (newCovered == covered and frame.R.size() < result.size())) {
				result = frame.R;
				covered = newCovered;
			}
		} else {
			// Otherwise, we need to recurse
			while (not frame.P.empty()) {
				frames.push_back(frame);
				frames.back().R.push_back(frame.P.back());
				auto o0 = options.begin() + frame.P.back();
				for (int i = (int)frames.back().P.size()-1; i >= 0; i--) {
					auto o1 = options.begin() + frames.back().P[i];
					if (o0->first.overlapsWith(o1->first)) {
						frames.back().P.erase(frames.back().P.begin() + i);
					}
				}
				for (int i = (int)frames.back().X.size()-1; i >= 0; i--) {
					auto o1 = options.begin() + frames.back().X[i];
					if (o0->first.overlapsWith(o1->first)) {
						frames.back().X.erase(frames.back().X.begin() + i);
					}
				}

				frame.X.push_back(frame.P.back());
				frame.P.pop_back();
			}
		}
	}

	// Now we have the best cell mapping. We need to apply it.
	for (auto i = result.begin(); i != result.end(); i++) {
		ckt.extract(subckts[options[*i].second], options[*i].first, options[*i].second);
		for (auto j = i+1; j != result.end(); j++) {
			options[*j].first.extract(options[*i].first);
		}
	}

	return ckt.mos.empty();
}

int Netlist::insert(int idx) {
	auto pos = cells.insert(pair<size_t, set<int> >(subckts[idx].id, set<int>()));
	for (auto j = pos.first->second.begin(); j != pos.first->second.end(); j++) {
		if (*j == idx) {
			return idx;
		} else if (subckts[*j] == subckts[idx]) {
			int result = *j > idx ? *j-1 : *j;
			erase(idx);
			return result;
		}
	}
	pos.first->second.insert(idx);
	return idx;
}

int Netlist::insert(const Subckt &cell) {
	auto pos = cells.insert(pair<size_t, set<int> >(cell.id, set<int>()));
	int index = (int)subckts.size();
	for (auto j = pos.first->second.begin(); j != pos.first->second.end(); j++) {
		if (subckts[*j] == cell) {
			index = *j;
			break;
		}
	}
	if (index >= (int)subckts.size()) {
		pos.first->second.insert(index);
		subckts.push_back(cell);
	}
	return index;
}

void Netlist::erase(int idx) {
	for (auto i = cells.begin(); i != cells.end(); ) {
		set<int> updated;
		for (auto j = i->second.begin(); j != i->second.end(); j++) {
			if (*j < idx) {
				updated.insert(*j);
			} else if (*j > idx) {
				updated.insert(*j-1);
			}
		}
		if (updated.empty()) {
			i = cells.erase(i);
		} else {
			i->second = updated;
			i++;
		}
	}

	subckts.erase(subckts.begin()+idx);
}

void Netlist::mapCells(bool progress) {
	// check existing cells
	for (int i = (int)subckts.size()-1; i >= 0; i--) {
		if (subckts[i].isCell and not subckts[i].mos.empty()) {
			subckts[i].canonicalize();
			insert(i);
		}
	}

	// break large subckts into new cells
	if (progress) {
		printf("Break subckts into cells:\n");
	}
	steady_clock::time_point start = steady_clock::now();
	for (int i = (int)subckts.size()-1; i >= 0; i--) {
		if (not subckts[i].isCell and not subckts[i].mos.empty()) {
			int count = (int)subckts.size();

			if (progress) {
				printf("  %s...", subckts[i].name.c_str());
				fflush(stdout);
			}

			auto segments = subckts[i].generateCells();

			int total = 0;
			for (auto s = segments.begin(); s != segments.end(); s++) {
				total += (int)s->devices.size();
			}

			printf("extracting cells %d/%d\n", total, (int)subckts[i].mos.size());
			for (auto s = segments.begin(); s != segments.end(); s++) {
				Subckt cell = s->generate(subckts[i]);
				s->remap(cell.canonicalize());
				cell.name = "cell_" + idToString(cell.id);
				int index = insert(cell);

				printf("extracting mapping %d\n", (int)subckts[i].mos.size());
				s->print();
				subckts[i].extract(subckts[index], *s, index);
				printf("done with extraction %d\n", (int)subckts[i].mos.size());

				//print();
				for (auto s1 = s+1; s1 != segments.end(); s1++) {
					// DESIGN(edward.bingham) if two segments overlap, then we just remove
					// the extra devices from one of the segments. It's only ok to have those
					// devices in a different cell if the signals connecting them don't
					// switch (for example, shared weak ground). Otherwise it's an isochronic
					// fork assumption violation.

					if (not s1->extract(*s)) {
						printf("internal %s:%d: overlapping cells found\n", __FILE__, __LINE__);
					}
					//segments[j].print();
				}
			}

			if (progress) {
				printf("[%s%d UNIQUE/%d CELLS%s]\n", KGRN, (int)subckts.size()-count, (int)segments.size(), KNRM);
			}
		}
	}
	steady_clock::time_point finish = steady_clock::now();
	if (progress) {
		printf("done [%gs]\n\n", ((float)duration_cast<milliseconds>(finish - start).count())/1000.0);
	}
}

string idToString(size_t id) {
	// spice names are not sensitive to capitalization, and only support alphanum
	// characters. Not enough character types to support base64.
	static const string digits = "abcdefghijklmnopqrstuvwxyz012345";

	std::string result;
	for (int i = 0; i < (int)sizeof(size_t); i++) {
		int idx = id & 0x1F;
		id >>= 5;
		result.push_back(digits[idx]);
	}
	return result;
}

}
