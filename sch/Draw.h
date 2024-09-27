#pragma once

#include <phy/Tech.h>
#include <phy/Layout.h>
#include <phy/vector.h>

#include <iostream>

#include "Router.h"

using namespace phy;

namespace sch {

void drawDiffusion(const Tech &tech, Layout &dst, int model, int net, vec2i ll, vec2i ur, vec2i dir);
void drawTransistor(const Tech &tech, Layout &dst, const Mos &mos, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1)); 
void drawVia(const Tech &tech, Layout &dst, int net, int viaLevel, vec2i axis, vec2i size=vec2i(0,0), bool expand=true, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawViaStack(const Tech &tech, Layout &dst, int net, int downLevel, int upLevel, vec2i axis, vec2i size=vec2i(0,0), vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawWire(const Tech &tech, Layout &dst, const Router &rt, const Wire &wire, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawPin(const Tech &tech, Layout &dst, const Subckt &ckt, const Stack &stack, int pinID, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));
void drawLayout(Layout &dst, const Layout &src, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

}
