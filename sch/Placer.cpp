#include "Placer.h"
#include "Kernel.h"
#include <limits>
#include <random>

#include <CL/opencl.hpp>

using namespace std;

namespace sch {

Schematic::Schematic() {
	totalArea = 0;
}

Schematic::~Schematic() {
}

int Schematic::pushNet(string name) {
	int result = (int)nets.size();
	nets.push_back(netsToCells.size());
	netNames.push_back(name);
	return result;
}

void Schematic::allocPorts(cl_uint count) {
	cl_uint blank = std::numeric_limits<cl_uint>::max();
	netsToCells.resize(netsToCells.size()+count, blank);
}

void Schematic::pushCell(int subckt, vec2i bound, cl_uint pos) {
	cells.push_back(cellsToNets.size());
	subckts.push_back(subckt);
	cellBounds.push_back({(cl_uint)bound[0], (cl_uint)bound[1]});
	hilbert.push_back(pos);
}

void Schematic::pushCell(int subckt, cl_uint2 bound, cl_uint pos) {
	cells.push_back(cellsToNets.size());
	subckts.push_back(subckt);
	cellBounds.push_back(bound);
	hilbert.push_back(pos);
}

void Schematic::pushPorts(vector<int> ports, cl_uint cell) {
	for (int i = 0; i < (int)ports.size(); i++) {
		pushPorts(ports[i], cell);
	}
}

void Schematic::pushPorts(int port, cl_uint cell) {
	cl_uint blank = std::numeric_limits<cl_uint>::max();
	size_t start = nets[port];
	size_t end = netsToCells.size();
	if (port+1 < (int)nets.size()) {
		end = nets[port+1];
	}
	for (size_t i = start; i < end; i++) {
		if (netsToCells[i] == blank) {
			netsToCells[i] = cell;
			return;
		}
	}
	if (end == netsToCells.size()) {
		netsToCells.push_back(cell);
		return;
	}
	int *p = nullptr;
	*p = 5;
	printf("not enough space\n");
}

void Schematic::finish() {
	nets.push_back(netsToCells.size());
	cells.push_back(cellsToNets.size());
}

bool Schematic::isCell() const {
	return cells.size() <= 1u;
}

Placement::Placement() {
	lst = nullptr;
}

Placement::~Placement() {
}

void Placement::configure(int platformId, int deviceId, bool debug) {
	configureSource(placer_cpp_string, platformId, deviceId, debug); 
}

void Placement::configurePath(string kernelPath, int platformId, int deviceId, bool debug) {
	string source;
	FILE *fptr = fopen(kernelPath.c_str(), "r");
	fseek(fptr, 0, SEEK_END);
	int size = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	source.resize(size, '\0');
	fread(source.data(), 1, size, fptr);
	fclose(fptr);

	configureSource(source, platformId, deviceId, debug);
}

void Placement::configureSource(string source, int platformId, int deviceId, bool debug) {
	string platformName = "No Platform";
	string deviceName = "No Device";
	size_t maxComputeUnits = 0;
	size_t maxWorkGroupSize = 0;
	cl_ulong globalMemSize = 0;

	vector<cl::Platform> platforms;
	vector<cl::Device> devices;
	cl::Platform::get(&platforms);
	if (platformId < (int)platforms.size()) {
		platformName = platforms[platformId].getInfo<CL_PLATFORM_NAME>();
		platforms[platformId].getDevices((cl_device_type)CL_DEVICE_TYPE_ALL, &devices);

		if (deviceId < (int)devices.size()) {
			deviceName = devices[deviceId].getInfo<CL_DEVICE_NAME>();
			devices[deviceId].getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &maxComputeUnits);
			devices[deviceId].getInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE, &maxWorkGroupSize);
			devices[deviceId].getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &globalMemSize);
			context = cl::Context({devices[deviceId]});
		}
	}
	
	if (debug) {
		cout << "Configured for " << platformName << ", " << deviceName << endl;
		cout << "CL_DEVICE_MAX_COMPUTE_UNITS: " << maxComputeUnits << endl;
		cout << "CL_DEVICE_MAX_WORK_GROUP_SIZE: " << maxWorkGroupSize << endl;
    cout << "GPU Global Memory: " << globalMemSize / (1024 * 1024) << " MB" << endl;
	}

	queue = cl::CommandQueue(context);
	cl::Program::Sources sources;
	sources.push_back(source.c_str());

	try {
		program = cl::Program(context, sources);
		program.build();
	} catch (cl::Error &err) {
		if (err.err() == CL_BUILD_PROGRAM_FAILURE) {
			// Get build log for debugging
			std::vector<cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
			for (auto &device : devices) {
				std::string buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
				std::cerr << "Build Log for Device: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
				std::cerr << buildLog << std::endl;
			}
		} else {
			std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		}
		exit(1);
	}	

	initPlacement = cl::Kernel(program, "initPlacement");
	stepPlacement = cl::Kernel(program, "stepPlacement");
	partitionCols = cl::Kernel(program, "partitionCols");
	partitionRows = cl::Kernel(program, "partitionRows");

	size_t kernelMaxWorkGroupSize = 0;
	size_t preferredWorkGroupMultiple = 0;
	if (deviceId < (int)devices.size()) {
		initPlacement.getWorkGroupInfo(devices[deviceId], CL_KERNEL_WORK_GROUP_SIZE, &kernelMaxWorkGroupSize);
		initPlacement.getWorkGroupInfo(devices[deviceId], CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, &preferredWorkGroupMultiple);
	}

	if (debug) {
		std::cout << "CL_KERNEL_WORK_GROUP_SIZE: " << kernelMaxWorkGroupSize << std::endl;
		std::cout << "CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: " << preferredWorkGroupMultiple << std::endl;
	}
}

