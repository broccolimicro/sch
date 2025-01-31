#pragma once

#include "Netlist.h"
#include <phy/Library.h>

#include <CL/opencl.hpp>

namespace sch {

struct Schematic {
	Schematic();
	~Schematic();

	// Each value is an index of a cell that is connected to this net in
	// Schematic::cells.
	vector<cl_uint> netsToCells;

	// All of the nets of the root circuit are listed first, then we recurse.

	// Each value is an index of the start of the port list for the net in
	// Schematic::inputs (plus one element at end to facilitate iteration)
	vector<cl_uint> nets;

	// Each value is an index of a net in port order that is connected
	// to this cell in Schematic::nets.
	vector<cl_uint> cellsToNets;

	// Each value is an index of the start of the port list for the cell in
	// Schematic::cellsToNets (plus one element at end to facilitate iteration)
	vector<cl_uint> cells;

	cl_ulong totalArea;
	// Each value is the area of a particular cell.
	vector<cl_uint2> cellBounds;
	vector<cl_uint> hilbert;

	vector<int> subckts;

	// TODO(edward.bingham) For development purposes only, delete this
	vector<string> netNames;

	int pushNet(string name);
	void allocPorts(cl_uint count);
	void pushCell(int subckt, vec2i bound, cl_uint pos);
	void pushCell(int subckt, cl_uint2 bound, cl_uint pos);
	void pushPorts(vector<int> ports, cl_uint cell);
	void pushPorts(int port, cl_uint cell);
	void finish();
	bool isCell() const;

	size_t numCells() const;
	size_t numNets() const;

	void print(const Netlist &lst) const;
};

struct Placer {
	Placer();
	Placer(phy::Library &lib, const Netlist &lst, int platformId=0, int deviceId=0, bool debug=false);
	~Placer();

	cl::Context context;
	cl::CommandQueue queue;
	cl::Program program;
	cl::Kernel initPlacement;
	cl::Kernel stepPlacement;
	cl::Kernel partitionCols;
	cl::Kernel partitionRows;

	const sch::Netlist *lst;
	phy::Library *lib;

	vector<Schematic> schem;

	// Configure the OpenCL Driver and Kernel
	void configure(int platformId=0, int deviceId=0, bool debug=false);
	void configurePath(string kernalPath, int platformId=0, int deviceId=0, bool debug=false);
	void configureSource(string source, int platformId=0, int deviceId=0, bool debug=false);

	void load(phy::Library &lib, const Netlist &lst);

	// Load a design into the placer
	void elaborateSchematicNets(int curr, bool debug=false);
	void elaborateSchematicInstance(int curr, int sub, bool debug=false);
	void elaborateSchematic(int curr, bool debug=false);
	void elaborate(int subckt, bool debug=false);

	cl_uint isqrt(cl_uint x);
	vector<cl_uint> computeOffsets(int curr, const vector<int> &index);
	cl_uint computeHPWL(int curr, const vector<cl_uint> &offset);
	vector<int> computeOrder(int subckt, int starts=10, float step=2.0, float rate=0.02);
};

struct Placement {
	Placement();
	Placement(Placer &placer, int root);
	~Placement();

	Placer *placer;
	int root;
	Schematic *schem;

	// x-coord, y-coord, cell index
	vector<cl_uint3> grid;
	vector<cl_uint2> position;
	
	cl::Buffer positionBuffer;
	cl::Buffer gridBuffer;

	template <typename T>
	size_t bufferSize(const vector<T> &v) {
		return v.size() * sizeof(T);
	}

	void init(Placer &placer, int root);

	// Run the placement algorithm
	void doGlobal();
	void doDetail();
	void doLegal();

	void solve();

	// Save the result to the layout library
	void save();
	void save(phy::Layout &layout);
	void save(phy::Library &lib);
};

}
