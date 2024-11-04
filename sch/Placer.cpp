#include "Placer.h"
#include "Draw.h"

#include <list>
#include <set>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace sch {

// 1. Solve the transistor placement length issue
// 2. Cache the computed vertical alignment between transistors
// 3. This alignment becomes invariant in the Router, pass it to the Router.
// 4. Use it to compute initial pin placement. Do a greedy algorithm to place
// the non-aligned pins so that we reduce the number of pin constraints.
// 5. Lock in those pin constraints by creating forced pin-orderings
// 6. With those pin constraints locked in, the route ordering won't change the horizontal relationships between pins, just the relative distances. This means that we can just compute the route ordering once and then expand out the pins to match, keeping those relationships.
// 7. Lower the routes as much as possible. Check/eliminate/add route constraints.

Placement::Placement(const Tech &tech, const Subckt &ckt, int l, int w, int g, std::default_random_engine &rand) {
	this->ckt = &ckt;

	this->l = l;
	this->w = w;
	this->g = g;

	wired.resize(ckt.nets.size(), false);
	for (int i = 0; i < (int)ckt.nets.size(); i++) {
		auto n = ckt.nets.begin()+i;
		wired[i] = ((n->gateOf[0].size()+n->gateOf[1].size() != 0)
			or ((n->sourceOf[0].size()+n->drainOf[0].size() != 0)
				and (n->sourceOf[1].size()+n->drainOf[1].size() != 0))
			or (n->sourceOf[0].size()+n->drainOf[0].size() > 2)
			or (n->sourceOf[1].size()+n->drainOf[1].size() > 2)
			or (n->portOf.size() != 0));
	}

	// fill stacks with devices that have random orientiations
	static std::bernoulli_distribution distribution(0.5);
	for (int i = 0; i < (int)ckt.mos.size(); i++) {
		stack[ckt.mos[i].type].push_back(Device{i, 0, ckt.mos[i].size[0], distribution(rand)});
	}
	// cache stack size differences
	this->d[0] = max(0, (int)stack[0].size()-(int)stack[1].size());
	this->d[1] = max(0, (int)stack[1].size()-(int)stack[0].size());

	int model = ckt.mos[0].model;
	int poly = tech.wires[0].draw;
	int via = tech.vias[tech.findVias(flip(model), 1)[0]].draw;
	int diff = tech.subst[tech.models[model].stack[0]].draw;

	int gateToGate = tech.getSpacing(poly, poly);
	int diffEncloseContact = tech.getEnclosing(diff, via)[1];
	int diffToDiff = tech.getSpacing(diff, diff);

	gateToContact = tech.getSpacing(poly, via);
	contactWidth = tech.paint[via].minWidth;

	seqDist = gateToGate;
	parDist = gateToContact*2+contactWidth;
	brkDist = (gateToContact+contactWidth+diffEncloseContact)*2+diffToDiff;

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

	// generate a random initial placement
	for (int type = 0; type < 2; type++) {
		shuffle(stack[type].begin(), stack[type].end(), rand);
	}

	// compute initial pos
	for (int type = 0; type < 2; type++) {
		for (int i = 1; i < (int)stack[type].size(); i++) {
			stack[type][i].pos = stack[type][i-1].pos
				+ dist(stack[type][i-1], stack[type][i]);
		}
	}
}

Placement::~Placement() {
}

int Placement::dist(const Device &d0, const Device &d1) {
	int n0 = ckt->mos[d0.device].right(d0.flip);
	int n1 = ckt->mos[d1.device].left(d1.flip);
	return d0.length + (
		n0 != n1 ? brkDist : (
			wired[n0] ? parDist :
				seqDist));
}

void Placement::move(vec4i choice) {
	for (int i = choice[0]; i < choice[1]; i++) {
		int j = choice[2], k = min(choice[3], (int)stack[i].size()-1);
		for (; j < k; j++, k--) {
			swap(stack[i][j], stack[i][k]);
			stack[i][j].flip = not stack[i][j].flip;
			stack[i][k].flip = not stack[i][k].flip;
		}
		if (j == k) {
			stack[i][j].flip = not stack[i][j].flip;
		}

		// update pos
		for (int j = choice[2]; j < (int)stack[i].size(); j++) {
			int npos = (j == 0 ? 0 : stack[i][j-1].pos
				+ dist(stack[i][j-1], stack[i][j]));
			if (stack[i][j].pos == npos and j > choice[3]) {
				break;
			}
			stack[i][j].pos = npos;
		}
	}
}

