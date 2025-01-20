#include <gtest/gtest.h>

#include <sch/Placer.h>
#include <sch/Netlist.h>
#include <sch/Subckt.h>

using namespace sch;
using namespace std;

TEST(Placer, solve)
{
	Tech tech;
	Netlist lst(tech);
	lst.subckts.resize(2);
	lst.subckts[0].push(sch::Net("a", true));
	lst.subckts[0].push(sch::Net("b", true));

	lst.subckts[1].push(sch::Net("a", false));
	lst.subckts[1].push(sch::Net("b", false));
	lst.subckts[1].push(sch::Net("c", false));
	lst.subckts[1].push(sch::Net("d", false));
	lst.subckts[1].push(sch::Instance(0, {0, 1}));
	lst.subckts[1].push(sch::Instance(0, {1, 2}));
	lst.subckts[1].push(sch::Instance(0, {2, 3}));
	lst.subckts[1].push(sch::Instance(0, {3, 0}));

	// Run the placer
	Placement result;
	result.configure(0, 0, true);
	result.load(lst, 1, true);	
	result.run();

	for (int i = 0; i < 4; i++) {
		cout << "at " << i << " = " << lst.cellAt(1, i) << endl;
	}
}


