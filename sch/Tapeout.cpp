#include "Tapeout.h"

#include "Netlist.h"
#include "Draw.h"
#include "Placer.h"
#include "Router.h"

#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include <filesystem>

#include <chrono>
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
using namespace std::chrono;

using namespace std;
using namespace phy;


namespace sch {

int routeCell(phy::Library &lib, Netlist &lst, int idx, bool progress, bool debug) {
	bool place = true;
	bool route = true;
	Placement pl = Placement::solve(lst.subckts[idx]);
	Router rt(*lib.tech, pl, progress, debug);
	route = rt.solve();
	drawCell(lib.macros[idx], rt);
	rt.annotateAreaPerim(lst.subckts[idx]);
	if (not place) {
		return 1;
	} else if (not route) {
		return 2;
	}
	return 0;
}

bool extract(Subckt &dst, Layout &src) {
	if (src.nets.empty()) {
		src.trace();
	}

	dst.name = src.name;
	for (auto net = src.nets.begin(); net != src.nets.end(); net++) {
		if (net->names.empty()) {
			dst.push(Net("_" + to_string(dst.nets.size()), net->isInput or net->isOutput));
		} else {
			sort(net->names.begin(), net->names.end());
			dst.push(Net(net->names[0], net->isInput or net->isOutput));
		}
	}

	auto poly = src.find(src.tech->wires[0].draw);
	if (poly == src.layers.end()) {
		return false;
	}

	for (auto model = src.tech->models.begin(); model != src.tech->models.end(); model++) {
		int modelID = (model-src.tech->models.begin());
		Layer diff(*src.tech);
		auto well = src.layers.end();
		bool found = true;
		for (auto sub = model->stack.begin(); sub != model->stack.end(); sub++) {
			if (src.tech->subst[flip(*sub)].isWell) {
				if (well == src.layers.end() and src.tech->subst[flip(*sub)].draw >= 0) {
					well = src.find(src.tech->subst[flip(*sub)].draw);
				}
				if (well == src.layers.end() and src.tech->subst[flip(*sub)].pin >= 0) {
					well = src.find(src.tech->subst[flip(*sub)].pin);
				}
				if (well == src.layers.end() and src.tech->subst[flip(*sub)].label >= 0) {
					well = src.find(src.tech->subst[flip(*sub)].label);
				}
				continue;
			}
			if (src.tech->subst[flip(*sub)].draw < 0) {
				continue;
			}

			auto layer = src.find(src.tech->subst[flip(*sub)].draw);
			if (layer == src.layers.end()) {
				found = false;
				// Then this model doesn't exist in the layout
				break;
			}
			if (diff.geo.empty()) {
				diff = layer->second;
			} else {
				diff = diff & layer->second;
			}
		}
		if (not found) {
			continue;
		}

		for (auto sub = model->excl.begin(); sub != model->excl.end(); sub++) {
			if (src.tech->subst[flip(*sub)].draw < 0) {
				continue;
			}

			auto layer = src.find(src.tech->subst[flip(*sub)].draw);
			if (layer == src.layers.end()) {
				continue;
			}
			if (not diff.geo.empty()) {
				diff = diff & ~layer->second;
			}
		}

		int globalBase = -1;
		if (well != src.layers.end()) {
			for (auto l1 = well->second.lbl.begin(); l1 != well->second.lbl.end(); l1++) {
				if (l1->net >= 0) {
					globalBase = l1->net;
					break;
				}
			}
		}

		vector<int> vias = src.tech->findVias(flip(modelID), 1);
		if (vias.empty()) {
			printf("error: transistor model has no contact definitions\n");
			continue;
		}

		auto via = src.find(src.tech->vias[vias[0]].draw);
		if (via == src.layers.end()) {
			continue;
		}
		
		// find the via layer associated with this model
		// overlap that with the vias to determine net names

		Layer gates = poly->second & diff;
		gates.merge();
		vector<Layer> ports = (diff & ~poly->second).split();
		vector<int> portIDs;
		for (auto p = ports.begin(); p != ports.end(); p++) {
			bool found = false;
			for (auto r0 = via->second.geo.begin(); r0 != via->second.geo.end(); r0++) {
				if (p->overlaps(*r0) and r0->net >= 0) {
					portIDs.push_back(r0->net);
					found = true;
					break;
				}
			}
			if (not found) {
				portIDs.push_back(dst.push(Net("_" + to_string(dst.nets.size()), false)));
			}
		}

		for (auto r0 = gates.geo.begin(); r0 != gates.geo.end(); r0++) {
			int gate = r0->net;
			vector<int> port;
			for (int i = 0; i < (int)ports.size(); i++) {
				if (ports[i].overlaps(*r0)) {
					port.push_back(portIDs[i]);
				}
			}

			int base = -1;
			if (well != src.layers.end()) {
				for (auto r1 = well->second.geo.begin(); r1 != well->second.geo.end(); r1++) {
					if (r1->overlaps(*r0) and r1->net >= 0) {
						base = r1->net;
						break;
					}
				}
			}
			if (base < 0) {
				base = globalBase;
			}
			if (base < 0) {
				base = dst.push(Net("_" + to_string(dst.nets.size()), false));
				globalBase = base;
			}

			while ((int)port.size() < 2) {
				port.push_back(dst.push(Net("_" + to_string(dst.nets.size()), false)));
			}
			dst.push(Mos(*src.tech, modelID, model->type, port[0], gate, port[1], base, r0->ur-r0->ll));
		}
	}

	dst.cleanDangling();

	return true;
}

bool extract(Netlist &net, phy::Library &lib) {
	bool result = true;
	int start = (int)net.subckts.size();
	net.subckts.resize(start+(int)lib.macros.size());
	for (int i = 0; i < (int)lib.macros.size(); i++) {
		result = extract(net.subckts[start+i], lib.macros[i]) and result;
		net.subckts[start+i].canonicalize();
	}
	return result;
}

}
