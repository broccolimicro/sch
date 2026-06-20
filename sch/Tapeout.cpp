#include "Tapeout.h"

#include "Draw.h"
#include "CellPlacer.h"
#include "CellRouter.h"
#include "Placer.h"

#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include <filesystem>

#include <common/text.h>

#include <chrono>
using namespace std::chrono;

using namespace std;
using namespace phy;

namespace sch {

vector<Subckt> mapCells(const Tech &tech, Subckt &ckt, bool progress) {
	std::vector<Subckt> cells;
	if (ckt.isCell and not ckt.mos.empty()) {
		ckt.canonicalize();
		return cells;
	} else if (ckt.mos.empty()) {
		return cells;
	}

	ckt.splitDevices(tech);

	auto segments = ckt.segment();
	cells.reserve(segments.size());
	for (auto s = segments.begin(); s != segments.end(); s++) {
		int index = cells.size();
		cells.push_back(Subckt(true));
		Mapping<int> m = s->generate(cells.back(), ckt);
		m *= cells.back().canonicalize();
		// TODO(edward.bingham) clean dangling?
		cells.back().name = "cell_" + encodeBase32(index);

		ckt.extract(*s);
		ckt.push(Instance(cells.back(), m.flip()));

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
		}
	}

	ckt.cleanDangling();
	return cells;
}

int buildCell(Layout &dst, Subckt &src, bool progress, bool debug) {
	bool place = true;
	bool route = true;
	CellPlacement pl = CellPlacement::solve(*dst.tech, src);
	CellRouter rt(*dst.tech, pl, progress, debug);
	route = rt.solve();
	drawCell(dst, rt);
	rt.annotateAreaPerim(src);
	if (not place) {
		return 1;
	} else if (not route) {
		return 2;
	}
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

}
