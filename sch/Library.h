#pragma once

#include "Circuit.h"

#include <phy/Tech.h>
#include <interpret_rect/ActConfig.h>
#include <set>
#include <map>

using namespace std;

struct Library {
	Library();
	Library(const Tech &tech);
	~Library();

	const Tech *tech;

	string libPath;
	
	vector<Circuit> cells; 

	void build(set<string> cellNames = set<string>());

	void emitGDS(string libname, string filename, set<string> cellNames = set<string>());
	void emitRect(const act::ActConfig &act, string path, set<string> cellNames = set<string>());
};