void Placement::elaborateSchematicNets(const Netlist &lst, int curr, bool debug) {
	auto currSch = schem.begin()+curr;
	auto currCkt = lst.subckts.begin()+curr;

	// Start by placing the nets for this cell
	for (int i = 0; i < (int)currCkt->nets.size(); i++) {
		currSch->pushNet(currCkt->nets[i].name);

		int count = 0;
		for (auto j = currCkt->nets[i].portOf.begin(); j != currCkt->nets[i].portOf.end(); j++) {
			auto inst = currCkt->inst.begin()+*j;
			auto nextSch = schem.begin()+inst->subckt;
			auto nextCkt = lst.subckts.begin()+inst->subckt;

			for (int k = 0; k < (int)inst->ports.size(); k++) {
				if (inst->ports[k] == i) {
					int net = nextCkt->ports[k];
					count += nextSch->isCell() ? 1 : nextSch->nets[net+1]-nextSch->nets[net];
				}
			}
		}
		currSch->allocPorts(count);
	}
}

void Placement::elaborateSchematicInstance(const phy::Library &lib, const Netlist &lst, int curr, int idx) {
	auto currSch = schem.begin()+curr;
	auto currCkt = lst.subckts.begin()+curr;

	int next = currCkt->inst[idx].subckt;
	auto nextLay = lib.macros.begin()+next;
	auto nextSch = schem.begin()+next;
	auto nextCkt = lst.subckts.begin()+next;

	cl_uint h = 0;
	if (not currSch->cellBounds.empty()) {
		cl_uint2 bound = currSch->cellBounds.back();
		h = currSch->hilbert.back() + (bound.s[0]*bound.s[1])/2;
	}

	currSch->totalArea += nextSch->totalArea;
	if (nextSch->isCell()) {
		currSch->pushCell(next, nextLay->box.size(), h+nextSch->totalArea/2);
		currSch->pushPorts(currCkt->inst[idx].ports, currSch->cells.size()-1);
		currSch->cellsToNets.insert(currSch->cellsToNets.end(), currCkt->inst[idx].ports.begin(), currCkt->inst[idx].ports.end());
	} else {
		// Then place the nets of the instances
		ucs::mapping currMap;
		for (int j = 0; j < (int)currCkt->inst[idx].ports.size(); j++) {
			currMap.set(nextCkt->ports[j], currCkt->inst[idx].ports[j]);
		}

		for (int j = 0; j+1 < (int)nextSch->nets.size(); j++) {
			int net = currMap.map(j);
			if (net < 0) {
				net = currSch->pushNet("c"+idToString(idx)+"."+nextSch->netNames[j]);
				if (net < 0) {
					continue;
				}
				currMap.set(j, net);
			}

			for (size_t port = nextSch->nets[j]; port < nextSch->nets[j+1]; port++) {
				currSch->pushPorts(net, currSch->cells.size()+nextSch->netsToCells[port]);
			}
		}

		for (int j = 0; j+1 < (int)nextSch->cells.size(); j++) {
			currSch->pushCell(nextSch->subckts[j], nextSch->cellBounds[j], h+nextSch->hilbert[j]);
			for (size_t k = nextSch->cells[j]; k < nextSch->cells[j+1]; k++) {
				currSch->cellsToNets.push_back(currMap.map(nextSch->cellsToNets[k]));
			}
		}
	}
}

void Placement::elaborateSchematic(const phy::Library &lib, const Netlist &lst, int curr, bool debug) {
	auto currSch = schem.begin()+curr;
	auto currCkt = lst.subckts.begin()+curr;

	// Do simulated annealing reduce total wirelength along the hilbert curve
	if (currCkt->inst.empty()) {
		auto currLay = lib.macros.begin()+curr;
		currSch->totalArea = currLay->box.area();
	} else {
		elaborateSchematicNets(lst, curr, debug);
		vector<int> index = doHier(*currCkt);
		for (auto i = index.begin(); i != index.end(); i++) {
			elaborateSchematicInstance(lib, lst, curr, *i);
		}
	}
	currSch->finish();
}

