#include "Placer.h"
#include "Draw.h"

#include <list>
#include <set>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace sch {

Placement::Placement(const Subckt &ckt, int b, int l, int w, int g, std::default_random_engine &rand) : ckt(ckt) {
	this->b = b;
	this->l = l;
	this->w = w;
	this->g = g;

	// fill stacks with devices that have random orientiations
	static std::bernoulli_distribution distribution(0.5);
	for (int i = 0; i < (int)ckt.mos.size(); i++) {
		stack[ckt.mos[i].type].push_back(Device{i, distribution(rand)});
	}
	// cache stack size differences
	this->d[0] = max(0, (int)stack[0].size()-(int)stack[1].size());
	this->d[1] = max(0, (int)stack[1].size()-(int)stack[0].size());

	// compute Wmin
	array<int, 2> D;
	for (int type = 0; type < 2; type++) {
		D[type] = -2;
		for (int i = 0; i < (int)ckt.nets.size(); i++) {
			D[type] += (ckt.nets[i].ports(type)&1);
		}
		D[type] >>= 1;
	}
	this->Wmin = max((int)stack[0].size()+D[0], (int)stack[1].size()+D[1]) - max((int)stack[0].size(), (int)stack[1].size());

	// add dummy transistors
	bool shorter = stack[1].size() < stack[0].size();
	stack[shorter].resize(stack[not shorter].size(), Device{-1,false});

	// generate a random initial placement
	for (int type = 0; type < 2; type++) {
		shuffle(stack[type].begin(), stack[type].end(), rand);
	}
}

Placement::~Placement() {
}

Placement::Placement(const Placement &p) : ckt(p.ckt) {
	this->b = p.b;
	this->l = p.l;
	this->w = p.w;
	this->g = p.g;
	this->Wmin = p.Wmin;
	this->d = p.d;
	this->stack = p.stack;
}

void Placement::move(vec4i choice) {
	for (int i = choice[0]; i < choice[1]; i++) {
		int j = choice[2], k = choice[3];
		for (; j < k; j++, k--) {
			swap(stack[i][j], stack[i][k]);
			stack[i][j].flip = not stack[i][j].flip;
			stack[i][k].flip = not stack[i][k].flip;
		}
		if (j == k) {
			stack[i][j].flip = not stack[i][j].flip;
		}
	}
}

// Compute the cost of this placement using the cost function documented in floret/floret/Placer.h
int Placement::score() {
	int B = 0, W = 0, L = 0, G = 0;
	array<int, 2> brk = {0,0};

	// compute intermediate values
	// brk[] counts the number of diffusion breaks in this placement for each stack
	// ext[] finds the first and last index of each net
	vector<vec2i> ext(ckt.nets.size(), vec2i(((int)stack[0].size()+1)*2, -1));
	for (int type = 0; type < 2; type++) {
		for (auto c = stack[type].begin(); c != stack[type].end(); c++) {
			brk[type] += (c+1 != stack[type].end() and c->device >= 0 and (c+1)->device >= 0 and ckt.mos[c->device].right(c->flip) != ckt.mos[(c+1)->device].left((c+1)->flip));

			if (c->device >= 0) {
				int gate = ckt.mos[c->device].gate;
				int source = ckt.mos[c->device].source;
				int drain = ckt.mos[c->device].drain;
				int off = (c-stack[type].begin())<<1;

				ext[gate][0] = min(ext[gate][0], off+1);
				ext[gate][1] = max(ext[gate][1], off+1);
				ext[source][0] = min(ext[source][0], off+2*((int)(not c->flip)));
				ext[source][1] = max(ext[source][1], off+2*((int)(not c->flip)));
				ext[drain][0] = min(ext[drain][0], off+2*((int)c->flip));
				ext[drain][1] = max(ext[drain][1], off+2*((int)c->flip));
			}
		}
	}

	// compute B, L, and W
	for (int i = 0; i < (int)ext.size(); i++) {
		L += ext[i][1] - ext[i][0];
	}
	B = brk[0]+brk[1];
	W = min(brk[0]+d[0]-Wmin,brk[1]+d[1]-Wmin);

	// compute G, keeping track of diffusion breaks
	for (auto i = stack[0].begin(), j = stack[1].begin(); i != stack[0].end() and j != stack[1].end(); i++, j++) {
		bool ibrk = (i != stack[0].begin() and (i-1)->device >= 0 and i->device >= 0 and ckt.mos[(i-1)->device].right((i-1)->flip) != ckt.mos[i->device].left(i->flip));
		bool jbrk = (j != stack[1].begin() and (j-1)->device >= 0 and j->device >= 0 and ckt.mos[(j-1)->device].right((j-1)->flip) != ckt.mos[j->device].left(j->flip));

		if (ibrk and not jbrk) {
			j++;
		} else if (jbrk and not ibrk) {
			i++;
		}
		
		if (i == stack[0].end() or j == stack[1].end()) {
			break;
		}

		G += (i->device >= 0 and j->device >= 0 and ckt.mos[i->device].gate != ckt.mos[j->device].gate);
	}

	return max(0, b*B*B + l*L + w*W*W + g*G);
}

