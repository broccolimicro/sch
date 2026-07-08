#include <gtest/gtest.h>

#include <sch/Placer.h>
#include <sch/Subckt.h>
#include <phy/Layout.h>

using namespace sch;
using namespace std;

struct Netlist {
	vector<Subckt> subckts;
	vector<Mapping<int> > toLayout;
};

struct Library {
	vector<Layout> macros;
};

struct TestLinker : sch::Linker {
	const phy::Tech &tech;
	Netlist &lst;
	Library &lib;

	TestLinker(const phy::Tech &tech, Netlist &lst, Library &lib) : tech(tech), lst(lst), lib(lib) {
	}
	~TestLinker() {}

	sch::Implementation find(const sch::Instance &inst) override {
		sch::Implementation result;
		for (Subckt &ckt : lst.subckts) {
			if (ckt.name == inst.type) {
				result.ckt = &ckt;
			}
		}
		for (Layout &macro : lib.macros) {
			if (macro.name == inst.type) {
				result.macro = &macro;
			}
		}

		if (result.macro == nullptr) {
			lib.macros.push_back(Layout(tech));
			result.macro = &lib.macros.back();
		}
		
		if (result.ckt != nullptr and result.macro != nullptr) {
			result.cktToMacro = result.ckt->mapToLayout(*result.macro);
		}
		return result;
	}
};

TEST(Placer, solve)
{
	Tech tech;
	Netlist lst;
	lst.subckts.resize(3);
	lst.subckts[0].name = "ckt0";
	lst.subckts[0].push(sch::Net("a", true));
	lst.subckts[0].push(sch::Net("b", true));

	lst.subckts[1].name = "ckt1";
	lst.subckts[1].push(sch::Net("a", true));
	lst.subckts[1].push(sch::Net("b", false));
	lst.subckts[1].push(sch::Net("c", true));
	lst.subckts[1].push(sch::Net("d", false));
	lst.subckts[1].push(sch::Instance(std::string("ckt0"), {0, 1}));
	lst.subckts[1].push(sch::Instance(std::string("ckt0"), {1, 2}));
	lst.subckts[1].push(sch::Instance(std::string("ckt0"), {2, 3}));
	lst.subckts[1].push(sch::Instance(std::string("ckt0"), {3, 0}));

	lst.subckts[2].name = "ckt2";
	lst.subckts[2].push(sch::Net("a", true));
	lst.subckts[2].push(sch::Net("b", false));
	lst.subckts[2].push(sch::Net("c", true));
	lst.subckts[2].push(sch::Net("d", false));
	lst.subckts[2].push(sch::Instance(std::string("ckt1"), {0, 1}));
	lst.subckts[2].push(sch::Instance(std::string("ckt1"), {1, 2}));
	lst.subckts[2].push(sch::Instance(std::string("ckt1"), {2, 3}));
	lst.subckts[2].push(sch::Instance(std::string("ckt1"), {3, 0}));

	lst.toLayout.resize(3, Mapping<int>(-1, true));

	Library lib;
	lib.macros.reserve(10);
	lib.macros.push_back(Layout(tech));
	lib.macros.back().box = Rect(-1, vec2i(0, 0), vec2i(10, 10));

	// Run the placer
	TestLinker linker(tech, lst, lib);
	Placer placer(&linker, 0, 0, true);
	placer.load(sch::Implementation(&lst.subckts[2], &lib.macros.back()));
	Placement result(placer, 0);
	result.solve();
	result.save(lib.macros[0]);

	for (const phy::Instance &i : lib.macros[0].inst) {
		cout << "at " << i.macro << " = " << i.pos << " " << i.dir << endl;
	}
}


