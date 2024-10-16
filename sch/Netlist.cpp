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

Netlist::Netlist(const Tech &tech) {
	this->tech = &tech;
}

Netlist::~Netlist() {
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

			auto segments = subckts[i].segment();

			int total = 0;
			for (auto s = segments.begin(); s != segments.end(); s++) {
				total += (int)s->mos.size();
			}

			for (auto s = segments.begin(); s != segments.end(); s++) {
				Subckt cell(true);
				Mapping m = s->generate(cell, subckts[i]);
				m.apply(cell.canonicalize());
				cell.name = "cell_" + idToString(cell.id);
				int index = insert(cell);

				subckts[i].extract(*s);
				subckts[i].pushInst(Instance(subckts[index], m, index));

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

			subckts[i].cleanDangling();
			if (not subckts[i].mos.empty()) {
				printf("failed to segment all devices\n");
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
