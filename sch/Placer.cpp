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
	//netNames.push_back(name);
	return result;
}

void Schematic::allocPorts(cl_uint count) {
	cl_uint blank = std::numeric_limits<cl_uint>::max();
	netsToCells.resize(netsToCells.size()+count, blank);
}

void Schematic::pushCell(int subckt, vec2i bound, cl_ulong pos) {
	cells.push_back(cellsToNets.size());
	subckts.push_back(subckt);
	cellBounds.push_back({(cl_uint)bound[0], (cl_uint)bound[1]});
	hilbert.push_back(pos);
}

void Schematic::pushCell(int subckt, cl_uint2 bound, cl_ulong pos) {
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
	printf("not enough space\n");
}

void Schematic::finish() {
	nets.push_back(netsToCells.size());
	cells.push_back(cellsToNets.size());
}

bool Schematic::isCell() const {
	return cells.size() <= 1u;
}

size_t Schematic::numCells() const {
	return cells.size()-1;
}

size_t Schematic::numNets() const {
	return nets.size()-1;
}

void Schematic::print(const Netlist &lst) const {
	cout << "cells " << totalArea << ": " << cells.size()-1 << endl;
	for (int i = 0; i+1 < (int)cells.size(); i++) {
		size_t start = cells[i];
		size_t end = cells[i+1];
		string cellName = lst.subckts[subckts[i]].name;
		cout << cellName << "(" << i << ") " << cellBounds[i].s[0] << "," << cellBounds[i].s[1] << ": {";
		for (size_t j = start; j < end; j++) {
			cout << cellsToNets[j] << " ";
		}
		cout << "}" << endl;
	}

	cout << "nets: " << nets.size()-1 << endl;
	for (int i = 0; i+1 < (int)nets.size(); i++) {
		size_t start = nets[i];
		size_t end = nets[i+1];
		//cout << netNames[i] << "(" << i << "): {";
		cout << i << ": {";
		for (size_t j = start; j < end; j++) {
			cout << netsToCells[j] << " ";
		}
		cout << "}" << endl;
	}
}

Placer::Placer() {
	lst = nullptr;
	lib = nullptr;
}

Placer::Placer(phy::Library &lib, const Netlist &lst, int platformId, int deviceId, bool debug) {
	configure(platformId, deviceId, debug);
	load(lib, lst);
}

Placer::~Placer() {
}

void Placer::configure(int platformId, int deviceId, bool debug) {
	configureSource(placer_cpp_string, platformId, deviceId, debug); 
}

void Placer::configurePath(string kernelPath, int platformId, int deviceId, bool debug) {
	string source;
	FILE *fptr = fopen(kernelPath.c_str(), "r");
	fseek(fptr, 0, SEEK_END);
	int size = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	source.resize(size, '\0');
	int total = 0;
	int count = 0;
	do {
		count = fread(source.data()+total, 1, size-total, fptr);
		total += count;
	} while (total < size and count > 0);
	fclose(fptr);

	configureSource(source, platformId, deviceId, debug);
}

void Placer::configureSource(string source, int platformId, int deviceId, bool debug) {
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

	globalStep = cl::Kernel(program, "globalStep");
	detailStep0 = cl::Kernel(program, "detailStep0");
	detailStep1 = cl::Kernel(program, "detailStep1");
	partitionCols = cl::Kernel(program, "partitionCols");
	partitionRows = cl::Kernel(program, "partitionRows");

	size_t kernelMaxWorkGroupSize = 0;
	size_t preferredWorkGroupMultiple = 0;
	if (deviceId < (int)devices.size()) {
		globalStep.getWorkGroupInfo(devices[deviceId], CL_KERNEL_WORK_GROUP_SIZE, &kernelMaxWorkGroupSize);
		globalStep.getWorkGroupInfo(devices[deviceId], CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, &preferredWorkGroupMultiple);
	}

	if (debug) {
		std::cout << "CL_KERNEL_WORK_GROUP_SIZE: " << kernelMaxWorkGroupSize << std::endl;
		std::cout << "CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: " << preferredWorkGroupMultiple << std::endl;
	}
}

void Placer::load(phy::Library &lib, const Netlist &lst) {
	this->lst = &lst;
	this->lib = &lib;
	schem.resize(lst.subckts.size());
}