void Placement::load(const phy::Library &lib, const Netlist &lst, int root, bool debug) {
	this->root = root;
	this->lst = &lst;
	schem.resize(lst.subckts.size());

	vector<int> stack(1, root);
	while (not stack.empty()) {
		int curr = stack.back();
		auto currCkt = lst.subckts.begin()+curr;
		
		bool done = true;
		for (auto i = currCkt->inst.begin(); i != currCkt->inst.end(); i++) {
			if (schem[i->subckt].cells.empty()) {
				done = false;
				stack.erase(remove(stack.begin(), stack.end(), i->subckt), stack.end());
				stack.push_back(i->subckt);
			}
		}

		if (done) {
			elaborateSchematic(lib, lst, curr, debug);
			stack.pop_back();
		}
	}

	cout << "cells " << schem[root].totalArea << ": " << schem[root].cells.size()-1 << endl;
	for (int i = 0; i+1 < (int)schem[root].cells.size(); i++) {
		size_t start = schem[root].cells[i];
		size_t end = schem[root].cells[i+1];
		int idx = lst.cellAt(root, i);
		string cellName = "nil";
		if (idx < (int)lst.subckts.size()) {
			cellName = lst.subckts[idx].name;
		}

		cout << cellName << "(" << i << ") " << schem[root].cellBounds[i].s[0] << "," << schem[root].cellBounds[i].s[1] << ": {";
		for (size_t j = start; j < end; j++) {
			cout << schem[root].cellsToNets[j] << " ";
		}
		cout << "}" << endl;
	}

	cout << "nets: " << schem[root].nets.size()-1 << endl;
	for (int i = 0; i+1 < (int)schem[root].nets.size(); i++) {
		size_t start = schem[root].nets[i];
		size_t end = schem[root].nets[i+1];
		cout << schem[root].netNames[i] << "(" << i << "): {";
		for (size_t j = start; j < end; j++) {
			cout << schem[root].netsToCells[j] << " ";
		}
		cout << "}" << endl;
	}
}

// From Hacker's Delight
cl_uint Placement::isqrt(cl_uint x) {
	cl_uint a, b, m; // Limits and midpoint.
	a = 1;
	b = (x >> 5) + 8;
	if (b > 65535) {
		b = 65535;
	}
	do {
		m = (a + b) >> 1;
		if (m*m > x) {
			b = m - 1;
		} else {
			a = m + 1;
		}
	} while (b >= a);
	return a - 1;
}

vector<cl_uint> Placement::hierComputeOffsets(const Subckt &ckt, const vector<int> &index) {
	// Determine midpoint locations of instances in Hilbert space using half
	// instance area.
	
	// position in ckt.inst -> hilbert position
	/*printf("offsets of cells {");
	for (int i = 0; i < (int)schem.size(); i++) {
		printf("%lu ", schem[i].totalArea);
	}
	printf("}\n");*/

	vector<cl_uint> offset(index.size(), 0);
	for (auto i = index.begin(); i != index.end(); i++) {
		offset[*i] = 0;
		if (i != index.begin()) {
			offset[*i] = offset[*std::prev(i)] + schem[ckt.inst[*std::prev(i)].subckt].totalArea/2;
		}
		offset[*i] += schem[ckt.inst[*i].subckt].totalArea/2;
	}
	return offset;
}

cl_uint Placement::hierComputeHPWL(const Subckt &ckt, const vector<cl_uint> &offset) {
	// Compute total half perimeter wire length. Estimate the expected perimeter
	// of an interval on the hilbert curve as `sqrt(length)*4` assuming that
	// allocated spaces on the hilbert curve tend to be rectangular and the
	// expected area of an interval as `length`.
	
	/*printf("hpwl of {");
	for (int i = 0; i < (int)offset.size(); i++) {
		printf("%u ", offset[i]);
	}
	printf("}");*/
	cl_uint hpwl = 0;
	for (int i = 0; i < (int)ckt.nets.size(); i++) {
		cl_uint lo = std::numeric_limits<cl_uint>::max();
		cl_uint hi = 0;
		for (auto j = ckt.nets[i].portOf.begin(); j != ckt.nets[i].portOf.end(); j++) {
			if (offset[*j] < lo) {
				lo = offset[*j];
			}
			if (offset[*j] > hi) {
				hi = offset[*j];
			}
		}
		if (hi <= lo) {
			continue;
		}

		hpwl += isqrt(hi-lo)*2;
	}
	//printf(" = %u\n", hpwl);
	return hpwl;
}

