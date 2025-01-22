#include "Tapeout.h"

#include "Netlist.h"
#include "Draw.h"
#include "CellPlacer.h"
#include "CellRouter.h"
#include "Placer.h"

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

int buildCell(phy::Library &lib, Netlist &lst, int idx, bool progress, bool debug) {
	bool place = true;
	bool route = true;
	CellPlacement pl = CellPlacement::solve(*lib.tech, lst.subckts[idx]);
	CellRouter rt(*lib.tech, pl, progress, debug);
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

int buildProcess(phy::Library &lib, Netlist &lst, int idx, bool progress, bool debug) {
	if ((int)lib.macros.size() < idx+1) {
		lib.macros.resize(idx+1, Layout(*lst.tech));
	}
	lib.macros[idx].name = lst.subckts[idx].name;
	Placement placer;
	placer.configure(0, 0, debug);
	placer.load(lib, lst, idx, debug);
	placer.doGlobal();
	placer.doDetail();
	//placer.run();
	placer.save(lib, lst);
	return 0;
}

bool extract(Subckt &dst, Layout &src, bool forceTrace) {
	if (forceTrace or src.nets.empty()) {
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

	if (src.tech->wires.empty()) {
		return false;
	}
	auto poly = src.find(src.tech->wires[0].draw);
	if (poly == src.layers.end()) {
		return false;
	}

	for (auto model = src.tech->models.begin(); model != src.tech->models.end(); model++) {
		int modelID = (model-src.tech->models.begin());
		const Substrate &diffMat = (const Substrate&)src.tech->at(model->diff);
		const Material &wellMat = src.tech->at(diffMat.well);

		Layer diff = src.get(model->diff);
		if (diff.empty()) {
			continue;
		}

		Layer well = src.get(diffMat.well);

		int globalBase = -1;
		if (not well.empty()) {
			for (auto l1 = well.lbl.begin(); l1 != well.lbl.end(); l1++) {
				if (l1->net >= 0) {
					globalBase = l1->net;
					break;
				}
			}
			// well is pin or label
			if (not wellMat.hasDraw()) {
				for (auto r0 = well.geo.begin(); r0 != well.geo.end(); r0++) {
					if (r0->net >= 0) {
						globalBase = r0->net;
						break;
					}
				}
			}
		}

		vector<int> vias = src.tech->via(model->diff, Level(Level::ROUTE, 1));
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

		vector<Layer> gates = ((poly->second & diff).merge()).split();
		vector<Layer> ports = ((diff & ~poly->second).merge()).split();
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

		for (auto g0 = gates.begin(); g0 != gates.end(); g0++) {
			vector<int> port;
			for (int i = 0; i < (int)ports.size(); i++) {
				if (ports[i].overlaps(*g0)) {
					port.push_back(portIDs[i]);
				}
			}

			int base = -1;
			if (not well.empty()) {
				if (not src.tech->isLabel(well.draw)) {
					for (auto r1 = well.geo.begin(); r1 != well.geo.end(); r1++) {
						if (g0->overlaps(*r1) and r1->net >= 0) {
							base = r1->net;
							break;
						}
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
			dst.push(Mos(*src.tech, modelID, model->type, port[0], g0->geo[0].net, port[1], base, g0->box.ur-g0->box.ll));
		}
	}

	dst.cleanDangling();

	return true;
}

bool extract(Netlist &net, phy::Library &lib, bool forceTrace) {
	bool result = true;
	int start = (int)net.subckts.size();
	net.subckts.resize(start+(int)lib.macros.size());
	for (int i = 0; i < (int)lib.macros.size(); i++) {
		result = extract(net.subckts[start+i], lib.macros[i], forceTrace) and result;
		net.subckts[start+i].canonicalize();
	}
	return result;
}

}
