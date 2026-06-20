#pragma once

#include <CL/opencl.hpp>
#include "Subckt.h"

namespace sch {

struct Placer;

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
	vector<cl_ulong> hilbert;

	vector<int> inst;
	vector<int> subckts;

	// TODO(edward.bingham) For development purposes only, delete this
	vector<string> netNames;

	int pushNet(string name);
	void allocPorts(cl_uint count);
	void pushCell(int subckt, vec2i bound, cl_ulong pos);
	void pushCell(int subckt, cl_uint2 bound, cl_ulong pos);
	void pushPorts(vector<int> ports, cl_uint cell);
	void pushPorts(int port, cl_uint cell);
	void finish();
	bool isCell() const;

	size_t numCells() const;
	size_t numNets() const;

	void print(const Placer &placer) const;
};

struct Implementation {
	const sch::Subckt *ckt;
	phy::Layout *macro;
	Mapping<int> cktToMacro;
};

struct Linker {
	Linker();
	virtual ~Linker() = 0;

	virtual Implementation find(std::string type) = 0;
};

struct Placer {
	Placer();
	Placer(Linker *linker, int platformId=0, int deviceId=0, bool progress=false, bool debug=false);
	~Placer();

	cl::Context context;
	cl::CommandQueue queue;
	cl::Program program;
	cl::Kernel globalStep;
	cl::Kernel detailStep0;
	cl::Kernel detailStep1;
	cl::Kernel partitionCols;
	cl::Kernel partitionRows;

	Linker *linker;

	vector<Schematic> schem;
	vector<Implementation> procs;
	map<std::string, int> table;

	bool progress;
	bool debug;

	// Configure the OpenCL Driver and Kernel
	void configure(int platformId=0, int deviceId=0);
	void configurePath(string kernalPath, int platformId=0, int deviceId=0);
	void configureSource(string source, int platformId=0, int deviceId=0);

	void load(Linker *linker);

	// Load a design into the placer
	int find(std::string type);

	bool elaborateSchematicInstances(int curr);
	void elaborateSchematicNets(int curr);
	void elaborateSchematicInstance(int curr, int sub);
	bool elaborateSchematic(int curr);
	bool elaborate(int top);

	cl_uint isqrt(cl_ulong x);
	vector<cl_uint> computeOffsets(int curr, const vector<int> &index);
	cl_uint computeHPWL(int curr, const vector<cl_uint> &offset);
	vector<int> computeOrder(int subckt, int starts=10, float step=2.0, float rate=0.02);

	void place(int subckt);
};

struct Placement {
	Placement();
	Placement(Placer &placer, int root);
	~Placement();

	Placer *placer;
	int root;

	// x-coord, y-coord, index
	vector<cl_uint3> position;

	// DETAIL ROUTING
	// mean x-coord, mean y-coord, variance x-axis, variance y-axis
	// sum(i=0 to (ceil(log_4(n))-1) of 4^i) = 1/3 (4^ceiling(log(n)/log(4)) - 1) elems
	vector<cl_uint4> cluster;
	// total cell area in cluster, total of (cell area^2) in cluster
	vector<cl_uint2> clusterArea;

	vector<cl_uint4> wirelse;

	// LEGALIZATION
	// x-coord, y-coord, cell index
	vector<cl_uint3> grid;
	
	cl::Buffer positionBuffer;
	cl::Buffer clusterBuffer;
	cl::Buffer gridBuffer;

	template <typename T>
	size_t bufferSize(const vector<T> &v) {
		return v.size() * sizeof(T);
	}

	bool init(Placer &placer, int root);

	// Run the placement algorithm
	void doGlobal();
	void doDetail();
	void doLegal();

	void solve();

	// Save the result to the layout library
	void save(phy::Layout &layout);
};

}