vector<int> Placement::doHier(const Subckt &ckt, int starts, float step, float rate) {
	// Order subckt instances to minimize the estimated HPWL of the layout using
	// a Hilbert space-filling curve. This is a really rough heuristic used to
	// quickly compute an initial placement. Since this is done hierarchically,
	// all cells will be roughly ordered to minimize the HPWL of their local
	// connections in the module hierarchy.

	// This ordering is done using a simple simulated annealing algorithm. See
	// hierComputeHPWL() to see how the total half perimeter wire length (HPWL)
	// of an orderng is estimated. See hierComputeOffsets() to see how we place
	// modules on the Hilbert curve by evenly distributing module area.

	std::default_random_engine rand(0/*std::random_device{}()*/);
	if (ckt.inst.empty()) {
		return vector<int>();
	}

	vector<int> best; // hilbert order -> position in ckt.inst
	for (int i = 0; i < (int)ckt.inst.size(); i++) {
		best.push_back(i);
	}
	vector<cl_uint> offset = hierComputeOffsets(ckt, best);
	cl_uint bestScore = hierComputeHPWL(ckt, offset);

	vector<vec2i> choices;
	for (int j = 0; j < (int)best.size(); j++) {
		for (int k = j+1; k < (int)best.size(); k++) {
			choices.push_back(vec2i(j, k));
		}
	}

	vector<int> curr = best;
	for (int i = 0; i < starts; i++) {
		shuffle(curr.begin(), curr.end(), rand);
		offset = hierComputeOffsets(ckt, curr);
		cl_uint score = 0;
		cl_uint newScore = hierComputeHPWL(ckt, offset);
		float currStep = step;

		do {
			score = newScore;
			//printf("score: %u\n", score);
			for (auto choice = choices.begin(); choice != choices.end(); choice++) {
				for (int j = (*choice)[0], k = (*choice)[1]; j < k; j++, k--) {
					swap(curr[j], curr[k]);
				}
				offset = hierComputeOffsets(ckt, curr);
				newScore = hierComputeHPWL(ckt, offset);
				if (newScore < score*currStep) {
					break;
				} else {
					for (int j = (*choice)[0], k = (*choice)[1]; j < k; j++, k--) {
						swap(curr[j], curr[k]);
					}
				}
			}

			shuffle(choices.begin(), choices.end(), rand);

			float prevStep = currStep;
			currStep -= (currStep - 1.0)*rate;
			if (currStep == prevStep) {
				currStep = 1.0;
			}
		} while ((float)score*currStep - (float)newScore > 0.01);

		if (score < bestScore) {
			bestScore = score;
			best = curr;
		}
	}

	printf("found HPWL of %u for {", bestScore);
	for (int i = 0; i < (int)best.size(); i++) {
		printf("%d ", best[i]);
	}
	printf("}\n");

	return best;
}

void Placement::doGlobal() {
	// Evenly space all cells based on area in the Hilbert space filling curve.
	// Cells have been elaborated in a hierarchical HPWL minimizing sorted order
	// as a fast initial guess at a placement. This placement is not optimal, so
	// we still need to run a detail placement algorithm.

	cl_uint num = (schem[root].cells.size()-1);
	if (num == 0) {
		return;
	}

	position.resize(num);

	size_t positionSize = num * sizeof(cl_uint2);
	size_t hilbertSize = num * sizeof(cl_uint);
	try {
		cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE, positionSize);
		cl::Buffer hilbertBuffer(context, CL_MEM_READ_WRITE, hilbertSize);	
		initPlacement.setArg(0, positionBuffer);
		initPlacement.setArg(1, hilbertBuffer);
		initPlacement.setArg(2, num);
		initPlacement.setArg(3, schem[root].totalArea);

		queue.enqueueWriteBuffer(hilbertBuffer, CL_TRUE, 0, hilbertSize, schem[root].hilbert.data());

		queue.enqueueNDRangeKernel(initPlacement, cl::NullRange, cl::NDRange(num), cl::NullRange);
		queue.finish();

		queue.enqueueReadBuffer(positionBuffer, CL_TRUE, 0, positionSize, position.data());
	} catch (cl::Error &err) {
		std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		exit(1);
	}
}

