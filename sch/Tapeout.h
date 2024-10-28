#pragma once

#include "Netlist.h"
#include <phy/Library.h>

namespace sch {

int routeCell(phy::Library &lib, Netlist &lst, int idx, bool progress=false, bool debug=false);
bool extract(Subckt &dst, Layout &src);
bool extract(Netlist &net, phy::Library &lib);

}