Placement Placement::solve(const Subckt &ckt, int starts, int b, int l, int w, int g, float step, float rate) {
	std::default_random_engine rand(0/*std::random_device{}()*/);
	if (ckt.mos.size() == 0) {
		return Placement(ckt, b, l, w, g, rand);
	}

	// TODO(edward.bingham) It might speed things up to scale the number of
	// starts based upon the cell complexity. Though, given that it would really
	// only reduce computation time for smaller cells and the larger cells
	// account for the majority of the computation time, I'm not sure that this
	// would really do that much to help.
	//starts = 50*(int)ckt.mos.size();

	Placement best(ckt, b, l, w, g, rand);
	int bestScore = best.score();

	// Precache the list of all possible moves. These will get reshuffled each time.
	vector<vec4i> choices;
	for (int i = 0; i < 3; i++) {
		int end = (i == 2 ? min((int)best.stack[0].size(), (int)best.stack[1].size()) : (int)best.stack[i].size());
		for (int j = 0; j < end; j++) {
			for (int k = j+1; k < end; k++) {
				choices.push_back(vec4i(i == 2 ? 0 : i, i == 2 ? 2 : i+1, j, k));
			}
		}
	}

	// Check multiple possible initial placements to avoid local minima
	for (int i = 0; i < starts; i++) {
		//printf("start %d/%d\r", i, starts);
		//fflush(stdout);

		// Run simulated annealing to find closest minimum
		Placement curr(ckt, b, l, w, g, rand);
		int score = 0;
		int newScore = curr.score();
		float currStep = step;
		do {
			// Test all of the possible moves and pick the best one.
			score = newScore;
			for (auto choice = choices.begin(); choice != choices.end(); choice++) {
				//printf("\r%06d/%06d  %f %f %06d<%06d", (int)(choice-choices.begin()), (int)choices.size(), currStep, rate, newScore, score);
				//fflush(stdout);
				curr.move(*choice);

				// Check if this move makes any improvement within the constraints of
				// the annealing temperature
				newScore = curr.score();
				if (newScore < score*currStep) {
					break;
				} else {
					// undo the previous move
					curr.move(*choice);
				}
			}

			// Reshuffle the list of possible moves
			shuffle(choices.begin(), choices.end(), rand);

			// cool the annealing temperature
			float prevStep = currStep;
			currStep -= (currStep - 1.0)*rate;
			if (currStep == prevStep) {
				currStep = 1.0;
			}
			//printf("%f %f %d<%d\n", currStep, rate, newScore, score);
		} while ((float)score*currStep - (float)newScore > 0.01);

		if (score < bestScore) {
			bestScore = score;
			best = curr;
		}
	}
	//printf("Placement complete after %d iterations\n", starts);

	return best;
}

Placement &Placement::operator=(const Placement &p) {
	this->b = p.b;
	this->l = p.l;
	this->w = p.w;
	this->g = p.g;
	this->Wmin = p.Wmin;
	this->d = p.d;
	this->stack = p.stack;
	return *this;
}

}
