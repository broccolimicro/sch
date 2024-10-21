#pragma once

#include <phy/Tech.h>
#include <phy/Layout.h>
#include <phy/vector.h>

#include <iostream>

#include "Router.h"

using namespace phy;

namespace sch {

void drawDiffusion(Layout &dst, int model, int net, vec2i ll, vec2i ur, vec2i dir);
void drawTransistor(Layout &dst, const Mos &mos, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1)); 
void drawVia(Layout &dst, int net, int base, int viaLevel, vec2i axis, vec2i size=vec2i(0,0), bool expand=true, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawViaStack(Layout &dst, int net, int base, int downLevel, int upLevel, vec2i axis, vec2i size=vec2i(0,0), vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawWire(Layout &dst, const Router &rt, const Wire &wire, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawPin(Layout &dst, const Subckt &ckt, const Stack &stack, int pinID, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawStack(Layout &dst, const Subckt &ckt, const Stack &stack);
void drawCell(Layout &dst, const Router &rt);
void drawLayout(Layout &dst, const Layout &src, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

}
