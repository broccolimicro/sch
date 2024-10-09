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

Layout &loadCell(Library &lib, const Netlist &lst, int idx, bool progress, bool debug) {
	string cellPath = lib.libPath + "/" + lst.subckts[idx].name+".gds";
	Layout cell(lib.tech);
	cell.name = lst.subckts[idx].name;
	if (progress) {
		printf("  %s...", cell.name.c_str());
		fflush(stdout);
	}
	if (not filesystem::exists(cellPath) or not import_layout(cell, lib.tech, cellPath, cell.name)) {
		bool place = true;
		bool route = true;
		Placement pl = Placement::solve(lst.subckts[idx]);
		Router rt(lib.tech, pl, progress, debug);
		route = rt.solve(lib.tech);
		drawCell(cell, rt);
		if (progress) {
			if (place and route) {
				printf("[%sGENERATED%s]\n", KGRN, KNRM);
			} else if (not place) {
				printf("[%sFAILED PLACEMENT%s]\n", KRED, KNRM);
			} else if (not route) {
				printf("[%sFAILED ROUTING%s]\n", KRED, KNRM);
			}
		}
	} else if (progress) {
		printf("[%sFOUND%s]\n", KGRN, KNRM);
	}
	if (idx >= (int)lib.macros.size()) {
		lib.macros.resize(idx+1, Layout(lib.tech));
	}
	lib.macros[idx] = cell;
	return lib.macros[idx];
}

void loadCells(Library &lib, const Netlist &lst, bool progress, bool debug) {
	if (progress) {
		printf("Load cell layouts:\n");
	}
	steady_clock::time_point start = steady_clock::now();
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (lst.subckts[i].isCell) {
			loadCell(lib, lst, i, progress);
		}
	}
	steady_clock::time_point finish = steady_clock::now();
	if (progress) {
		printf("done [%gs]\n\n", ((float)duration_cast<milliseconds>(finish - start).count())/1000.0);
	}
}

}
