#pragma once

#include "Netlist.h"
#include <phy/Library.h>

namespace sch {

int buildCell(phy::Library &lib, Netlist &lst, int idx, bool progress=false, bool debug=false);
int buildProcess(phy::Library &lib, Netlist &lst, int idx, bool progress=false, bool debug=false);
bool extract(Subckt &dst, Layout &src, bool forceTrace=false);
bool extract(Netlist &net, phy::Library &lib, bool forceTrace=false);

}
