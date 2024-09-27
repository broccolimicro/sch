#include <gtest/gtest.h>

#include <sch/Placer.h>

using namespace sch;
using namespace std;

TEST(placer, solve)
{
	Subckt ckt;
	ckt.name = "test";
	int gnd = (int)ckt.nets.size();
	ckt.nets.push_back(Net("GND"));
	int vdd = (int)ckt.nets.size();
	ckt.nets.push_back(Net("Vdd"));
	int a = (int)ckt.nets.size();
	ckt.nets.push_back(Net("a"));
	int b = (int)ckt.nets.size();
	ckt.nets.push_back(Net("b"));
	ckt.mos.push_back(Mos(-1, Model::NMOS));
	ckt.mos.back().gate = a;
	ckt.mos.back().source = gnd;
	ckt.mos.back().drain = b;
	ckt.mos.back().bulk = gnd;
	ckt.mos.back().size = vec2i(1.0, 1.0);
	ckt.mos.push_back(Mos(-1, Model::PMOS));
	ckt.mos.back().gate = a;
	ckt.mos.back().source = vdd;
	ckt.mos.back().drain = b;
	ckt.mos.back().bulk = vdd;
	ckt.mos.back().size = vec2i(1.0, 1.0);

	Placement result = Placement::solve(&ckt);
}