void Placement::doDetail() {
	// This is a GPU optimized variant of RePlAce, a force directed graph layout
	// with two types of forces:
	// 1. attractive forces between cells connected by a net
	// 2. repulsive forces between neighboring cells.
	//
	// Given N cells, RePlAce creates an NxN grid of bins, and computes a cell
	// area vs capacity density value for each bin. Then it takes the fast
	// fourier transform of that, followed by a low pass filter, then uses that
	// as the gradiant to push cells around as the repulsive force. As cells
	// stablize, they increase the frequency they pass.
	//
	// The approach we'll take does the same thing, but slightly differently. We
	// are given an initial cell placement on the Hilbert space filing curve.
	// 1. start with a quad tree with one node.
	// 2. for each node in the quadtree, compute the centroid, the cell area vs
	// capacity amplitude, and the standard deviation. As the number of points in
	// a distribution grows, it tends toward a normal distribution. This computes
	// that normal distribution.
	// 3. Apply the gradient on all cells from that normal distribution.
	// 4. When cells stabilize in a node, subdivide that node.
	// 5. There is a constant time algorithm to identify neighbors of a quad-tree
	// node. Use this to walk the quadtree and apply gradient forces until those
	// forces become negligible due to distance. Use a breadth first search.
	// 6. Stop subdividing when there are 7 to 13 cells in the node.
	//
	// If this method starts to lose acuity at smaller distributions, then we
	// need to switch to a more detailed method.
	// 1. The delauny triangulation is an optimal mesh that eliminates thin
	// triangules, further there is a unique delauny triangulation for any
	// distribution of vertices. This means that solving the delauny
	// triangulation locally will also solve it globally because local solutions
	// will be consistent with eachother.
	// 2. The expected maximum degree of a vertex in this mesh is
	// M = log(N)/log(log(N)). For 1T points, that's 12. for 300k points, thats 8.
	// 3. For each cell, search for 2M nearest neighbors using the quadtree.
	// 4. Filter out the nearest neighbors that violate the delauny constraint.
	// 5. The remaining nearest neighbors will correctly implement the delauny
	// triangulation. Even if there is a mistake, that doesn't matter.
	// 6. In the next iteration on the GPU, we now have the complete delauny triangulation.
	// 7. Walk this graph using Breadth first search, and apply electrostatic repulsion forces.
	// 8. do a natural interpolation of nearest neighbors to determine gradient.


	/*for (int i = 0; i < (int)position.size(); i++) {
		int idx = lst->cellAt(root, i);
		string cellName = "nil";
		if (idx < (int)lst->subckts.size()) {
			cellName = lst->subckts[idx].name;
		}
		cout << cellName << "(" << i << "): {" << position[i].s[0] << " " << position[i].s[1] << "}" << endl;
	}

	size_t dataSize = (schem[root].cells.size()-1) * sizeof(cl_float2);
	cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, position.data());
	cl::Buffer velocityBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, velocity.data());
	cl::Buffer forceBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, force.data());

	size_t netsToCellsSize = schem[root].netsToCells.size() * sizeof(size_t);
	size_t netsSize = schem[root].nets.size() * sizeof(size_t);
	size_t cellsToNetsSize = schem[root].cellsToNets.size() * sizeof(size_t);
	size_t cellsSize = schem[root].cells.size() * sizeof(size_t);

	cl::Buffer netsToCellsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, netsToCellsSize, schem[root].netsToCells.data());
	cl::Buffer netsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, netsSize, schem[root].nets.data());
	cl::Buffer cellsToNetsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, cellsToNetsSize, schem[root].cellsToNets.data());
	cl::Buffer cellsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, cellsSize, schem[root].cells.data());

	size_t nets = schem[root].nets.size()-1;
	size_t cells = schem[root].cells.size()-1;
	float k = 1.0f;
	float damping = 0.85f;
	float step = 0.1f;
	float end = 100.0f;

	// opencl kernel on cells to apply attractive forces from nets, update position, locate cells to specific bins, and zero forces
	//   a. For a given cell, I need to know all the nets connected to it
	//   b. Then I need to look up all the other cells on those nets
	//   c. Then I need to compute the bin x and y and use that to compute the bin index
	//   d. Then I need to add the cell area to that bin's density
	//   e. Use the bin's density to compute the repulsive forces
	
	kernel.setArg(0, netsToCellsBuffer);
	kernel.setArg(1, netsBuffer);
	kernel.setArg(2, cellsToNetsBuffer);
	kernel.setArg(3, cellsBuffer);
	kernel.setArg(4, nets);
	kernel.setArg(5, cells);
	kernel.setArg(6, positionBuffer);
	kernel.setArg(7, velocityBuffer);
	kernel.setArg(8, forceBuffer);
	kernel.setArg(9, k);
	kernel.setArg(10, damping);
	kernel.setArg(11, step);

	int i = 0;
	for (float t = 0.0f; t < end; t+=step) {
		printf("\rstep %d %f/%f       ", i, t, end);
		fflush(stdout);
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(nets), cl::NullRange);
		queue.finish();
		i++;
	}
	printf("Simulated %d steps        \n\n", i);

	queue.enqueueReadBuffer(positionBuffer, CL_TRUE, 0, dataSize, position.data());

	for (int i = 0; i < (int)position.size(); i++) {
		int idx = lst->cellAt(root, i);
		string cellName = "nil";
		if (idx < (int)lst->subckts.size()) {
			cellName = lst->subckts[idx].name;
		}
		cout << cellName << "(" << i << "): {" << position[i].s[0] << " " << position[i].s[1] << "}" << endl;
	}*/
}

