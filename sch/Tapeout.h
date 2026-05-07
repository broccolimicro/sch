#pragma once

#include "Subckt.h"
#include <phy/Tech.h>
#include <phy/Layout.h>

namespace sch {

vector<Subckt> mapCells(const Tech &tech, Subckt &ckt, bool progress=false);
int buildCell(Layout &dst, Subckt &src, bool progress=false, bool debug=false);
bool extract(Subckt &dst, Layout &src, bool forceTrace=false);

}
