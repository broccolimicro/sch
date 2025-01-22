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
			auto currSch = schem.begin()+curr;

			// Start by placing the nets for this cell
			for (int i = 0; i < (int)currCkt->nets.size(); i++) {
				currSch->nets.push_back(currSch->netsToCells.size());
				currSch->netNames.push_back(currCkt->nets[i].name);
				for (int j = 0; j < (int)currCkt->nets[i].portOf.size(); j++) {
					int index = currCkt->nets[i].portOf[j];
					auto inst = currCkt->inst.begin()+index;
					auto nextSch = schem.begin()+inst->subckt;
					auto nextCkt = lst.subckts.begin()+inst->subckt;

					if ((int)nextSch->cells.size() <= 1) {
						currSch->netsToCells.push_back(index);
					} else {
						for (int k = 0; k < (int)inst->ports.size(); k++) {
							if (inst->ports[k] == i) {
								int net = nextCkt->ports[k];
								for (size_t port = nextSch->nets[net]; port < nextSch->nets[net+1]; port++) {
									currSch->netsToCells.push_back(nextSch->netsToCells[port]*currCkt->inst.size()+index);
								}
							}
						}
					}
				}
			}

			// Do simulated annealing reduce total wirelength along the hilbert curve
			if (currCkt->inst.empty()) {
				auto currLay = lib.macros.begin()+curr;
				currSch->totalArea = currLay->box.area();
			} else {
				vector<int> index = doHier(*currCkt);
				for (auto i = index.begin(); i != index.end(); i++) {
					int next = currCkt->inst[*i].subckt;
					auto nextSch = schem.begin()+next;
					auto nextCkt = lst.subckts.begin()+next;

					cl_uint h = 0;
					if (not currSch->cellAreas.empty()) {
						 h = currSch->hilbert.back() + currSch->cellAreas.back()/2;
					}

					if ((int)nextSch->cells.size() <= 1) {
						currSch->subckts.push_back(next);
						currSch->cells.push_back(currSch->cellsToNets.size());
						currSch->hilbert.push_back(h + nextSch->totalArea/2);
						currSch->cellAreas.push_back(nextSch->totalArea);
						currSch->totalArea += nextSch->totalArea;
						currSch->cellsToNets.insert(currSch->cellsToNets.end(), currCkt->inst[*i].ports.begin(), currCkt->inst[*i].ports.end());
					} else {
						currSch->totalArea += nextSch->totalArea;
						// Then place the nets of the instances
						vector<size_t> netMap;
						netMap.resize(nextSch->nets.size(), std::numeric_limits<size_t>::max());
						for (int j = 0; j < (int)currCkt->inst[*i].ports.size(); j++) {
							netMap[nextCkt->ports[j]] = currCkt->inst[*i].ports[j];
						}

						for (int j = 0; j+1 < (int)nextSch->nets.size(); j++) {
							if (j >= (int)nextCkt->nets.size() or not nextCkt->nets[j].isIO) {
								netMap[j] = currSch->nets.size();
								currSch->nets.push_back(currSch->netsToCells.size());
								currSch->netNames.push_back("c"+idToString(*i)+"."+nextSch->netNames[j]);
								for (size_t port = nextSch->nets[j]; port < nextSch->nets[j+1]; port++) {
									currSch->netsToCells.push_back(nextSch->netsToCells[port]*currCkt->inst.size()+*i);
								}
							}
						}

						for (int j = 0; j+1 < (int)nextSch->cells.size(); j++) {
							currSch->cells.push_back(currSch->cellsToNets.size());
							currSch->subckts.push_back(nextSch->subckts[j]);
							currSch->cellAreas.push_back(nextSch->cellAreas[j]);
							currSch->hilbert.push_back(h+nextSch->hilbert[j]);
							for (size_t k = nextSch->cells[j]; k < nextSch->cells[j+1]; k++) {
								currSch->cellsToNets.push_back(netMap[nextSch->cellsToNets[k]]);
							}
						}
						netMap.clear();
					}
				}
			}
			currSch->nets.push_back(currSch->netsToCells.size());
			currSch->cells.push_back(currSch->cellsToNets.size());
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

		cout << cellName << "(" << i << ") " << schem[root].cellAreas[i] << ": {";
		for (size_t j = start; j < end; j++) {
			cout << schem[root].cellsToNets[j] << " ";
		}
		cout << "}" << endl;
	}

	cout << "nets: " << schem[root].nets.size() << endl;
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
	// Compute total half perimeter wire length. Estimate the
	// expected perimeter of an interval on the hilbert curve as
	// `sqrt(length)*4` assuming that allocated spaces on the hilbert
	// curve tend to be rectangular and the expected area of an
	// interval as `length`.
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
	// TODO(edward.bingham) I need to convert position coordinates from the
	// universal domain to the scaled domain of the layout problem.

	cl_uint num = (schem[root].cells.size()-1);
	position.resize(num);
	index.resize(num);

	size_t positionSize = num * sizeof(cl_uint2);
	size_t indexSize = num * sizeof(cl_uint);
	size_t hilbertSize = num * sizeof(cl_uint);
	size_t areaSize = num * sizeof(cl_uint);
	try {
		cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE, positionSize);
		cl::Buffer indexBuffer(context, CL_MEM_READ_WRITE, indexSize);
		cl::Buffer hilbertBuffer(context, CL_MEM_READ_WRITE, hilbertSize);	
		cl::Buffer areaBuffer(context, CL_MEM_READ_WRITE, areaSize);	

		initPlacement.setArg(0, positionBuffer);
		initPlacement.setArg(1, indexBuffer);
		initPlacement.setArg(2, hilbertBuffer);
		initPlacement.setArg(3, areaBuffer);
		initPlacement.setArg(4, num);
		initPlacement.setArg(5, schem[root].totalArea);

		queue.enqueueWriteBuffer(areaBuffer, CL_TRUE, 0, areaSize, schem[root].cellAreas.data());
		queue.enqueueWriteBuffer(hilbertBuffer, CL_TRUE, 0, hilbertSize, schem[root].hilbert.data());

		queue.enqueueNDRangeKernel(initPlacement, cl::NullRange, cl::NDRange(num), cl::NullRange);
		queue.finish();

		queue.enqueueReadBuffer(positionBuffer, CL_TRUE, 0, positionSize, position.data());
		queue.enqueueReadBuffer(indexBuffer, CL_TRUE, 0, indexSize, index.data());
	} catch (cl::Error &err) {
		std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		exit(1);
	}
}

