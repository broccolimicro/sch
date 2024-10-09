#pragma once

#include "Netlist.h"
#include <phy/Library.h>

namespace sch {

Layout &loadCell(phy::Library &lib, const Netlist &lst, int idx, bool progress=false, bool debug=false);
void loadCells(phy::Library &lib, const Netlist &lst, bool progress=false, bool debug=false);

}
