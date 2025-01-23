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

	// Each value is an index of the start of the port list for the net in
	// Schematic::inputs (plus one element at end to facilitate iteration)
	vector<cl_uint> nets;

	// Each value is an index of a net that is connected to this cell in
	// Schematic::nets.
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
};

struct Placement {
	Placement();
	~Placement();

	// 1. treat cells as particles
	// 2. each gets a position, velocity, acceleration, and force
	// 3. wires between cells are springs
	//   a. lock the "springs" along the vertical and horizontal axes.
	//  ----O      O
	//  |          |
	//  O      O----
	// Use OpenCL to run the simulation for millions of particles
	// Simulated Annealing
	// 1. Cells start very light and springs very springy
	// 2. As time goes on, add more and more damping

	// export ascii file with cell locations?


	// TODO(edward.bingham) For development purposes only, delete this
	const Netlist *lst;

	cl::Context context;
	cl::CommandQueue queue;
	cl::Program program;
	cl::Kernel initPlacement;
	cl::Kernel stepPlacement;

	int root;
	vector<Schematic> schem;

	// total cell area of bin
	vector<cl_uint2> position;
	vector<cl_uint> index;

	// Configure the OpenCL Driver and Kernel
	void configure(int platformId=0, int deviceId=0, bool debug=false);
	void configurePath(string kernalPath, int platformId=0, int deviceId=0, bool debug=false);
	void configureSource(string source, int platformId=0, int deviceId=0, bool debug=false);

	// Load a design into the placer
	void load(const phy::Library &lib, const Netlist &lst, int root, bool debug=false);
	
	// Run the placement algorithm
	cl_uint isqrt(cl_uint x);
	vector<cl_uint> hierComputeOffsets(const Subckt &ckt, const vector<int> &index);
	cl_uint hierComputeHPWL(const Subckt &ckt, const vector<cl_uint> &offset);
	vector<int> doHier(const Subckt &ckt, int starts=10, float step=2.0, float rate=0.02);
	void doGlobal();
	void doDetail();
	void doLegal();

	// Save the result to the layout library
	void save(phy::Library &lib, const sch::Netlist &lst);
};

}
