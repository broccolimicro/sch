#include "Draw.h"
#include <limits>

using namespace std;

namespace sch {

int clamp(int value, int lo, int hi) {
	return value < lo ? lo : (value > hi ? hi : value);
}

void drawDiffusion(Layout &dst, int model, int net, vec2i ll, vec2i ur, vec2i dir) {
	int curr = -1;
	for (auto layer = dst.tech->models[model].stack.begin(); layer != dst.tech->models[model].stack.end(); layer++) {
		int prev = curr;
		curr = dst.tech->subst[flip(*layer)].draw;

		if (prev < 0) {
			dst.box.bound(ll, ur);
		} else {
			vec2i overhang = max(dst.tech->getEnclosing(curr, prev), 0);
			// This is a diffusion, so we want the long dimension along the x-axis to
			// reduce cell height at the expense of cell length. This reduces total
			// layout area.
			overhang.swap(0,1);
			
			ll -= overhang*dir;
			ur += overhang*dir;
		}
		
		dst.push(dst.tech->subst[flip(*layer)], Rect(-1, ll, ur));
	}
}

void drawTransistor(Layout &dst, const Mos &mos, bool flip, vec2i pos, vec2i dir) {
	vec2i ll = pos;
	vec2i ur = pos + mos.size*dir;

	int poly = dst.tech->wires[0].draw;
	int diff = dst.tech->subst[::flip(dst.tech->models[mos.model].stack[0])].draw;

	// We want the transistors to be oriented left to right, so the long
	// dimension of poly overhang should be vertical.
	vec2i polyOverhang = max(dst.tech->getEnclosing(poly, diff), 0)*dir;

	// draw poly
	dst.box.bound(ll - polyOverhang, ur + polyOverhang);
	dst.push(dst.tech->wires[0], Rect(mos.gate, ll - polyOverhang, ur + polyOverhang));

	// draw diffusion
	int curr = poly;
	for (auto layer = dst.tech->models[mos.model].stack.begin(); layer != dst.tech->models[mos.model].stack.end(); layer++) {
		int prev = curr;
		curr = dst.tech->subst[::flip(*layer)].draw;
		vec2i diffOverhang = max(dst.tech->getEnclosing(curr, prev), 0);
		// We want the transistors to be oriented left to right, so the long
		// dimension of diff overhang should be horizontal.
		diffOverhang.swap(0,1);
		diffOverhang *= dir;

		ll -= diffOverhang;
		ur += diffOverhang;
		if (prev == poly) {
			// first layer should be the diffusion layer
			dst.box.bound(ll, ur);
		}
		int net = -1;
		if (dst.tech->subst[::flip(*layer)].isWell) {
			net = mos.base;
		}
		dst.push(dst.tech->subst[::flip(*layer)], Rect(net, ll, ur));
	}
}

void drawVia(Layout &dst, int net, int base, int viaLevel, vec2i axis, vec2i size, bool expand, vec2i pos, vec2i dir) {
	int viaLayer = dst.tech->vias[viaLevel].draw;
	int downLevel = dst.tech->vias[viaLevel].downLevel;
	int upLevel = dst.tech->vias[viaLevel].upLevel;
	int downLayer = downLevel < 0 ? dst.tech->subst[flip(dst.tech->models[flip(downLevel)].stack[0])].draw : dst.tech->wires[downLevel].draw;
	int upLayer = upLevel < 0 ? dst.tech->subst[flip(dst.tech->models[flip(upLevel)].stack[0])].draw : dst.tech->wires[upLevel].draw;

	// spacing and width of a via
	int viaWidth = dst.tech->paint[viaLayer].minWidth;
	int viaSpacing = dst.tech->getSpacing(viaLayer, viaLayer);

	// enclosure rules and default orientation
	vec2i dn = dst.tech->getEnclosing(downLayer, viaLayer);
	if (axis[0] == 0) {
		dn.swap(0,1);
	}
	vec2i up = dst.tech->getEnclosing(upLayer, viaLayer);
	if (axis[1] == 0) {
		up.swap(0,1);
	}
	
	vec2i num = max(1 + (size-viaWidth - 2*dn) / (viaSpacing + viaWidth), 1);
	vec2i width = num * viaWidth + (num-1)*viaSpacing;
	vec2i off = max((size-width)/2, 0);

	// Special rule for diffusion vias
	if (downLevel < 0) {
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
		if (downLevel >= 0) {
			dn = max(dn, off);
		}
		if (upLevel >= 0) {
			up = max(up, off);
		}
	}

	// draw down
	vec2i ll = pos+(off-dn)*dir;
	vec2i ur = pos+(off+width+dn)*dir;
	if (downLevel >= 0) {
		// routing level
		dst.box.bound(ll, ur);
		dst.push(dst.tech->wires[downLevel], Rect(net, ll, ur));
	} else {
		// diffusion level
		int model = -downLevel-1;

		int curr = -1;
		for (auto layer = dst.tech->models[model].stack.begin(); layer != dst.tech->models[model].stack.end(); layer++) {
			int prev = curr;
			curr = dst.tech->subst[flip(*layer)].draw;

			if (prev == -1) {
				dst.box.bound(ll, ur);
			} else {
				vec2i overhang = max(dst.tech->getEnclosing(curr, prev), 0);
				// This is a diffusion, so we want the long dimension along the x-axis to
				// reduce cell height at the expense of cell length. This reduces total
				// layout area.
				overhang.swap(0,1);

				ll -= overhang*dir;
				ur += overhang*dir;
			}
			int rnet = -1;
			if (dst.tech->subst[flip(*layer)].isWell) {
				rnet = base;
			}
			dst.push(dst.tech->subst[flip(*layer)], Rect(rnet, ll, ur));
		}
	}

	// draw via
	vec2i idx;
	int step = viaWidth+viaSpacing;
	for (idx[0] = 0; idx[0] < num[0]; idx[0]++) {
		for (idx[1] = 0; idx[1] < num[1]; idx[1]++) {
			dst.push(dst.tech->vias[viaLevel], Rect(net, pos+(off+idx*step)*dir, pos+(off+idx*step+viaWidth)*dir));
		}
	}

	// draw up
	ll = pos+(off-up)*dir;
	ur = pos+(off+width+up)*dir;
	if (upLevel >= 0) {
		// routing level
		dst.box.bound(ll, ur);
		dst.push(dst.tech->wires[upLevel], Rect(net, ll, ur));
	} else {
		// diffusion level
		int model = -upLevel-1;

		int curr = -1;
		for (auto layer = dst.tech->models[model].stack.begin(); layer != dst.tech->models[model].stack.end(); layer++) {
			int prev = curr;
			curr = dst.tech->subst[flip(*layer)].draw;

			if (prev == -1) {
				dst.box.bound(ll, ur);
			} else {
				vec2i overhang = max(dst.tech->getEnclosing(curr, prev), 0);
				// This is a diffusion, so we want the long dimension along the x-axis to
				// reduce cell height at the expense of cell length. This reduces total
				// layout area.
				overhang.swap(0,1);

				ll -= overhang*dir;
				ur += overhang*dir;
			}
			int rnet = -1;
			if (dst.tech->subst[flip(*layer)].isWell) {
				rnet = base;
			}
			dst.push(dst.tech->subst[flip(*layer)], Rect(rnet, ll, ur));
		}
	}
}

void drawViaStack(Layout &dst, int net, int base, int downLevel, int upLevel, vec2i axis, vec2i size, vec2i pos, vec2i dir) {
	if (downLevel == upLevel) {
		int layer = dst.tech->wires[downLevel].draw;
		int width = dst.tech->paint[layer].minWidth;
		size[0] = max(size[0], width);
		size[1] = max(size[1], width);
		dst.push(dst.tech->wires[downLevel], Rect(net, pos, pos+size*dir));
		return;
	}

	vector<int> vias = dst.tech->findVias(downLevel, upLevel);
	for (int i = 0; i < (int)vias.size(); i++) {
		drawVia(dst, net, base, vias[i], axis, size, true, pos, dir);
	}
}

void drawWire(Layout &dst, const Router &rt, const Wire &wire, vec2i pos, vec2i dir) {
	// [via level][pin]
	vector<vector<int> > posArr;
	posArr.resize(dst.tech->vias.size());

	for (int i = 0; i < (int)dst.tech->vias.size(); i++) {
		posArr[i].reserve(wire.pins.size());

		for (int j = 0; j < (int)wire.pins.size(); j++) {
			const Pin &pin = rt.pin(wire.pins[j].idx);

			int pinLevel = pin.layer;
			int prevLevel = wire.getLevel(j-1);
			int nextLevel = wire.getLevel(j);

			int viaPos = pin.pos;
			if (pinLevel != prevLevel or pinLevel != nextLevel) {
				viaPos = clamp(viaPos, wire.pins[j].left, wire.pins[j].right);
			}
			if (wire.pins[j].left > wire.pins[j].right) {
				//printf("error: pin violation on pin %d\n", j);
				//printf("pinPos=%d left=%d right=%d viaPos=%d\n", pin.pos, wire.pins[j].left, wire.pins[j].right, viaPos);
			}

			posArr[i].push_back(viaPos);
		}
	}

	for (int i = 0; i < (int)dst.tech->vias.size(); i++) {
		vector<Layout> vias;
		vias.reserve(wire.pins.size());
		int height = 0;
		for (int j = 0; j < (int)wire.pins.size(); j++) {
			// TODO(edward.bingham) use the via position to help
			// determine the location of the vertical route. Make the
			// vertical route width wider when possible to route from
			// the pin to the via.
			const Pin &pin = rt.pin(wire.pins[j].idx);
			int pinLevel = pin.layer;
			int pinLayer = dst.tech->wires[pinLevel].draw;
			int prevLevel = wire.getLevel(j-1);
			int nextLevel = wire.getLevel(j);
			int wireLow = min(nextLevel, prevLevel);
			int wireHigh = max(nextLevel, prevLevel);

			int wireLayer = dst.tech->wires[nextLevel].draw;
			int minSpacing = dst.tech->getSpacing(pinLayer, pinLayer);
			height = dst.tech->paint[wireLayer].minWidth;

			if ((pinLevel <= dst.tech->vias[i].downLevel and wireHigh >= dst.tech->vias[i].upLevel) or
			    (wireLow <= dst.tech->vias[i].downLevel and pinLevel >= dst.tech->vias[i].upLevel)) {
				// Draw the horizontal wire from the pin to the via
				int width = dst.tech->paint[pinLayer].minWidth;

				vec2i axis(0,0);
				//if (wireLow <= dst.tech->vias[i].downLevel and wireHigh >= dst.tech->vias[i].downLevel and j > 0 and j < (int)wire.pins.size()-1) {
				//	axis[0] = 0;
				//}
				//if (wireLow <= dst.tech->vias[i].upLevel and wireHigh >= dst.tech->vias[i].upLevel and j > 0 and j < (int)wire.pins.size()-1) {
				//	axis[1] = 0;
				//}

				// Draw the via
				Layout next(*dst.tech);
				drawVia(next, wire.net, pin.baseNet, i, axis, vec2i(width, height), true, vec2i(posArr[i][j], 0));
				auto layer = next.find(pinLayer);
				if (layer != next.layers.end()) {
					// TODO(edward.bingham) This draws the wire from the pin
					// to the via, that wire is made to be the same
					// thickness as the via. However, we might want to make
					// that wire min width once we get past the min spacing
					// and/or notch size rules.
					Rect bbox = layer->bbox();
					if (wire.pins[j].idx.type == Model::PMOS and bbox.ll[0] < pin.pos+width+minSpacing and pin.pos < bbox.ur[0]+minSpacing) {
						dst.push(dst.tech->wires[pinLevel], Rect(wire.net, vec2i(pin.pos, bbox.ll[1]), vec2i(posArr[i][j], width)));
					} else if (wire.pins[j].idx.type == Model::NMOS and bbox.ll[0] < pin.pos+width+minSpacing and pin.pos-minSpacing < bbox.ur[0]) {
						dst.push(dst.tech->wires[pinLevel], Rect(wire.net, vec2i(pin.pos, 0), vec2i(posArr[i][j], bbox.ur[1])));
					} else {
						dst.push(dst.tech->wires[pinLevel], Rect(wire.net, vec2i(pin.pos, 0), vec2i(posArr[i][j], width)));
					}
				}

				int off = numeric_limits<int>::min();
				// Check if we need to merge the vias
				if (not vias.empty() and minOffset(&off, 0, vias.back(), 0, next, 0, Layout::IGNORE, Layout::DEFAULT) and off > 0) {
					Rect box = vias.back().box.bound(next.box);
					vias.back().clear();
					drawVia(vias.back(), wire.net, pin.baseNet, i, axis, vec2i(box.ur[0]-box.ll[0], height), true, vec2i(box.ll[0], 0));
				} else {
					vias.push_back(next);
				}
			}
		}

		// add all of the new vias to the final layout
		for (int i = 0; i < (int)vias.size(); i++) {
			drawLayout(dst, vias[i], pos, dir);
		}
	}

	// TODO(edward.bingham) We need to create pin locations on each wire for the
	// inputs and outputs.

	for (int i = 1; i < (int)wire.pins.size(); i++) {
		int prevLevel = wire.getLevel(i-1);
		int nextLevel = wire.getLevel(i);

		int left = numeric_limits<int>::min();
		int right = numeric_limits<int>::max();
		for (int j = 0; j < (int)dst.tech->vias.size(); j++) {
			if (dst.tech->vias[j].downLevel == prevLevel or dst.tech->vias[j].upLevel == prevLevel) {
				left = max(left, posArr[j][i-1]);
			}
			if (dst.tech->vias[j].downLevel == nextLevel or dst.tech->vias[j].upLevel == nextLevel) {
				right = min(right, posArr[j][i]);
			}
		}

		int height = dst.tech->paint[dst.tech->wires[prevLevel].draw].minWidth;
		vec2i ll = pos+vec2i(left, 0)*dir;
		vec2i ur = pos+vec2i(right, height)*dir;
		dst.push(dst.tech->wires[prevLevel], Rect(wire.net, ll, ur));
	}
}

void drawPin(Layout &dst, const Subckt &ckt, const Stack &stack, int pinID, vec2i pos, vec2i dir) {
	pos[0] += stack.pins[pinID].pos;
	if (stack.pins[pinID].isContact()) {
		int model = -1;
		for (int i = pinID-1; i >= 0 and model < 0; i--) {
			if (stack.pins[i].isGate()) {
				model = ckt.mos[stack.pins[i].device].model;
			}
		}

		for (int i = pinID+1; i < (int)stack.pins.size() and model < 0; i++) {
			if (stack.pins[i].isGate()) {
				model = ckt.mos[stack.pins[i].device].model;
			}
		}

		if (model >= 0) {
			drawViaStack(dst, stack.pins[pinID].outNet, stack.pins[pinID].baseNet, -model-1, 1, vec2i(1,1), vec2i(stack.pins[pinID].width, stack.pins[pinID].height), pos, dir);
		} else {
			const Pin &pin = stack.pins[pinID];
			pos[0] += pin.pos;
			int level = pin.layer;
			int layer = dst.tech->wires[level].draw;
			int width = dst.tech->paint[layer].minWidth;
			vec2i size(width, width);
			dst.push(dst.tech->wires[level], Rect(pin.outNet, pos, pos+size*dir));
		}
	} else {
		drawTransistor(dst, ckt.mos[stack.pins[pinID].device], stack.pins[pinID].leftNet != ckt.mos[stack.pins[pinID].device].source, pos, dir);
	}
}

void drawStack(Layout &dst, const Subckt &ckt, const Stack &stack) {
	dst.clear();
	// Draw the stacks
	for (auto i = stack.pins.begin(); i != stack.pins.end(); i++) {
		vec2i dir = vec2i(1, stack.type == Model::NMOS ? -1 : 1);
		drawLayout(dst, i->layout, vec2i(i->pos, 0), dir);
		if (i != stack.pins.begin() and (i->device >= 0 or (i-1)->device >= 0)) {
			int height = min(i->height, (i-1)->height);
			int model = -1;
			if (i->device >= 0) {
				model = ckt.mos[i->device].model;
			} else {
				model = ckt.mos[(i-1)->device].model;
			}

			drawDiffusion(dst, model, -1, vec2i((i-1)->pos, 0), vec2i(i->pos, height)*dir, dir);
		}
	}
}

void drawCell(Layout &dst, const Router &rt) {
	vec2i dir(1,-1);
	dst.name = rt.ckt->name;

	dst.nets.reserve(rt.ckt->nets.size());
	for (int i = 0; i < (int)rt.ckt->nets.size(); i++) {
		dst.nets.push_back(Port(rt.ckt->nets[i].name));
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
			
			int pinLevel = pin.layer;
			int pinLayer = dst.tech->wires[pinLevel].draw;
			int width = dst.tech->paint[pinLayer].minWidth;
			//int minSpacing = dst.tech->getSpacing(pinLayer, pinLayer);

			vector<Layer> layers;
			for (auto j = rt.routes.begin(); j != rt.routes.end(); j++) {
				if (j->hasPin(&rt, Index(type, i))) {
					auto layer = j->layout.find(pinLayer);
					if (layer != j->layout.layers.end()) {
						Layer l = layer->clamp(0, pin.pos, pin.pos+pin.width);
						l.shift(vec2i(0, j->offset[Model::PMOS])*dir, dir);
						layers.push_back(l);

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
 			dst.push(dst.tech->wires[pinLevel], Rect(pin.outNet, vec2i(pin.pos, bottom)*dir, vec2i(pin.pos+width, top)*dir));
		}
	}

	// fill in min-spacing violations between two wires on the same layer connected to the same net.
	for (auto layer = dst.layers.begin(); layer != dst.layers.end(); layer++) {
		if (layer->isRouting) {
			layer->fillSpacing();
		}
	}

	for (int i = 0; i < (int)dst.layers.size(); i++) {
		if (dst.layers[i].draw >= 0 and dst.tech->paint[dst.layers[i].draw].fill) {
			Rect box = dst.layers[i].bbox();
			dst.layers[i].clear();
			dst.layers[i].push(box, true);
		}
	}

	Rect box = dst.bbox();
	if (dst.tech->boundary >= 0) {
		dst.push(dst.tech->boundary, box); 
	}

	dst.merge();

	/*nlab = dst.tech->findPaint("nwell.label");
	plab = dst.tech->findPaint("pwell.label");
	nwell = dst.tech->findPaint("nwell.drawing");*/	

	// Find best place to put the pin for the ports
	vector<bool> nets;
	nets.resize(rt.ckt->nets.size(), false);
	for (int i = (int)dst.tech->wires.size()-1; i >= 0; i--) {
		auto layer = dst.find(dst.tech->wires[i].draw);
		if (layer != dst.layers.end()) {
			for (int j = (int)layer->geo.size()-1; j >= 0; j--) {
				auto r = layer->geo.begin()+j;
				for (int k = 0; k < (int)rt.ckt->nets.size(); k++) {
					if (not nets[k] and r->net == k) {
						dst.nets[k].label = dst.tech->wires[i].draw;
						if (find(rt.ckt->ports.begin(), rt.ckt->ports.end(), k) != rt.ckt->ports.end()) {
							dst.push(dst.tech->wires[i].pin, *r);
							dst.nets[k].label = dst.tech->wires[i].label;
						}
						dst.nets[k].pos = (r->ll+r->ur)/2;
						nets[k] = true;
					}
				}
			}
		}
	}

	for (int i = 0; i < (int)dst.tech->subst.size(); i++) {
		auto layer = dst.find(dst.tech->subst[i].draw);
		if (layer != dst.layers.end()) {
			for (int j = (int)layer->geo.size()-1; j >= 0; j--) {
				auto r = layer->geo.begin()+j;
				for (int k = 0; k < (int)rt.ckt->nets.size(); k++) {
					if (not nets[k] and r->net == k) {
						dst.nets[k].label = dst.tech->subst[i].draw;
						if (find(rt.ckt->ports.begin(), rt.ckt->ports.end(), k) != rt.ckt->ports.end()) {
							dst.push(dst.tech->subst[i].pin, *r);
							dst.nets[k].label = dst.tech->subst[i].label;
						}
						dst.nets[k].pos = (r->ll+r->ur)/2;
						nets[k] = true;
					}
				}
			}
		}
	}
}

void drawLayout(Layout &dst, const Layout &src, vec2i pos, vec2i dir) {
	dst.box.bound(src.box.ll*dir + pos, src.box.ur*dir+pos);
	for (auto layer = src.layers.begin(); layer != src.layers.end(); layer++) {
		auto dstLayer = dst.at(layer->draw);
		for (int i = 0; i < (int)layer->geo.size(); i++) {
			dstLayer->push(layer->geo[i].shift(pos, dir));
		}
	}
}

}
