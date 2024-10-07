#pragma once

#include <map>
#include <string>
#include <vector>
#include <unordered_set>
#include <limits>
#include <array>

using namespace std;

namespace sch {

struct Partition {
	using Cell = vector<int>;
	using Cells = vector<Cell>;

	Cells cells;

	static Partition unit(int size);
	static Partition discrete(int size);

	int cellOf(int v) const;
	int next() const;
	bool isDiscrete() const;
	bool isDiscrete(int ci) const;
	Partition pop(int ci, int vi);
	void swap(Partition &other);
	void merge(const Partition &other, int from=0);
	vector<int> toLabels() const;


	template <typename Graph>
	Partition refineCell(const Graph &g, int ci, const Partition &beta) const {
		map<Cells, Cell> partitions;
		for (auto v = cells[ci].begin(); v != cells[ci].end(); v++) {
			partitions.insert(
				pair<Cells, Cell>(
					g.createPartitionKey(*v, beta),
					Cell()
				)
			).first->second.push_back(*v);
		}

		Partition result;
		for (auto c = partitions.begin(); c != partitions.end(); c++) {
			result.cells.push_back(c->second);
		}
		return result;
	}

	template <typename Graph>
	bool refine(const Graph &g, Partition alpha) {
		bool change = false;	
		Partition next;
		while (not alpha.cells.empty() and not isDiscrete()) {
			// choose arbitrary subset of alpha
			// TODO(edward.bingham) figure out how to make this choice so as to speed up convergence.
			Partition beta;
			beta.cells.push_back(alpha.cells.back());
			alpha.cells.pop_back();

			for (int i = 0; i < (int)cells.size(); i++) {
				Partition refined = refineCell(g, i, beta);
				next.merge(refined);
				if (refined.cells.size() > 1) {
					alpha.merge(refined, 1);
					change = true;
				}
			}
			cells.swap(next.cells);
			next.cells.clear();
		}
		return change;
	}
};

struct Frame {
	Frame() {}

	template <typename Graph>
	Frame(const Graph &g) {
		part = Partition::unit(g.verts());
		part.refine(g, part);
		ci = part.next();
		vi = 0;
		
		v = -1;
	}

	~Frame() {}

	Partition part;
	int ci;
	int vi;
	
	int v;

	vector<array<int, 4> > l;

	bool inc() {
		v = part.cells[ci][vi];
		return (++vi < (int)part.cells[ci].size());
	}

	template <typename Graph>
	bool pop(const Graph &g) {
		if (part.refine(g, part.pop(ci, vi)) or part.isDiscrete(ci)) {
			ci = part.next();
		}
		vi = 0;

		l = g.lambda(part);
		return ci < 0;
	}

	template <typename Graph>
	bool isLessThan(const Graph &g, const Frame &frame) const {
		int m = (int)min(l.size(), frame.l.size());
		for (int i = 0; i < m; i++) {
			for (int j = 0; j < 4; j++) {
				if (l[i][j] < frame.l[i][j]) {
					return false;
				} else if (l[i][j] > frame.l[i][j]) {
					return true;
				}
			}
		}

		if (not part.isDiscrete() or not frame.part.isDiscrete()) {
			return false;
		}
		return g.comparePartitions(part, frame.part) == -1;
	}
};

//vector<int> omega(vector<vector<int> > pi) const;
//vector<vector<int> > discreteCellsOf(vector<vector<int> > pi) const;



// This function will be derived from Nauty, Bliss, and DviCL
//
// B.D. McKay: Computing automorphisms and canonical labellings of
// graphs. International Conference on Combinatorial Mathematics,
// Canberra (1977), Lecture Notes in Mathematics 686, Springer-
// Verlag, 223-232.
//
// Junttila, Tommi, and Petteri Kaski. "Engineering an efficient canonical
// labeling tool for large and sparse graphs." 2007 Proceedings of the Ninth
// Workshop on Algorithm Engineering and Experiments (ALENEX). Society for
// Industrial and Applied Mathematics, 2007.
//
// Lu, Can, et al. "Graph ISO/auto-morphism: a divide-&-conquer approach."
// Proceedings of the 2021 International Conference on Management of Data.
// 2021.
//
// TODO(edward.bingham) implement optimizations from nauty, bliss, and dvicl
//
// struct Graph {
//   // Used to refine partitions (required)
//   vector<vector<int> > createPartitionKey(int v, const vector<vector<int> > &beta) const;
//
//   // Used to identify canonical labelings (required)
//   int comparePartitions(const vector<vector<int> > &pi0, const vector<vector<int> > &pi1) const;
//
//   // Used to prune the search tree (optional)
//   vector<array<int, 4> > lambda(vector<vector<int> > pi) const;
// };

template <typename Graph>
vector<int> canonicalLabels(const Graph &g) {
	// TODO(edward.bingham) map discreteCellsOf() -> omega() and use this to
	// prune automorphisms from the search tree.
	// map<vector<int>, vector<int> > stored;

	vector<Frame> best;
	vector<Frame> frames(1, Frame(g));
	if (frames.back().part.isDiscrete()) {
		best = frames;
		frames.pop_back();
	}

	int explored = 0;
	while (not frames.empty()) {
		Frame next = frames.back();
		if (not frames.back().inc()) {
			frames.pop_back();
		}

		/*for (auto p = next.part.begin(); p != next.part.end(); p++) {
			if (not is_sorted(p->begin(), p->end())) {
				printf("violation of sorting assumption\n");
			}
		}*/

		if (next.pop(g)) {
			// found a discrete partition
			explored++;
			// found terminal node in tree
			int cmp = best.empty() ? 1 : g.comparePartitions(next.part, best.back().part);
			if (cmp == 1) {
				best = frames;
				best.push_back(next);
			} else if (cmp == 0) {
				// We found an automorphism. We can use this to prune the search space.
				// Find a shared node in the frames list between best and next
				int from = 0;
				while (from < (int)frames.size()
					and from < (int)best.size()
					and frames[from].v == best[from].v) {
					from++;
				}
				frames.resize(from);
			}
		} else if (best.empty() or not next.isLessThan(g, best.back())) {
			frames.push_back(next);
		}
	}

	return best.back().part.toLabels();
}

}