void Placer::elaborateSchematicNets(int curr, bool debug) {
	auto currSch = schem.begin()+curr;
	auto currCkt = lst->subckts.begin()+curr;

	// Start by placing the nets for this cell
	for (int i = 0; i < (int)currCkt->nets.size(); i++) {
		currSch->pushNet(currCkt->nets[i].name);

		int count = 0;
		for (auto j = currCkt->nets[i].portOf.begin(); j != currCkt->nets[i].portOf.end(); j++) {
			auto inst = currCkt->inst.begin()+*j;
			auto nextSch = schem.begin()+inst->subckt;
			auto nextCkt = lst->subckts.begin()+inst->subckt;

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

void Placer::elaborateSchematicInstance(int curr, int sub, bool debug) {
	auto currSch = schem.begin()+curr;
	auto currCkt = lst->subckts.begin()+curr;

	int next = currCkt->inst[sub].subckt;
	auto nextLay = lib->macros.begin()+next;
	auto nextSch = schem.begin()+next;
	auto nextCkt = lst->subckts.begin()+next;

	cl_ulong h = 0;
	if (not currSch->cellBounds.empty()) {
		cl_uint2 bound = currSch->cellBounds.back();
		h = currSch->hilbert.back() + (cl_ulong)(bound.s[0]*bound.s[1])/2;
	}

	currSch->totalArea += nextSch->totalArea;
	if (nextSch->isCell()) {
		currSch->pushCell(next, nextLay->box.size(), h+nextSch->totalArea/2);
		printf("hilbert %llu/%llu\n", currSch->hilbert.back(), currSch->totalArea);
		currSch->pushPorts(currCkt->inst[sub].ports, currSch->cells.size()-1);
		currSch->cellsToNets.insert(currSch->cellsToNets.end(), currCkt->inst[sub].ports.begin(), currCkt->inst[sub].ports.end());
	} else {
		// Then place the nets of the instances
		mapping currMap;
		for (int j = 0; j < (int)currCkt->inst[sub].ports.size(); j++) {
			currMap.set(nextCkt->ports[j], currCkt->inst[sub].ports[j]);
		}

		for (int j = 0; j+1 < (int)nextSch->nets.size(); j++) {
			int net = currMap.map(j);
			if (net < 0) {
				net = currSch->pushNet("");//"c"+idToString(sub)+"."+nextSch->netNames[j]);
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
			printf("hilbert %llu/%llu\n", currSch->hilbert.back(), currSch->totalArea);
			for (size_t k = nextSch->cells[j]; k < nextSch->cells[j+1]; k++) {
				currSch->cellsToNets.push_back(currMap.map(nextSch->cellsToNets[k]));
			}
		}
	}
}

void Placer::elaborateSchematic(int curr, bool debug) {
	auto currSch = schem.begin()+curr;
	auto currCkt = lst->subckts.begin()+curr;

	// Do simulated annealing reduce total wirelength along the hilbert curve

	// TODO(edward.bingham) think about threading this using a worker pool if
	// it's too slow.

	printf("Elaborating %s\n", lst->subckts[curr].name.c_str());
	lst->subckts[curr].print();

	if (currCkt->inst.empty()) {
		auto currLay = lib->macros.begin()+curr;
		currSch->totalArea = currLay->box.area();
	} else {
		elaborateSchematicNets(curr, debug);
		vector<int> index = computeOrder(curr);
		for (auto i = index.begin(); i != index.end(); i++) {
			printf("next instance %s %llu\n", currCkt->inst[*i].name.c_str(), schem[currCkt->inst[*i].subckt].totalArea);
			elaborateSchematicInstance(curr, *i, debug);
		}
	}
	currSch->finish();

	//currSch->print(*lst);
	printf("done\n\n");
}

void Placer::elaborate(int subckt, bool debug) {
	if (lst == nullptr or lib == nullptr) {
		printf("error: the netlist and layout library have not been loaded\n");
		return;
	}

	if (not schem[subckt].cells.empty()) {
		return;
	}

	vector<int> stack(1, subckt);
	while (not stack.empty()) {
		int curr = stack.back();
		auto currCkt = lst->subckts.begin()+curr;
		
		bool done = true;
		for (auto i = currCkt->inst.begin(); i != currCkt->inst.end(); i++) {
			if (schem[i->subckt].cells.empty()) {
				done = false;
				stack.erase(remove(stack.begin(), stack.end(), i->subckt), stack.end());
				stack.push_back(i->subckt);
			}
		}

		if (done) {
			elaborateSchematic(curr, debug);
			stack.pop_back();
		}
	}
}

// From Hacker's Delight
cl_uint Placer::isqrt(cl_ulong x) {
	cl_ulong a, b, m; // Limits and midpoint.
	a = 1;
	b = (x >> 5) + 8;
	if (b > std::numeric_limits<cl_uint>::max()) {
		b = std::numeric_limits<cl_uint>::max();
	}
	do {
		m = (a + b) >> 1;
		if (m*m > x) {
			b = m - 1;
		} else {
			a = m + 1;
		}
	} while (b >= a);
	return (cl_uint)(a - 1);
}

vector<cl_uint> Placer::computeOffsets(int curr, const vector<int> &index) {
	// Determine midpoint locations of instances in Hilbert space using half
	// instance area.
	auto currCkt = lst->subckts.begin()+curr;

	
	// position in ckt.inst -> hilbert position
	/*printf("offsets of cells {");
	for (int i = 0; i < (int)schem.size(); i++) {
		printf("%llu ", schem[i].totalArea);
	}
	printf("}\n");*/

	vector<cl_uint> offset(index.size(), 0);
	for (auto i = index.begin(); i != index.end(); i++) {
		offset[*i] = 0;
		if (i != index.begin()) {
			offset[*i] = offset[*std::prev(i)] + schem[currCkt->inst[*std::prev(i)].subckt].totalArea/2;
		}
		offset[*i] += schem[currCkt->inst[*i].subckt].totalArea/2;
	}
	return offset;
}

cl_uint Placer::computeHPWL(int curr, const vector<cl_uint> &offset) {
	// Compute total half perimeter wire length. Estimate the expected perimeter
	// of an interval on the hilbert curve as `sqrt(length)*4` assuming that
	// allocated spaces on the hilbert curve tend to be rectangular and the
	// expected area of an interval as `length`.
	auto currCkt = lst->subckts.begin()+curr;
	
	/*printf("hpwl of {");
	for (int i = 0; i < (int)offset.size(); i++) {
		printf("%u ", offset[i]);
	}
	printf("}");*/
	cl_uint hpwl = 0;
	for (int i = 0; i < (int)currCkt->nets.size(); i++) {
		cl_uint lo = std::numeric_limits<cl_uint>::max();
		cl_uint hi = 0;
		for (auto j = currCkt->nets[i].portOf.begin(); j != currCkt->nets[i].portOf.end(); j++) {
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

vector<int> Placer::computeOrder(int subckt, int starts, float step, float rate) {
	// Order subckt instances to minimize the estimated HPWL of the layout using
	// a Hilbert space-filling curve. This is a really rough heuristic used to
	// quickly compute an initial placement. Since this is done hierarchically,
	// all cells will be roughly ordered to minimize the HPWL of their local
	// connections in the module hierarchy.

	// This ordering is done using a simple simulated annealing algorithm. See
	// computeHPWL() to see how the total half perimeter wire length (HPWL)
	// of an orderng is estimated. See computeOffsets() to see how we place
	// modules on the Hilbert curve by evenly distributing module area.
	auto currCkt = lst->subckts.begin()+subckt;

	std::default_random_engine rand(0/*std::random_device{}()*/);
	if (currCkt->inst.empty()) {
		return vector<int>();
	}

	vector<int> best; // hilbert order -> position in currCkt->inst
	for (int i = 0; i < (int)currCkt->inst.size(); i++) {
		best.push_back(i);
	}
	vector<cl_uint> offset = computeOffsets(subckt, best);
	cl_uint bestScore = computeHPWL(subckt, offset);

	vector<vec2i> choices;
	for (int j = 0; j < (int)best.size(); j++) {
		for (int k = j+1; k < (int)best.size(); k++) {
			choices.push_back(vec2i(j, k));
		}
	}

	vector<int> curr = best;
	for (int i = 0; i < starts; i++) {
		shuffle(curr.begin(), curr.end(), rand);
		offset = computeOffsets(subckt, curr);
		cl_uint score = 0;
		cl_uint newScore = computeHPWL(subckt, offset);
		float currStep = step;

		do {
			score = newScore;
			//printf("score: %u\n", score);
			for (auto choice = choices.begin(); choice != choices.end(); choice++) {
				for (int j = (*choice)[0], k = (*choice)[1]; j < k; j++, k--) {
					swap(curr[j], curr[k]);
				}
				offset = computeOffsets(subckt, curr);
				newScore = computeHPWL(subckt, offset);
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

	/*printf("found HPWL of %u for {", bestScore);
	for (int i = 0; i < (int)best.size(); i++) {
		printf("%d ", best[i]);
	}
	printf("}\n");*/

	return best;
}

void Placer::place(int subckt) {
	Placement prob(*this, subckt);
	prob.solve();
	prob.save();
}

Placement::Placement() {
	placer = nullptr;
	root = -1;
	schem = nullptr;
}

Placement::Placement(Placer &placer, int root) {
	init(placer, root);
}

Placement::~Placement() {
}

void Placement::init(Placer &placer, int root) {
	this->placer = &placer;
	this->root = root;
	this->schem = &placer.schem[root];
	placer.elaborate(root);
	position.resize(schem->numCells());
	grid.resize(schem->numCells());
	positionBuffer = cl::Buffer(placer.context, CL_MEM_READ_WRITE, bufferSize(position));
	gridBuffer = cl::Buffer(placer.context, CL_MEM_READ_WRITE, bufferSize(grid));
}

// Evenly space all cells based on area in the Hilbert space filling curve.
// Cells have been elaborated in a hierarchical HPWL minimizing sorted order as
// a fast initial guess at a placement. This placement is not optimal, so we
// still need to run a detail placement algorithm.
void Placement::doGlobal() {
	if (placer == nullptr or root < 0 or schem == nullptr) {
		printf("error: placement has not been initialized.\n");
		return;
	}

	if (schem->numCells() == 0) {
		printf("error: no cells to place.\n");
		return;
	}

	cl_uint side = placer->isqrt(schem->totalArea);
	side += side >> 3;
	if (side == 0) {
		side = 1;
	}
	cl_uint scale = std::numeric_limits<cl_uint>::max() / side;

	try {
		cl::Buffer hilbertBuffer(placer->context, CL_MEM_READ_WRITE, bufferSize(schem->hilbert));	
		placer->globalStep.setArg(0, positionBuffer);
		placer->globalStep.setArg(1, hilbertBuffer);
		placer->globalStep.setArg(2, schem->numCells());
		placer->globalStep.setArg(3, schem->totalArea);
		placer->globalStep.setArg(4, scale);

		placer->queue.enqueueWriteBuffer(hilbertBuffer, CL_TRUE, 0, bufferSize(schem->hilbert), schem->hilbert.data());

		placer->queue.enqueueNDRangeKernel(placer->globalStep, cl::NullRange, cl::NDRange(schem->numCells()), cl::NullRange);
		placer->queue.finish();

		placer->queue.enqueueReadBuffer(positionBuffer, CL_TRUE, 0, bufferSize(position), position.data());
	} catch (cl::Error &err) {
		std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		exit(1);
	}
}

void Placement::doDetail() {
	if (placer == nullptr or root < 0 or schem == nullptr) {
		printf("error: Placement has not been initialized.\n");
		return;
	}

	if (schem->numCells() == 0) {
		printf("error: no cells to place.\n");
		return;
	}

	/*for (int i = 0; i < (int)position.size(); i++) {
		int idx = lst->cellAt(root, i);
		string cellName = "nil";
		if (idx < (int)lst->subckts.size()) {
			cellName = lst->subckts[idx].name;
		}
		cout << cellName << "(" << i << "): {" << position[i].s[0] << " " << position[i].s[1] << "}" << endl;
	}

	size_t dataSize = (schem->cells.size()-1) * sizeof(cl_float2);
	cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, position.data());
	cl::Buffer velocityBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, velocity.data());
	cl::Buffer forceBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, force.data());

	size_t netsToCellsSize = schem->netsToCells.size() * sizeof(size_t);
	size_t netsSize = schem->nets.size() * sizeof(size_t);
	size_t cellsToNetsSize = schem->cellsToNets.size() * sizeof(size_t);
	size_t cellsSize = schem->cells.size() * sizeof(size_t);

	cl::Buffer netsToCellsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, netsToCellsSize, schem->netsToCells.data());
	cl::Buffer netsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, netsSize, schem->nets.data());
	cl::Buffer cellsToNetsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, cellsToNetsSize, schem->cellsToNets.data());
	cl::Buffer cellsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, cellsSize, schem->cells.data());

	size_t nets = schem->nets.size()-1;
	size_t cells = schem->cells.size()-1;
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

void Placement::doLegal() {
	if (placer == nullptr or root < 0 or schem == nullptr) {
		printf("error: Placement has not been initialized.\n");
		return;
	}

	if (schem->numCells() == 0) {
		printf("error: no cells to place.\n");
		return;
	}

	cl_uint side = placer->isqrt(schem->totalArea);
	side += side >> 3;
	if (side == 0) {
		side = 1;
	}

	// TODO(edward.bingham) Get tap to diff enclosure rule Column is then N times
	// tap to diff enclosure rule with a well tap on either side of the column.
	
	cl_uint coeff = 8;
	cl_uint diffTap = 848; //tech.getEnclosing()
	cl_uint buffer = diffTap >> 3;
	cl_uint colWidth = coeff * (diffTap - buffer);

	cl_uint numCol = (side / colWidth) + 1;
	colWidth = (side+numCol-1) / numCol;

	//printf("num=%u numCol=%u side=%u colWidth=%u diffTap=%u buffer=%u\n", num, numCol, side, colWidth, diffTap, buffer);

	// Start by assigning columns based only on the x-coord
	vector<cl_uint> rows(numCol, 0);
	for (int i = 0; i < (int)position.size(); i++) {
		grid[i].s[0] = position[i].s[0] / colWidth;
		rows[grid[i].s[0]] += schem->cellBounds[i].s[0];
	}

	// Then compute the number of rows needed in each column by dividing the
	// total aggregated width of all cells in that column by the width of the
	// column.
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
		cl_uint2 b = schem->cellBounds[i];
		cl_uint c = grid[i].s[0];
		grid[i].s[1] = 2 * (y * assign[c].size() / (side * 2));
		assign[c][grid[i].s[1]].push_back({x, b.s[0], b.s[1], (cl_uint)i});
	}

	// separate every other row into two using the width-weighted median height of the row
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
			mapping prevChildToParent;
			for (int j = 0; j < (int)assign[c][i].size(); j++) {
				cl_uint index = assign[c][i][j].s[3];
				int currSubckt = schem->subckts[index];
				cl_uint2 bound = schem->cellBounds[index];
				mapping currChildToParent;
				for (int k = 0; k < (int)placer->lst->subckts[currSubckt].ports.size(); k++) {
					currChildToParent.set(placer->lst->toLayout[currSubckt].map(placer->lst->subckts[currSubckt].ports[k]), schem->cellsToNets[schem->cells[index]+k]);
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
						minOffset(&value, 0, placer->lib->macros[prevSubckt], 0, placer->lib->macros[currSubckt], 0, Layout::MERGENET, Layout::DEFAULT, true, prevChildToParent.nets, currChildToParent.nets);
						offset.insert({{prevSubckt, currSubckt}, value});
						currPos += value;
					}
				}

				position[index].s[0] = currPos; 
				position[index].s[1] = rowStart;

				prevSubckt = currSubckt;
				prevChildToParent = currChildToParent;
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

void Placement::solve() {
	if (placer == nullptr or root < 0 or schem == nullptr) {
		printf("error: Placement has not been initialized.\n");
		return;
	}

	if (schem->numCells() == 0) {
		printf("error: no cells to place.\n");
		return;
	}

	doGlobal();
	//doDetail();
	doLegal();
}

void Placement::save(phy::Layout &layout) {
	for (int i = 0; i < (int)position.size(); i++) {
		vec2i pos((int)position[i].s[0], (int)position[i].s[1]);
		vec2i dir(1,1);
		if (i < (int)grid.size()) {
			// Alternate orientation to line up power rails
			dir[1] = 1-2*(grid[i].s[1]%2);
		}

		layout.inst.push_back(phy::Instance(schem->subckts[i], pos, dir));
	}

	// TODO(do remainder of layout operations)
	// 1. draw power grid
	// 2. route to power
	// 3. draw well taps
	// 4. draw filler
}

void Placement::save(phy::Library &lib) {
	save(lib.macros[root]);
}

void Placement::save() {
	save(*placer->lib);
}

}
