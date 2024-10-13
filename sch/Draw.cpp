#include "Draw.h"
#include <limits>

using namespace std;

namespace sch {

int clamp(int value, int lo, int hi) {
	return value < lo ? lo : (value > hi ? hi : value);
}

void drawDiffusion(Layout &dst, int model, int net, vec2i ll, vec2i ur, vec2i dir) {
	for (int i = 0; i < (int)dst.tech->models[model].paint.size(); i++) {
		if (i != 0) {
			ll -= dst.tech->models[model].paint[i].overhang*dir;
			ur += dst.tech->models[model].paint[i].overhang*dir;
		}
		if (i == 0) {
			dst.box.bound(ll, ur);
		}
		dst.push(dst.tech->models[model].paint[i], Rect(-1, ll, ur));
	}
}

void drawTransistor(Layout &dst, const Mos &mos, bool flip, vec2i pos, vec2i dir) {
	vec2i ll = pos;
	vec2i ur = pos + mos.size*dir;

	// draw poly
	vec2i polyOverhang = vec2i(0, dst.tech->models[mos.model].polyOverhang)*dir;
	dst.box.bound(ll - polyOverhang, ur + polyOverhang);
	dst.push(dst.tech->wires[0], Rect(mos.gate, ll - polyOverhang, ur + polyOverhang));

	// draw diffusion
	for (auto layer = dst.tech->models[mos.model].paint.begin(); layer != dst.tech->models[mos.model].paint.end(); layer++) {
		vec2i diffOverhang = layer->overhang*dir;
		ll -= diffOverhang;
		ur += diffOverhang;
		bool isDiffusion = layer == dst.tech->models[mos.model].paint.begin();
		if (isDiffusion) {
			dst.box.bound(ll, ur);
		}
		dst.push(*layer, Rect(-1, ll, ur));
	}
}

void drawVia(Layout &dst, int net, int viaLevel, vec2i axis, vec2i size, bool expand, vec2i pos, vec2i dir) {
	int viaLayer = dst.tech->vias[viaLevel].draw;
	int downLevel = dst.tech->vias[viaLevel].downLevel;
	int upLevel = dst.tech->vias[viaLevel].upLevel;

	// spacing and width of a via
	int viaWidth = dst.tech->paint[viaLayer].minWidth;
	int rule = dst.tech->getSpacing(viaLayer, viaLayer);
	int viaSpacing = 0;
	if (rule < 0) {
		viaSpacing = dst.tech->rules[flip(rule)].params[0];
	}

	// enclosure rules and default orientation
	vec2i dn = dst.tech->vias[viaLevel].dn;
	if (axis[0] == 0) {
		dn.swap(0,1);
	}
	vec2i up = dst.tech->vias[viaLevel].up;
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
		for (int i = 0; i < (int)dst.tech->models[model].paint.size(); i++) {
			if (i != 0) {
				ll -= dst.tech->models[model].paint[i].overhang*dir;
				ur += dst.tech->models[model].paint[i].overhang*dir;
			}
			if (i == 0) {
				dst.box.bound(ll, ur);
			}
			dst.push(dst.tech->models[model].paint[i], Rect(-1, ll, ur));
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
		for (int i = 0; i < (int)dst.tech->models[model].paint.size(); i++) {
			if (i != 0) {
				ll -= dst.tech->models[model].paint[i].overhang*dir;
				ur += dst.tech->models[model].paint[i].overhang*dir;
			}
			if (i == 0) {
				dst.box.bound(ll, ur);
			}
			dst.push(dst.tech->models[model].paint[i], Rect(-1, ll, ur));
		}
	}
}

void drawViaStack(Layout &dst, int net, int downLevel, int upLevel, vec2i axis, vec2i size, vec2i pos, vec2i dir) {
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
		drawVia(dst, net, vias[i], axis, size, true, pos, dir);
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
				printf("error: pin violation on pin %d\n", j);
				printf("pinPos=%d left=%d right=%d viaPos=%d\n", pin.pos, wire.pins[j].left, wire.pins[j].right, viaPos);
			}

