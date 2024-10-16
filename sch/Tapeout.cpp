#include "Tapeout.h"

#include "Netlist.h"
#include "Draw.h"
#include "Placer.h"
#include "Router.h"

#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include <filesystem>

#include <chrono>
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
using namespace std::chrono;

using namespace std;
using namespace phy;


namespace sch {

int routeCell(phy::Library &lib, const Netlist &lst, int idx, bool progress, bool debug) {
	bool place = true;
	bool route = true;
	Placement pl = Placement::solve(lst.subckts[idx]);
	Router rt(*lib.tech, pl, progress, debug);
	route = rt.solve();
	drawCell(lib.macros[idx], rt);
	if (not place) {
		return 1;
	} else if (not route) {
		return 2;
	}
	return 0;
}

}
