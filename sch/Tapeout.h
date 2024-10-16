#pragma once

#include "Netlist.h"
#include <phy/Library.h>

namespace sch {

int routeCell(phy::Library &lib, const Netlist &lst, int idx, bool progress=false, bool debug=false);

}
