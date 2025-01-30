#include <gtest/gtest.h>

#include <sch/Placer.h>
#include <sch/Netlist.h>
#include <sch/Subckt.h>
#include <phy/Library.h>
#include <phy/Layout.h>

using namespace sch;
using namespace std;

TEST(Placer, solve)
{
	Tech tech;
	Netlist lst(tech);
	lst.subckts.resize(3);
	lst.subckts[0].name = "ckt0";
	lst.subckts[0].push(sch::Net("a", true));
	lst.subckts[0].push(sch::Net("b", true));

	lst.subckts[1].name = "ckt1";
	lst.subckts[1].push(sch::Net("a", true));
	lst.subckts[1].push(sch::Net("b", false));
	lst.subckts[1].push(sch::Net("c", true));
	lst.subckts[1].push(sch::Net("d", false));
	lst.subckts[1].push(sch::Instance(0, {0, 1}));
	lst.subckts[1].push(sch::Instance(0, {1, 2}));
	lst.subckts[1].push(sch::Instance(0, {2, 3}));
	lst.subckts[1].push(sch::Instance(0, {3, 0}));

	lst.subckts[2].name = "ckt2";
	lst.subckts[2].push(sch::Net("a", true));
	lst.subckts[2].push(sch::Net("b", false));
	lst.subckts[2].push(sch::Net("c", true));
	lst.subckts[2].push(sch::Net("d", false));
	lst.subckts[2].push(sch::Instance(1, {0, 1}));
	lst.subckts[2].push(sch::Instance(1, {1, 2}));
	lst.subckts[2].push(sch::Instance(1, {2, 3}));
	lst.subckts[2].push(sch::Instance(1, {3, 0}));


	phy::Library lib(tech);
	lib.macros.resize(3, Layout(tech));
	lib.macros[0].box = Rect(-1, vec2i(0, 0), vec2i(10, 10));

	// Run the placer
	Placement result;
	result.configure(0, 0, true);
	result.load(lib, lst, 2, true);	
	result.doGlobal();
	//result.doDetail();
	result.doLegal(lib);

	result.save(lib, lst);

	for (int i = 0; i < 4; i++) {
		cout << "at " << i << " = " << lst.cellAt(1, i) << endl;
	}
}


