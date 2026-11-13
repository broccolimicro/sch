#pragma once

#include <phy/Tech.h>
#include <phy/Layout.h>
#include <phy/vector.h>

#include <iostream>

#include "CellRouter.h"

using namespace phy;

namespace sch {

// Build physical layout geometry from transistors, pins, and routes.
// Where supplied, pos sets the drawing origin and dir controls reflection
// along each axis. Use 1 to keep an axis and -1 to reflect it.

// Draw the polysilicon gate and diffusion for mos. Use mos.size for
// the channel dimensions and the technology rules for layer overhangs.
void drawTransistor(Layout &dst, const Mos &mos, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1)); 

// Draw a via cut array and its lower and upper enclosures. Size the array
// from the requested footprint and the technology rules. The two axis
// entries control enclosure orientation on the lower and upper layers.
// If expand is true, enlarge non-substrate enclosures to fill the footprint.
void drawVia(Layout &dst, int net, int base, int viaLevel, vec2i axis, vec2i size=vec2i(0,0), bool expand=true, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

// Draw the technology-defined via stack between down and up. If both
// levels are equal, draw one rectangle at least as wide as the layer minimum
// in each dimension.
void drawViaStack(Layout &dst, int net, int base, Level down, Level up, vec2i axis, vec2i size=vec2i(0,0), vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

// Draw the horizontal segments of wire and the vias that connect them
// to each other and to the pins. Use the pin offsets and route layers.
void drawWire(Layout &dst, const CellRouter &rt, const Wire &wire, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

// Draw a stack pin at its horizontal offset. Draw a transistor for a gate
// pin, a via stack for a diffusion contact, or a minimum-width square if
// the contact has no neighboring gate.
void drawPin(Layout &dst, const Subckt &ckt, const Stack &stack, int pinID, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

// Clear dst and draw the stack from its stored pin layouts. Reflect the
// NMOS stack vertically and fill diffusion between neighboring pins
// when at least one is a gate.
void drawStack(Layout &dst, const Subckt &ckt, const Stack &stack);

// Assemble the cell from the router's route layouts and pin connections.
// Copy the cell and net names, set the net I/O flags, and center the
// resulting geometry at the origin.
void drawCell(Layout &dst, const CellRouter &rt);

// Append transformed rectangles from src to the corresponding layers in
// dst. Expand the destination bounding box to include the source geometry.
void drawLayout(Layout &dst, const Layout &src, vec2i pos=vec2i(0,0), vec2i dir=vec2i(1,1));

}
