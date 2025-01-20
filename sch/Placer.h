#pragma once

#include "Netlist.h"
#include <phy/Library.h>

#include <CL/opencl.hpp>

namespace sch {

struct Schematic {
	Schematic();
	~Schematic();

	uint64_t cells;

	// Each value in ports is index of cell that is connected to this net.
	vector<uint64_t> ports;

	// Each value in nets is index of start of port list for net in
	// Schematic::ports
	vector<size_t> nets;
};

struct Placement {
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

	cl::Context context;
	cl::CommandQueue queue;
	cl::Program program;
	cl::Kernel kernel;

	int root;
	vector<Schematic> schem;
	vector<cl_float2> position;
	vector<cl_float2> velocity;
	vector<cl_float2> force;

	// Configure the OpenCL Driver and Kernel
	void configure(int platformId=0, int deviceId=0, bool debug=false);
	void configurePath(string kernelName, string kernalPath, int platformId=0, int deviceId=0, bool debug=false);
	void configureSource(string kernelName, string source, int platformId=0, int deviceId=0, bool debug=false);

	// Load a design into the placer
	void load(const Netlist &lst, int root, bool debug=false);
	
	// Run the placement algorithm
	void run();

	// Save the result to the layout library
	void save(phy::Layout &layout, const sch::Netlist &lst);
};

}