			posArr[i].push_back(viaPos);
		}
	}

	for (int i = 0; i < (int)dst.tech->vias.size(); i++) {
		vector<Layout> vias;
		vias.reserve(wire.pins.size());
		int height = 0;
		for (int j = 0; j < (int)wire.pins.size(); j++) {
			const Pin &pin = rt.pin(wire.pins[j].idx);
			int pinLevel = pin.layer;
			int prevLevel = wire.getLevel(j-1);
			int nextLevel = wire.getLevel(j);
			int wireLow = min(nextLevel, prevLevel);
			int wireHigh = max(nextLevel, prevLevel);

			int wireLayer = dst.tech->wires[nextLevel].draw;
			height = dst.tech->paint[wireLayer].minWidth;

			if ((pinLevel <= dst.tech->vias[i].downLevel and wireHigh >= dst.tech->vias[i].upLevel) or
			    (wireLow <= dst.tech->vias[i].downLevel and pinLevel >= dst.tech->vias[i].upLevel)) {
				int width = dst.tech->paint[dst.tech->wires[pinLevel].draw].minWidth;
				dst.push(dst.tech->wires[pinLevel], Rect(wire.net, vec2i(pin.pos, 0), vec2i(posArr[i][j], width)));

				vec2i axis(0,0);
				//if (wireLow <= dst.tech->vias[i].downLevel and wireHigh >= dst.tech->vias[i].downLevel and j > 0 and j < (int)wire.pins.size()-1) {
				//	axis[0] = 0;
				//}
				//if (wireLow <= dst.tech->vias[i].upLevel and wireHigh >= dst.tech->vias[i].upLevel and j > 0 and j < (int)wire.pins.size()-1) {
				//	axis[1] = 0;
				//}

				Layout next(*dst.tech);
				drawVia(next, wire.net, i, axis, vec2i(width, height), true, vec2i(posArr[i][j], 0));
				int off = numeric_limits<int>::min();
				if (not vias.empty() and minOffset(&off, 0, vias.back(), 0, next, 0, Layout::IGNORE, Layout::DEFAULT) and off > 0) {
					Rect box = vias.back().box.bound(next.box);
					vias.back().clear();
					drawVia(vias.back(), wire.net, i, axis, vec2i(box.ur[0]-box.ll[0], height), true, vec2i(box.ll[0], 0));
				} else {
					vias.push_back(next);
				}
			}
		}

		for (int i = 0; i < (int)vias.size(); i++) {
			drawLayout(dst, vias[i], pos, dir);
		}
	}

	// TODO(edward.bingham) We need to check every pin on this wire to every via
	// to make sure we haven't created a notch that will violate spacing rules.
	// If we do end up creating a notch, then we need to fill it in.

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
			drawViaStack(dst, stack.pins[pinID].outNet, -model-1, 1, vec2i(1,1), vec2i(stack.pins[pinID].width, stack.pins[pinID].height), pos, dir);
		} else {
			printf("error: unable to identify transistor model for diffusion contact %d\n", pinID);
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
		if ((int)i->pins.size() > 1) {
			drawLayout(dst, i->layout, vec2i(0, i->offset[Model::PMOS])*dir, dir);
		}
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

			for (auto j = rt.routes.begin(); j != rt.routes.end(); j++) {
				if (j->hasPin(&rt, Index(type, i))) {
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

 			dst.push(dst.tech->wires[pinLevel], Rect(pin.outNet, vec2i(pin.pos, bottom)*dir, vec2i(pin.pos+width, top)*dir));
		}
	}

	for (int i = 0; i < (int)dst.layers.size(); i++) {
		if (dst.tech->paint[dst.layers[i].draw].fill) {
			Rect box = dst.layers[i].bbox();
			dst.layers[i].clear();
			dst.layers[i].push(box, true);
		}
	}

	if (dst.tech->boundary >= 0) {
		dst.push(dst.tech->boundary, dst.bbox()); 
	}

	dst.merge();
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