void Placement::doDetail() {
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

void Placement::doLegal() {
	// For legalization, we need to first divide the space up into columns, then
	// divide the space up into rows. Column clusters should seek to reduce the
	// standard deviation of the height of the cells within each row while
	// maximizing the number of cells in the column (to a point). Row clusters
	// should seek to reduce the standard deviation of the total width of the
	// column across rows while bucketing cells by cell height.

	// This algorithm also needs to be easily parallelizeable.
	
	// Lets start by just doing the stupid thing. Divide the space into an equal
	// number of columns, then divide each column into an equal number of rows.

	// 
}

void Placement::save(phy::Library &lib, const sch::Netlist &lst) {
	cl_uint side = isqrt(schem[root].totalArea);
	// apply a buffer
	side += side>>3;
	// ensure nonzero
	if (side == 0) {
		side = 1;
	}

	cl_uint scale = std::numeric_limits<cl_uint>::max()/side;

	for (int i = 0; i < (int)position.size(); i++) {
		int idx = schem[root].subckts[i];
		string cellName = "nil";
		if (idx < (int)lst.subckts.size()) {
			cellName = lst.subckts[idx].name;
		}
		position[i].s[0] = position[i].s[0]/scale;
		position[i].s[1] = position[i].s[1]/scale;
		cout << cellName << "(" << i << "): {" << position[i].s[0] << " " << position[i].s[1] << "} " << index[i] << " " << schem[root].hilbert[i] << endl;
		
		lib.macros[root].inst.push_back(phy::Instance(idx, vec2i((int)position[i].s[0], (int)position[i].s[1])));
	}
}

}