void Placement::doLegal(phy::Library &lib) {
	// For legalization, we need to first divide the space up into columns, then
	// divide the space up into rows. Column clusters should seek to reduce the
	// standard deviation of the height of the cells within each row while
	// maximizing the number of cells in the column (to a point). Row clusters
	// should seek to reduce the standard deviation of the total width of the
	// column across rows while bucketing cells by cell height. This algorithm
	// also needs to be easily parallelizeable.

	// 1. break cells into columns
	//    a. parallel kernel to compute column based on x coordinate / column width
	//    b. for each column, compute average cell height `h`
	// 3. put cells into H/2h rows based on y coordinate / row height
	//    a. another parallel kernel given column assignments and average cell height per column
	// 4. for each row, sort based on cell height, find midpoint of cell width to determine weighted median of cell height `m`
	// 5. break each row into two with all cells shorter than `m` in one and all cells taller than `m` in the other. Compute max height of each row.
	// 6. lock y-coordinates into rows based on each row height
	// 7. create a database of cell offsets for every pair of cells in the design
	// 8. use this to pack x-coordinates in each row. Rows to the right of midpoint should be packed left to right and visa versa for left of midpoint.
	// 9. record row and column geometry.

	cl_uint num = (schem[root].cells.size()-1);
	if (num == 0) {
		return;
	}

	cl_uint side = isqrt(schem[root].totalArea);
	side += side >> 3;

	// TODO(edward.bingham) Get tap to diff enclosure rule Column is then N times
	// tap to diff enclosure rule with a well tap on either side of the column.
	
	cl_uint coeff = 8;
	cl_uint diffTap = 848; //tech.getEnclosing()
	cl_uint buffer = diffTap >> 3;
	cl_uint colWidth = coeff * (diffTap - buffer);

	cl_uint numCol = (side / colWidth) + 1;
	colWidth = side / numCol;

	printf("num=%u numCol=%u side=%u colWidth=%u diffTap=%u buffer=%u\n", num, numCol, side, colWidth, diffTap, buffer);

	// Start by assigning columns based only on the x-coord
	vector<cl_uint> rows(numCol, 0);
	grid.resize(num);
	for (int i = 0; i < (int)position.size(); i++) {
		grid[i].s[0] = position[i].s[0] / colWidth;
		rows[grid[i].s[0]] += schem[root].cellBounds[i].s[0];
	}

	// Then compute the number of rows needed in each column
	// x-coord, x-bound, y-bound, cell index
	vector<vector<vector<cl_uint4> > > assign(numCol);
	for (int i = 0; i < (int)rows.size(); i++) {
		cl_uint count = (rows[i]+colWidth-1)/colWidth;
		count += count%2;
		assign[i].resize(count);
	}

	// Then assign each cell to every other row based loosely on the
	// y-coord
	for (int i = 0; i < (int)position.size(); i++) {
		cl_uint x = position[i].s[0];
		cl_uint y = position[i].s[1];
		cl_uint2 b = schem[root].cellBounds[i];
		cl_uint c = grid[i].s[0];
		grid[i].s[1] = 2 * (y * assign[c].size() / (side * 2));
		assign[c][grid[i].s[1]].push_back({x, b.s[0], b.s[1], (cl_uint)i});
	}

	vector<vector<cl_uint> > rowHeight(numCol);
	for (int c = 0; c < (int)assign.size(); c++) {
		rowHeight[c].resize(assign[c].size(), 0);
		for (int r = 0; r < (int)assign[c].size(); r+=2) {
			sort(assign[c][r].begin(), assign[c][r].end(), [](const cl_uint4 &a, const cl_uint4 &b) { 
				return a.s[2] < b.s[2]; 
			});
			cl_uint prev = 0;
			for (int i = 0; i < (int)assign[c][r].size(); i++) {
				assign[c][r][i].s[1] += prev;
				prev = assign[c][r][i].s[1];
			}
			auto pos = lower_bound(assign[c][r].begin(), assign[c][r].end(), prev/2, [](const cl_uint4 &a, const cl_uint &b) { 
				return a.s[1] < b; 
			});
			if (pos != assign[c][r].begin()) {
				rowHeight[c][r] = std::prev(pos)->s[2];
			}
			if (not assign[c][r].empty()) {
				rowHeight[c][r+1] = assign[c][r].back().s[2];
			}
			
			assign[c][r+1].insert(assign[c][r+1].end(), pos, assign[c][r].end());
			assign[c][r].erase(pos, assign[c][r].end());

			for (int i = 0; i < (int)assign[c][r+1].size(); i++) {
				++grid[assign[c][r+1][i].s[3]].s[1];
			}
		}
	}
	// assign is now x-coord, XXXXXXX, y-bound, idx

	/*size_t positionSize = num * sizeof(cl_uint2);
	size_t boundSize = num * sizeof(cl_uint2);
	size_t colSize = num * sizeof(cl_uint);
	size_t colHeightSize = numCol * sizeof(cl_uint);
	size_t colTotalWidthSize = numCol * sizeof(cl_uint);
	size_t colCountSize = numCol * sizeof(cl_uint);

	vector<cl_uint> colHeight(numCol, 0);
	vector<cl_uint> colTotalWidth(numCol, 0);
	vector<cl_uint> colCount(numCol, 0);
	col.resize(num, 0);

	try {
		cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE, positionSize);
		cl::Buffer boundBuffer(context, CL_MEM_READ_WRITE, boundSize);	
		cl::Buffer colBuffer(context, CL_MEM_READ_WRITE, colSize);
		cl::Buffer colHeightBuffer(context, CL_MEM_READ_WRITE, colHeightSize);
		cl::Buffer colTotalWidthBuffer(context, CL_MEM_READ_WRITE, colTotalWidthSize);
		cl::Buffer colCountBuffer(context, CL_MEM_READ_WRITE, colCountSize);
		//cl::Buffer rowBuffer(context, CL_MEM_READ_WRITE, rowSize);

		partitionCols.setArg(0, positionBuffer);
		partitionCols.setArg(1, boundBuffer);
		partitionCols.setArg(2, num);
		partitionCols.setArg(3, colBuffer);
		partitionCols.setArg(4, colHeightBuffer);
		partitionCols.setArg(5, colTotalWidthBuffer);
		partitionCols.setArg(6, colCountBuffer);
		partitionCols.setArg(7, colWidth);

		queue.enqueueWriteBuffer(positionBuffer, CL_TRUE, 0, positionSize, position.data());
		queue.enqueueWriteBuffer(boundBuffer, CL_TRUE, 0, boundSize, schem[root].cellBounds.data());
		queue.enqueueWriteBuffer(colHeightBuffer, CL_TRUE, 0, colHeightSize, colHeight.data());
		queue.enqueueWriteBuffer(colTotalWidthBuffer, CL_TRUE, 0, colTotalWidthSize, colTotalWidth.data());
		queue.enqueueWriteBuffer(colCountBuffer, CL_TRUE, 0, colCountSize, colCount.data());

		queue.enqueueNDRangeKernel(partitionCols, cl::NullRange, cl::NDRange(num), cl::NullRange);
		queue.finish();

		queue.enqueueReadBuffer(colBuffer, CL_TRUE, 0, colSize, col.data());
		queue.enqueueReadBuffer(colHeightBuffer, CL_TRUE, 0, colHeightSize, colHeight.data());
		queue.enqueueReadBuffer(colTotalWidthBuffer, CL_TRUE, 0, colTotalWidthSize, colTotalWidth.data());
		queue.enqueueReadBuffer(colCountBuffer, CL_TRUE, 0, colCountSize, colCount.data());
	} catch (cl::Error &err) {
		std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		exit(1);
	}

	row.resize(num, 0);

	size_t rowSize = num * sizeof(cl_uint);

	try {
		cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE, positionSize);
		cl::Buffer colBuffer(context, CL_MEM_READ_WRITE, colSize);
		cl::Buffer colHeightBuffer(context, CL_MEM_READ_WRITE, colHeightSize);
		cl::Buffer colCountBuffer(context, CL_MEM_READ_WRITE, colCountSize);
		cl::Buffer rowBuffer(context, CL_MEM_READ_WRITE, rowSize);

		partitionRows.setArg(0, positionBuffer);
		partitionRows.setArg(1, num);
		partitionRows.setArg(2, colBuffer);
		partitionRows.setArg(3, colHeightBuffer);
		partitionRows.setArg(4, colCountBuffer);
		partitionRows.setArg(5, rowBuffer);

		queue.enqueueWriteBuffer(positionBuffer, CL_TRUE, 0, positionSize, position.data());
		queue.enqueueWriteBuffer(colBuffer, CL_TRUE, 0, colSize, col.data());
		queue.enqueueWriteBuffer(colHeightBuffer, CL_TRUE, 0, colHeightSize, colHeight.data());
		queue.enqueueWriteBuffer(colCountBuffer, CL_TRUE, 0, colCountSize, colCount.data());

		queue.enqueueNDRangeKernel(partitionRows, cl::NullRange, cl::NDRange(num), cl::NullRange);
		queue.finish();

		queue.enqueueReadBuffer(rowBuffer, CL_TRUE, 0, rowSize, row.data());
	} catch (cl::Error &err) {
		std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		exit(1);
	}*/

	/*for (int i = 0; i < (int)num; i++) {
		printf("position %d:(%u %u) col=%u row=%u\n", i, position[i].s[0], position[i].s[1], col[i], row[i]);
	}

	for (int i = 0; i < (int)numCol; i++) {
		printf("col %d height=%d count=%d\n", i, colHeight[i], colCount[i]);
	}

	vector<vector<vector<cl_uint3> > > rowWidth(numCol);
	vector<vector<cl_uint> > rowMedian(numCol);

	for (int i = 0; i < (int)row.size(); i++) {
		if (row[i] >= rowWidth[col[i]].size()) {
			rowWidth[col[i]].resize(row[i]+1);
		}
		rowWidth[col[i]][row[i]].push_back(cl_uint3{schem[root].cellBounds[i].s[0], schem[root].cellBounds[i].s[1], (cl_uint)i});
	}

	vector<vector<vector<cl_uint2> > > rowAssign(numCol);
	vector<vector<cl_uint> > rowHeight(numCol);
	for (int c = 0; c < (int)rowWidth.size(); c++) {
		rowAssign[c].resize(rowWidth[c].size()*2);
		rowHeight[c].resize(rowWidth[c].size()*2, 0);
		for (int i = 0; i < (int)rowWidth[c].size(); i++) {
			sort(rowWidth[c][i].begin(), rowWidth[c][i].end(), [](const cl_uint3 &a, const cl_uint3 &b) { 
				return a.s[1] < b.s[1]; 
			});
			for (int j = 1; j < (int)rowWidth[c][i].size(); j++) {
				rowWidth[c][i][j].s[0] += rowWidth[c][i][j-1].s[0];
			}

			int s = 0;
			for (int j = 0; j < (int)rowWidth[c][i].size(); j++) {
				if (rowWidth[c][i][j].s[0] > rowWidth[c][i].back().s[0]/2) {
					s = 1;
				}

				cl_uint index = rowWidth[c][i][j].s[2];
				row[index] = i*2+s;
				rowAssign[c][i*2+s].push_back(cl_uint2{position[index].s[0], index});
				cl_uint2 bound = schem[root].cellBounds[index];
				if (bound.s[1] > rowHeight[c][i*2+s]) {
					rowHeight[c][i*2+s] = bound.s[1];
				}
			}
		}
	}*/

	map<pair<int, int>, int> offset;
	int colStart = 0;
	for (int c = 0; c < (int)assign.size(); c++) {
		if (rowHeight[c].empty()) {
			continue;
		}

		int rowStart = rowHeight[c][0]/2;
		int rowWidth = 0;
		for (int i = 0; i < (int)assign[c].size(); i++) {
			sort(assign[c][i].begin(), assign[c][i].end(), [](const cl_uint4 &a, const cl_uint4 &b) {
				return a.s[0] < b.s[0];
			});

			int prevPos = colStart;
			int prevSubckt = -1;
			ucs::mapping prevMap;
			for (int j = 0; j < (int)assign[c][i].size(); j++) {
				cl_uint index = assign[c][i][j].s[3];
				int currSubckt = schem[root].subckts[index];
				cl_uint2 bound = schem[root].cellBounds[index];
				ucs::mapping currMap;
				for (int k = 0; k < (int)lst->subckts[currSubckt].ports.size(); k++) {
					currMap.set(lst->subckts[currSubckt].ports[k], schem[root].cellsToNets[schem[root].cells[index]+k]);
				}

				int currPos = prevPos;
				if (j == 0) {
					currPos += bound.s[0]/2;
				}
				if (prevSubckt >= 0) {
					auto off = offset.find({prevSubckt, currSubckt});
					if (off != offset.end()) {
						currPos += off->second;
					} else {
						int value = 0;
						minOffset(&value, 0, lib.macros[prevSubckt], 0, lib.macros[currSubckt], 0, Layout::DEFAULT, Layout::DEFAULT, true, prevMap, currMap);
						offset.insert({{prevSubckt, currSubckt}, value});
						printf("from %s({%d %d} {%d %d}) to %s({%d %d} {%d %d}): %d\n", lib.macros[prevSubckt].name.c_str(), lib.macros[prevSubckt].box.ll[0], lib.macros[prevSubckt].box.ll[1], lib.macros[prevSubckt].box.ur[0], lib.macros[prevSubckt].box.ur[1], lib.macros[currSubckt].name.c_str(), lib.macros[currSubckt].box.ll[0], lib.macros[currSubckt].box.ll[1], lib.macros[currSubckt].box.ur[0], lib.macros[currSubckt].box.ur[1], value);
						currPos += value;
					}
				}

				position[index].s[0] = currPos; 
				position[index].s[1] = rowStart;

				prevSubckt = currSubckt;
				prevMap = currMap;
				prevPos = currPos;
				if (currPos+(int)bound.s[0]/2 > rowWidth) {
					rowWidth = currPos + bound.s[0]/2;
				}
			}
			rowStart += rowHeight[c][i]/2;
			if (i+1 < (int)assign[c].size()) {
				rowStart += rowHeight[c][i+1]/2;
			}
		}
		colStart += colWidth;
		rowStart = 0;
	}
}

void Placement::save(phy::Library &lib, const sch::Netlist &lst) {
	for (int i = 0; i < (int)position.size(); i++) {
		int idx = schem[root].subckts[i];
		string cellName = "nil";
		if (idx < (int)lst.subckts.size()) {
			cellName = lst.subckts[idx].name;
		}
		vec2i pos((int)position[i].s[0], (int)position[i].s[1]);
		vec2i dir(1, 1-2*(grid[i].s[1]%2));

		cout << cellName << "(" << i << "): pos={" << pos[0] << " " << pos[1] << "} dir={" << dir[0] << " " << dir[1] << "} " << schem[root].hilbert[i] << endl;
		lib.macros[root].inst.push_back(phy::Instance(idx, pos, dir));
	}

	// 1. draw power grid
	// 2. route to power
	// 3. draw well taps
	// 4. draw filler
}

}
