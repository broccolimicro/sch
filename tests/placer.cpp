#include <gtest/gtest.h>

#include <sch/Placer.h>

using namespace sch;
using namespace std;

TEST(placer, solve)
{
	Subckt ckt;
	ckt.name = "test";
	ckt.nets.push_back(Net("GND"));
	ckt.nets.push_back(Net("Vdd"));
	ckt.nets.push_back(Net("a"));
	ckt.nets.push_back(Net("b"));
	ckt.mos.push_back(Mos(-1, Model::NMOS));
	ckt.mos.back().gate = 2;
	ckt.mos.back().ports = vector<int>({0, 3});
	ckt.mos.back().bulk = 0;
	ckt.mos.back().size = vec2i(1.0, 1.0);
	ckt.mos.push_back(Mos(-1, Model::PMOS));
	ckt.mos.back().gate = 2;
	ckt.mos.back().ports = vector<int>({1, 3});
	ckt.mos.back().bulk = 1;
	ckt.mos.back().size = vec2i(1.0, 1.0);

	Placement result = Placement::solve(&ckt);
}


