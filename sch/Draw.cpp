#include "Draw.h"
#include <limits>

using namespace std;

namespace sch {

int clamp(int value, int lo, int hi) {
	return value < lo ? lo : (value > hi ? hi : value);
}

void drawTransistor(Layout &dst, const Mos &mos, vec2i pos, vec2i dir) {
	// We want the transistors to be oriented left to right, so the
	// long dimension of poly overhang should be vertical.

	auto model = dst.tech->models.begin()+mos.model;
	Rect gate(-1, pos, pos+mos.size*dir);

	int polyLayer = dst.tech->wires[0].draw;
	int diffLayer = dst.tech->at(model->diff).draw;
	
	vec2i polyOverhang = max(dst.tech->getEnclosing(polyLayer, diffLayer), 0);
	vec2i diffOverhang = max(dst.tech->getEnclosing(diffLayer, polyLayer), 0);
	diffOverhang.swap(0, 1);

	// draw poly
	Rect poly(mos.gate, gate.ll, gate.ur);
	poly.grow(polyOverhang);
	dst.push(Level(Level::ROUTE, 0), poly, mos.base, 1);

	// draw diffusion
	Rect diff = gate;
	diff.grow(diffOverhang);
	dst.push(model->diff, diff, mos.base, 0);
}

void drawVia(Layout &dst, int net, int base, int viaLevel, vec2i axis, vec2i size, bool expand, vec2i pos, vec2i dir) {
	int viaLayer = dst.tech->vias[viaLevel].draw;
	Level downLevel = dst.tech->vias[viaLevel].down;
	Level upLevel = dst.tech->vias[viaLevel].up;

	// spacing and width of a via
	int viaWidth = dst.tech->getWidth(viaLayer);
	int viaSpacing = dst.tech->getSpacing(viaLayer, viaLayer);

	// enclosure rules and default orientation
	vec2i dn = dst.tech->getEnclosing(dst.tech->at(downLevel).draw, viaLayer);
	if (axis[0] == 0) {
		dn.swap(0,1);
	}
	vec2i up = dst.tech->getEnclosing(dst.tech->at(upLevel).draw, viaLayer);
	if (axis[1] == 0) {
		up.swap(0,1);
	}
	
	vec2i num = max(1 + (size-viaWidth - 2*dn) / (viaSpacing + viaWidth), 1);
	vec2i width = num * viaWidth + (num-1)*viaSpacing;
	vec2i off = max((size-width)/2, 0);

	// Special rule for diffusion vias
	if (downLevel.type == Level::SUBST) {
		if (off[1] >= dn[1] and off[0] < dn[1]) {
			dn.swap(0,1);
		}
		dn[1] = off[1];
	} else if (off[axis[0]] < dn[axis[0]] and off[1-axis[0]] >= dn[axis[0]]) {
		dn.swap(0,1);
	}

	if (off[axis[1]] < up[axis[1]] and off[1-axis[1]] >= up[axis[1]]) {
		up.swap(0,1);
	}

	if (expand) {
		if (downLevel.type != Level::SUBST) {
			dn = max(dn, off);
		}
		if (upLevel.type != Level::SUBST) {
			up = max(up, off);
		}
	}

	// draw down
	vec2i ll = pos+(off-dn)*dir;
	vec2i ur = pos+(off+width+dn)*dir;
	int downNet = net;
	if (downLevel.type == Level::SUBST) {
		downNet = -1;
	}
	dst.push(downLevel, Rect(downNet, ll, ur), base);

	// draw via
	vec2i idx;
	int step = viaWidth+viaSpacing;
	for (idx[0] = 0; idx[0] < num[0]; idx[0]++) {
		for (idx[1] = 0; idx[1] < num[1]; idx[1]++) {
			dst.push(Level(Level::VIA, viaLevel), Rect(net, pos+(off+idx*step)*dir, pos+(off+idx*step+viaWidth)*dir), base);
		}
	}

	// draw up
	ll = pos+(off-up)*dir;
	ur = pos+(off+width+up)*dir;
	int upNet = net;
	if (upLevel.type == Level::SUBST) {
		upNet = -1;
	}
	dst.push(upLevel, Rect(upNet, ll, ur), base);
}

void drawViaStack(Layout &dst, int net, int base, Level down, Level up, vec2i axis, vec2i size, vec2i pos, vec2i dir) {
	if (down == up) {
		int width = dst.tech->getWidth(dst.tech->at(down).draw);
		size[0] = max(size[0], width);
		size[1] = max(size[1], width);
		int downNet = net;
		if (down.type == Level::SUBST) {
			downNet = -1;
		}
		dst.push(down, Rect(downNet, pos, pos+size*dir), base);
		return;
	}

	vector<int> vias = dst.tech->via(down, up);
	for (int i = 0; i < (int)vias.size(); i++) {
		drawVia(dst, net, base, vias[i], axis, size, true, pos, dir);
	}
}

void drawWire(Layout &dst, const CellRouter &rt, const Wire &wire, vec2i pos, vec2i dir) {
	// [via level][pin]
	vector<vector<int> > posArr;
	posArr.resize(dst.tech->vias.size());

	for (int i = 0; i < (int)dst.tech->vias.size(); i++) {
		posArr[i].reserve(wire.pins.size());

		for (int j = 0; j < (int)wire.pins.size(); j++) {
			const Pin &pin = rt.pin(wire.pins[j].idx);

			Level pinLevel = pin.layer;
			Level prevLevel = wire.getLevel(j-1);
			Level nextLevel = wire.getLevel(j);

			int viaPos = pin.offset[0];
			if (pinLevel != prevLevel or pinLevel != nextLevel) {
				viaPos = clamp(viaPos, wire.pins[j].offset[0], wire.pins[j].bound[0]);
			}
			//if (wire.pins[j].offset[0] > rightOfCell-wire.pins[j].offset[1]) {
				//printf("error: pin violation on pin %d\n", j);
				//printf("pinPos=%d left=%d right=%d viaPos=%d\n", pin.pos, wire.pins[j].left, wire.pins[j].right, viaPos);
			//}

			posArr[i].push_back(viaPos);
		}
	}

	for (int i = 0; i < (int)dst.tech->vias.size(); i++) {
		vector<pair<Layout, Rect> > vias;
		vias.reserve(wire.pins.size());
		int height = 0;
		for (int j = 0; j < (int)wire.pins.size(); j++) {
			// TODO(edward.bingham) use the via position to help
			// determine the location of the vertical route. Make the
			// vertical route width wider when possible to route from
			// the pin to the via.
			const Pin &pin = rt.pin(wire.pins[j].idx);
			Level pinLevel = pin.layer;
			int pinLayer = dst.tech->at(pinLevel).draw;
			Level prevLevel = wire.getLevel(j-1);
			Level nextLevel = wire.getLevel(j);
			Level wireLow = min(nextLevel, prevLevel);
			Level wireHigh = max(nextLevel, prevLevel);

			int wireLayer = dst.tech->at(nextLevel).draw;
			int minSpacing = dst.tech->getSpacing(pinLayer, pinLayer);
			height = dst.tech->getWidth(wireLayer);

			if ((pinLevel <= dst.tech->vias[i].down and wireHigh >= dst.tech->vias[i].up) or
			    (wireLow <= dst.tech->vias[i].down and pinLevel >= dst.tech->vias[i].up)) {
				// Draw the horizontal wire from the pin to the via
				int width = dst.tech->getWidth(pinLayer);

				vec2i axis(0,0);
				//if (wireLow <= dst.tech->vias[i].downLevel and wireHigh >= dst.tech->vias[i].downLevel and j > 0 and j < (int)wire.pins.size()-1) {
				//	axis[0] = 0;
				//}
				//if (wireLow <= dst.tech->vias[i].upLevel and wireHigh >= dst.tech->vias[i].upLevel and j > 0 and j < (int)wire.pins.size()-1) {
				//	axis[1] = 0;
				//}

				// Draw the via
				vec2i viall(posArr[i][j], 0);
				vec2i viasz(width, height);
				vec2i viaur = viall+viasz;
				Layout next(*dst.tech);
				drawVia(next, wire.net, pin.baseNet, i, axis, viasz, true, viall);
				auto layer = next.find(pinLayer);
				if (layer != next.layers.end()) {
					// TODO(edward.bingham) This draws the wire from the pin
					// to the via, that wire is made to be the same
					// thickness as the via. However, we might want to make
					// that wire min width once we get past the min spacing
					// and/or notch size rules.
					Rect box = layer->second.box;

					vec2i ll(min(pin.offset[0], viall[0]), 0);
					vec2i ur(max(pin.offset[0]+width, viaur[0]), width);
					if (wire.pins[j].idx.type == Model::PMOS and box.ll[0] < pin.offset[0]+width+minSpacing and pin.offset[0] < box.ur[0]+minSpacing) {
						ll[1] = box.ll[1];
					}
					if (wire.pins[j].idx.type == Model::NMOS and box.ll[0] < pin.offset[0]+width+minSpacing and pin.offset[0]-minSpacing < box.ur[0]) {
						ur[1] = box.ur[1];
					}

					dst.push(pinLevel, Rect(wire.net, ll, ur), pin.baseNet);
				}

				int off = numeric_limits<int>::min();
				// Check if we need to merge the vias
				if (not vias.empty() and minOffset(&off, 0, vias.back().first, 0, next, 0, Layout::IGNORE, Layout::DEFAULT) and off > 0) {
					Rect box = vias.back().second.bound(Rect(-1, viall, viaur));
					vias.back().first.clear();
					drawVia(vias.back().first, wire.net, pin.baseNet, i, axis, vec2i(box.ur[0]-box.ll[0], height), true, vec2i(box.ll[0], 0));
					vias.back().second = box;
				} else {
					vias.push_back(pair<Layout, Rect>(next, Rect(-1, viall, viaur)));
				}
			}
		}

		// add all of the new vias to the final layout
		for (int i = 0; i < (int)vias.size(); i++) {
			drawLayout(dst, vias[i].first, pos, dir);
		}
	}

	// TODO(edward.bingham) We need to create pin locations on each wire for the
	// inputs and outputs.

	for (int i = 0; i < (int)wire.pins.size(); i++) {
		const Pin &pin = rt.pin(wire.pins[i].idx);
		Level prevLevel = wire.getLevel(i);
		Level nextLevel = wire.getLevel(i+1);
		int height = dst.tech->getWidth(dst.tech->at(prevLevel).draw);

		int left = numeric_limits<int>::min();
		int right = numeric_limits<int>::max();
		for (int j = 0; j < (int)dst.tech->vias.size(); j++) {
			if (dst.tech->vias[j].down == prevLevel or dst.tech->vias[j].up == prevLevel) {
				left = max(left, posArr[j][i]);
			}
			if (dst.tech->vias[j].down == nextLevel or dst.tech->vias[j].up == nextLevel) {
				if (i+1 < (int)wire.pins.size()) {
					right = min(right, posArr[j][i+1]);
				} else {
					right = min(right, posArr[j][i]+height);
				}
			}
		}

		vec2i ll = pos+vec2i(left, 0)*dir;
		vec2i ur = pos+vec2i(right, height)*dir;
		dst.push(prevLevel, Rect(wire.net, ll, ur), pin.baseNet);
	}
}

void drawPin(Layout &dst, const Subckt &ckt, const Stack &stack, int pinID, vec2i pos, vec2i dir) {
	const Pin &pin = stack.pins[pinID];

	pos[0] += pin.offset[0];
	if (pin.isContact()) {
		int model = -1;
		if (pinID >= 1 and stack.pins[pinID-1].isGate()) {
			model = ckt.mos[stack.pins[pinID-1].device].model;
		} else if (pinID+1 < (int)stack.pins.size() and stack.pins[pinID+1].isGate()) {
			model = ckt.mos[stack.pins[pinID+1].device].model;
		}

		if (model >= 0) {
			drawViaStack(dst, pin.outNet, pin.baseNet, dst.tech->models[model].diff, Level(Level::ROUTE, 1), vec2i(1,1), vec2i(pin.width, pin.height), pos, dir);
		} else {
			pos[0] += pin.offset[0];
			Level level = pin.layer;
			int width = dst.tech->getWidth(dst.tech->at(level).draw);
			dst.push(level, Rect(pin.outNet, pos, pos+vec2i(width, width)*dir), pin.baseNet);
		}
	} else {
		drawTransistor(dst, ckt.mos[pin.device], pos, dir);
	}
}

void drawStack(Layout &dst, const Subckt &ckt, const Stack &stack) {
	dst.clear();
	// Draw the stacks
	for (auto i = stack.pins.begin(); i != stack.pins.end(); i++) {
		vec2i dir = vec2i(1, stack.type == Model::NMOS ? -1 : 1);
		drawLayout(dst, i->layout, vec2i(i->offset[0], 0), dir);
		if (i != stack.pins.begin() and (i->isGate() or (i-1)->isGate())) {
			int height = min(i->height, (i-1)->height);
			int idx = -1;
			if (i->isGate()) {
				idx = ckt.mos[i->device].model;
			} else if ((i-1)->isGate()) {
				idx = ckt.mos[(i-1)->device].model;
			} else {
				continue;
			}
			auto model = dst.tech->models.begin()+idx;

			Rect rect(-1,
			  vec2i((i-1)->offset[0], 0),
			  vec2i(i->offset[0], height)*dir);

			dst.push(model->diff, rect, i->baseNet);
		}
	}
}

void drawCell(Layout &dst, const CellRouter &rt) {
	vec2i dir(1,-1);
	dst.name = rt.ckt->name;

	dst.nets.reserve(rt.ckt->nets.size());
	for (int i = 0; i < (int)rt.ckt->nets.size(); i++) {
		dst.nets.push_back(rt.ckt->nets[i].name);
		dst.nets.back().isInput = rt.ckt->nets[i].remoteIO and rt.ckt->nets[i].isInput();
		dst.nets.back().isOutput = rt.ckt->nets[i].remoteIO and rt.ckt->nets[i].isOutput();
		// TODO(edward.bingham) information about power and ground
	}

	for (auto i = rt.routes.begin(); i != rt.routes.end(); i++) {
		//if ((int)i->pins.size() > 1) {
		drawLayout(dst, i->layout, vec2i(0, i->offset[Model::PMOS])*dir, dir);
		//}
	}

	for (int type = 0; type < (int)rt.stack.size(); type++) {
		for (int i = 0; i < (int)rt.stack[type].pins.size(); i++) {
			const Pin &pin = rt.stack[type].pins[i];
			bool first = true;
			int bottom = 0;
			int top = 0;

			Level pinLevel = pin.layer;
			int pinLayer = dst.tech->at(pinLevel).draw;
			int width = dst.tech->getWidth(pinLayer);
			//int minSpacing = dst.tech->getSpacing(pinLayer, pinLayer);

			//vector<Layer> layers;
			for (auto j = rt.routes.begin(); j != rt.routes.end(); j++) {
				if (j->hasPin(&rt, Index(type, i))) {
					auto layer = j->layout.find(pinLayer);
					if (layer != j->layout.layers.end()) {
						//Layer l = layer->clamp(0, pin.pos, pin.pos+pin.width);
						//l.shift(vec2i(0, j->offset[Model::PMOS])*dir, dir);
						//layers.push_back(l);

						int v = j->offset[Model::PMOS];
						if (j->net >= 0) {
							top = first ? v+width : max(top, v+width);
						} else {
							top = first ? v : max(top, v);
						}
						bottom = first ? v : min(bottom, v);
						first = false;
					}
				}
			}

			// TODO(edward.bingham) determine if route is within minimum
			// separation distance. If so, then intersect the route
			// layout with the bounds of the pin. Then use that
			// intersection to determine the bounds of the vertical
			// route from the pin to the wire.
			/*for (auto j0 = layers.begin(); j0 != layers.end(); j0++) {
				for (auto j1 = j0+1; j1 != layers.end(); j1++) {
					for (auto r0 = j0->geo.begin(); r0 != j0->geo.end(); r0++) {
						for (auto r1 = j1->geo.begin(); r1 != j1->geo.end(); r1++) {
							if (r1->ur[1] < r0->ll[1] and r0->ll[1] - r1->ur[1] < minSpacing) {
								dst.push(dst.tech->wires[pinLevel], Rect(pin.outNet, vec2i(max(r1->ll[0], r0->ll[0]), r1->ur[1]), vec2i(min(r1->ur[0], r0->ur[0]), r0->ll[1])));
							} else if (r0->ur[1] < r1->ll[1] and r1->ll[1] - r0->ur[1] < minSpacing) {
								dst.push(dst.tech->wires[pinLevel], Rect(pin.outNet, vec2i(max(r0->ll[0], r1->ll[0]), r0->ur[1]), vec2i(min(r0->ur[0], r1->ur[0]), r1->ll[1])));
							}
						}
					}
				}
			}*/

			// Draw the vertical route from the pin to the wire.
 			dst.push(pinLevel, Rect(pin.outNet, vec2i(pin.offset[0], bottom)*dir, vec2i(pin.offset[0]+width, top)*dir), pin.baseNet);
		}
	}

	// fill in min-spacing violations between two wires on the same layer connected to the same net.
	for (auto layer = dst.layers.begin(); layer != dst.layers.end(); layer++) {
		if (layer->second.isRouting or (layer->first >= 0 and dst.tech->paint[layer->first].fill)) {
			layer->second.fillSpacing();
		}
	}

	if (dst.tech->boundary >= 0) {
		dst.push(dst.tech->boundary, dst.box); 
	}

	dst.merge();

	// Find best place to put the pin for the ports
	vector<bool> labelled;
	labelled.resize(rt.ckt->nets.size(), false);
	for (auto wire = dst.tech->wires.rbegin(); wire != dst.tech->wires.rend(); wire++) {
		auto layer = dst.find(wire->draw);
		if (layer != dst.layers.end()) {
			for (auto r = layer->second.geo.begin(); r != layer->second.geo.end(); r++) {
				if (r->net >= 0 and not labelled[r->net]) {
					labelled[r->net] = true;
					dst.label(wire->label, Label(r->net, r->center(), rt.ckt->nets[r->net].name));
					if (find(rt.ckt->ports.begin(), rt.ckt->ports.end(), r->net) != rt.ckt->ports.end()) {
						dst.push(wire->pin, *r);
					}
				}
			}
		}
	}

	for (auto sub = dst.tech->subst.begin(); sub != dst.tech->subst.end(); sub++) {
		if (sub->draw < 0) {
			auto layer = dst.find(sub->label);
			if (layer != dst.layers.end()) {
				for (auto r = layer->second.geo.begin(); r != layer->second.geo.end(); r++) {
					if (r->net >= 0) {
						dst.label(sub->label, Label(r->net, r->center(), rt.ckt->nets[r->net].name));
						// substrate pins are only allowed to be drawn on a welltap. This is not a welltap
						//if (find(rt.ckt->ports.begin(), rt.ckt->ports.end(), r->net) != rt.ckt->ports.end()) {
						//	dst.push(sub->pin, *r);
						//}
					}
				}
			}
		} else {
			auto layer = dst.find(sub->draw);
			if (layer != dst.layers.end()) {
				for (auto r = layer->second.geo.begin(); r != layer->second.geo.end(); r++) {
					if (r->net >= 0) {
						dst.label(sub->label, Label(r->net, r->center(), rt.ckt->nets[r->net].name));
						// substrate pins are only allowed to be drawn on a welltap. This is not a welltap
						//if (find(rt.ckt->ports.begin(), rt.ckt->ports.end(), r->net) != rt.ckt->ports.end()) {
						//	dst.push(sub->pin, *r);
						//}
					}
				}
			}
		}
	}

	dst.shift_inplace(-dst.box.center());
	/*for (auto l = dst.layers.begin(); l != dst.layers.end(); l++) {
		if (l->second.isWell) {
			l->second.print();
		}
	}*/

	/*printf("cell: (%d %d) (%d %d)\n", dst.box.ll[0], dst.box.ll[1], dst.box.ur[0], dst.box.ur[1]);
	Rect box;
	for (auto l = dst.layers.begin(); l != dst.layers.end(); l++) {
		for (auto r = l->second.geo.begin(); r != l->second.geo.end(); r++) {
			box.bound(*r);
		}
	}
	printf("computed: (%d %d) (%d %d)\n", box.ll[0], box.ll[1], box.ur[0], box.ur[1]);*/
}

void drawLayout(Layout &dst, const Layout &src, vec2i pos, vec2i dir) {
	dst.box.bound(src.box.shift(pos, dir));
	for (auto layer = src.layers.begin(); layer != src.layers.end(); layer++) {
		auto dstLayer = dst.at(layer->first);
		for (auto r = layer->second.geo.begin(); r != layer->second.geo.end(); r++) {
			dstLayer->second.push(r->shift(pos, dir));
		}
	}
}

}