// Compute the cost of this placement using the cost function documented in floret/floret/Placer.h
int Placement::score() {
	vector<vec4i> nets(ckt->nets.size(), vec2i(
			std::numeric_limits<int>::max(),
			std::numeric_limits<int>::max(),
			std::numeric_limits<int>::min(),
			std::numeric_limits<int>::min()
		));

	int width = max(stack[0].empty() ? 0 : stack[0].back().pos, stack[1].empty() ? 0 : stack[1].back().pos);

	vector<bool> hasGate(nets.size(), false);
	for (int type = 0; type < (int)stack.size(); type++) {
		int right = stack[type].empty() ? 0 : stack[type].back().pos;
		for (int i = 0; i < (int)stack[type].size(); i++) {
			auto mos = ckt->mos.begin()+stack[type][i].device;
			hasGate[mos->gate] = true;
			// Gate
			int lpos = stack[type][i].pos;
			int rpos = width - (right-lpos);
			int net = mos->gate;
			nets[net][0] = min(nets[net][0], lpos);
			nets[net][1] = min(nets[net][1], rpos);
			nets[net][2] = max(nets[net][2], lpos + stack[type][i].length);
			nets[net][3] = max(nets[net][3], rpos + stack[type][i].length);

			lpos = stack[type][i].pos + stack[type][i].length + gateToContact;
			rpos = width - (right-lpos);
			net = mos->right(stack[type][i].flip);
			nets[net][0] = min(nets[net][0], lpos);
			nets[net][1] = min(nets[net][1], rpos);
			nets[net][2] = max(nets[net][2], lpos + contactWidth);
			nets[net][3] = max(nets[net][3], rpos + contactWidth);

			lpos = stack[type][i].pos - gateToContact;
			rpos = width - (right-lpos);
			net = mos->left(stack[type][i].flip);
			nets[net][0] = min(nets[net][0], lpos);
			nets[net][1] = min(nets[net][1], rpos);
			nets[net][2] = max(nets[net][2], lpos + contactWidth);
			nets[net][3] = max(nets[net][3], rpos + contactWidth);
		}
	}

	int overlap = 0;
	int extent = 0;
	for (int i = 0; i < (int)nets.size(); i++) {
		for (int j = 0; j < (int)nets.size(); j++) {
			if (hasGate[i] and hasGate[j]) {
				overlap += (nets[i][0] <= nets[j][3] and nets[j][0] <= nets[i][3]);
				overlap += ((nets[i][1] <= nets[j][3] and nets[j][1] <= nets[i][3])
					or (nets[i][0] <= nets[j][2] and nets[j][0] <= nets[i][2]));
				overlap += (nets[i][1] <= nets[j][2] and nets[j][1] <= nets[i][2]);
			}
		}
		extent += max(0, nets[i][2]-nets[i][1]);
	}

	// compute minimum and maximum number of overlapping routes
	// compute total minimum extent of routes
	// compute width of current placement
	return max(0, l*extent + w*width + g*overlap);
}

Placement Placement::solve(const Tech &tech, const Subckt &ckt, int starts, int l, int w, int g, float step, float rate) {
	//printf("Running Placement\n");
	std::default_random_engine rand(0/*std::random_device{}()*/);
	Placement best(tech, ckt, l, w, g, rand);
	if (ckt.mos.size() == 0) {
		return best;
	}

	// TODO(edward.bingham) It might speed things up to scale the number of
	// starts based upon the cell complexity. Though, given that it would really
	// only reduce computation time for smaller cells and the larger cells
	// account for the majority of the computation time, I'm not sure that this
	// would really do that much to help.
	//starts = 50*(int)ckt->mos.size();
	int bestScore = best.score();

	// Precache the list of all possible moves. These will get reshuffled each time.
	vector<vec4i> choices;
	for (int i = 0; i < 3; i++) {
		int end = (i == 2 ? max((int)best.stack[0].size(), (int)best.stack[1].size()) : (int)best.stack[i].size());
		for (int j = 0; j < end; j++) {
			for (int k = j+1; k < end; k++) {
				choices.push_back(vec4i(i == 2 ? 0 : i, i == 2 ? 2 : i+1, j, k));
			}
		}
	}

	// Check multiple possible initial placements to avoid local minima
	for (int i = 0; i < starts; i++) {
		//printf("start %d/%d\n", i, starts);

		// Run simulated annealing to find closest minimum
		Placement curr(tech, ckt, l, w, g, rand);
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

		//printf("done %d/%d\n", score, bestScore);
		if (score < bestScore) {
			bestScore = score;
			best = curr;
		}
	}
	//printf("Placement complete after %d iterations\n", starts);

	return best;
}

}
