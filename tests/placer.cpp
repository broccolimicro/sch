#include <gtest/gtest.h>

#include <sch/Placer.h>

using namespace sch;
using namespace std;

TEST(placer, solve)
{
	Subckt ckt;
	ckt.name = "test";
	// Create an inverter
	int gnd = ckt.push(sch::Net("GND", true));
	int vdd = ckt.push(sch::Net("Vdd", true));
	int a = ckt.push(sch::Net("a"));
	int b = ckt.push(sch::Net("b"));
	ckt.push(Mos(-1, Model::NMOS, b, a, gnd));
	ckt.push(Mos(-1, Model::PMOS, b, a, vdd));

	// Run the placer
	Placement result = Placement::solve(ckt);
}


