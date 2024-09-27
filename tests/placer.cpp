#include <gtest/gtest.h>

#include <sch/Placer.h>

using namespace sch;
using namespace std;

TEST(placer, solve)
{
	Subckt ckt;
	ckt.name = "test";
	// Create an inverter
	int gnd = ckt.pushNet("GND", true);
	int vdd = ckt.pushNet("Vdd", true);
	int a = ckt.pushNet("a");
	int b = ckt.pushNet("b");
	ckt.pushMos(-1, Model::NMOS, b, a, gnd);
	ckt.pushMos(-1, Model::PMOS, b, a, vdd);

	// Run the placer
	Placement result = Placement::solve(&ckt);
}


