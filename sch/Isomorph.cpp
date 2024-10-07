#include <limits>
#include <algorithm>
#include <string>
#include <set>

#include "Isomorph.h"

using namespace std;

namespace sch {

Partition Partition::unit(int size) {
	Partition result;
	result.cells.push_back(vector<int>());
	result.cells.back().reserve(size);
	for (int i = 0; i < size; i++) {
		result.cells.back().push_back(i);
	}
	return result;
}

Partition Partition::discrete(int size) {
	Partition result;
	result.cells.reserve(size);
	for (int i = 0; i < size; i++) {
		result.cells.push_back(vector<int>(1, i));
	}
	return result;
}

int Partition::cellOf(int v) const {
	for (int i = 0; i < (int)cells.size(); i++) {
		for (int j = 0; j < (int)cells[i].size(); j++) {
			if (cells[i][j] == v) {
				return i;
			}
		}
	}
	return -1;
}

int Partition::next() const {
	int curr = -1;
	for (int i = 0; i < (int)cells.size(); i++) {
		if (cells[i].size() == 2) {
			return i;
		} else if (cells[i].size() > 2 and (curr < 0 or cells[i].size() < cells[curr].size())) {
			curr = i;
		}
	}
	return curr;
}

bool Partition::isDiscrete(int ci) const {
	return cells[ci].size() == 1;
}

bool Partition::isDiscrete() const {
	for (auto p = cells.begin(); p != cells.end(); p++) {
		if (p->size() != 1) {
			return false;
		}
	}
	return true;
}

Partition Partition::pop(int ci, int vi) {
	cells.push_back(vector<int>(1, cells[ci][vi]));
	cells[ci].erase(cells[ci].begin()+vi);
	Partition result;
	result.cells.push_back(cells.back());
	return result;
}

void Partition::swap(Partition &other) {
	cells.swap(other.cells);
}

void Partition::merge(const Partition &other, int from) {
	cells.insert(cells.end(), other.cells.begin()+from, other.cells.end());
}

vector<int> Partition::toLabels() const {
	vector<int> result;
	for (auto cell = cells.begin(); cell != cells.end(); cell++) {
		result.push_back(cell->back());
	}
	return result;
}

/*vector<int> Subckt::omega(vector<vector<int> > pi) const {
	vector<int> result;
	for (auto cell = pi.begin(); cell != pi.end(); cell++) {
		result.push_back((*cell)[0]);
	}
	return result;
}

vector<vector<int> > Subckt::discreteCellsOf(vector<vector<int> > pi) const {
	vector<vector<int> > result;
	for (int i = 0; i < (int)pi.size(); i++) {
		if (pi[i].size() == 1) {
			result.push_back(pi[i]);
		}
	}
	return result;
}*/

}
