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
					auto nextLay = lib.macros.begin()+next;
					auto nextSch = schem.begin()+next;
					auto nextCkt = lst.subckts.begin()+next;

					cl_uint h = 0;
					if (not currSch->cellBounds.empty()) {
						cl_uint2 bound = currSch->cellBounds.back();
						h = currSch->hilbert.back() + (bound.s[0]*bound.s[1])/2;
					}

					if ((int)nextSch->cells.size() <= 1) {
						currSch->subckts.push_back(next);
						currSch->cells.push_back(currSch->cellsToNets.size());
						currSch->hilbert.push_back(h + nextSch->totalArea/2);
						currSch->cellBounds.push_back({(cl_uint)nextLay->box.width(), (cl_uint)nextLay->box.height()});
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
							currSch->cellBounds.push_back(nextSch->cellBounds[j]);
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

		cout << cellName << "(" << i << ") " << schem[root].cellBounds[i].s[0] << "," << schem[root].cellBounds[i].s[1] << ": {";
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
	position.resize(num);
	index.resize(num);

	size_t positionSize = num * sizeof(cl_uint2);
	size_t indexSize = num * sizeof(cl_uint);
	size_t hilbertSize = num * sizeof(cl_uint);
	try {
		cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE, positionSize);
		cl::Buffer indexBuffer(context, CL_MEM_READ_WRITE, indexSize);
		cl::Buffer hilbertBuffer(context, CL_MEM_READ_WRITE, hilbertSize);	
		initPlacement.setArg(0, positionBuffer);
		initPlacement.setArg(1, indexBuffer);
		initPlacement.setArg(2, hilbertBuffer);
		initPlacement.setArg(3, num);
		initPlacement.setArg(4, schem[root].totalArea);

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

void Placement::doLegal() {
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

	/*cl_uint num = (schem[root].cells.size()-1);
	position.resize(num);
	index.resize(num);

	size_t positionSize = num * sizeof(cl_uint2);
	size_t indexSize = num * sizeof(cl_uint);
	size_t hilbertSize = num * sizeof(cl_uint);
	size_t boundsSize = num * sizeof(cl_uint);
	try {
		cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE, positionSize);
		cl::Buffer indexBuffer(context, CL_MEM_READ_WRITE, indexSize);
		cl::Buffer hilbertBuffer(context, CL_MEM_READ_WRITE, hilbertSize);	
		cl::Buffer boundsBuffer(context, CL_MEM_READ_WRITE, boundsSize);	

		initPlacement.setArg(0, positionBuffer);
		initPlacement.setArg(1, indexBuffer);
		initPlacement.setArg(2, hilbertBuffer);
		initPlacement.setArg(3, boundsBuffer);
		initPlacement.setArg(4, num);
		initPlacement.setArg(5, schem[root].totalArea);

		queue.enqueueWriteBuffer(boundsBuffer, CL_TRUE, 0, boundsSize, schem[root].cellBounds.data());
		queue.enqueueWriteBuffer(hilbertBuffer, CL_TRUE, 0, hilbertSize, schem[root].hilbert.data());

		queue.enqueueNDRangeKernel(initPlacement, cl::NullRange, cl::NDRange(num), cl::NullRange);
		queue.finish();

		queue.enqueueReadBuffer(positionBuffer, CL_TRUE, 0, positionSize, position.data());
		queue.enqueueReadBuffer(indexBuffer, CL_TRUE, 0, indexSize, index.data());
	} catch (cl::Error &err) {
		std::cerr << "OpenCL Error: " << err.what() << " (" << err.err() << ")" << std::endl;
		exit(1);
	}*/



	/*vector<int> indices; // cell indices ordered by y-coordinate from bottom to top
	vector<int> columns; // index into the indices array of balanced columns
	
	for (int i = 0; i < (int)position.size(); i++) {
		indices.push_back(i);
	}*/

	
}

void Placement::save(phy::Library &lib, const sch::Netlist &lst) {
	for (int i = 0; i < (int)position.size(); i++) {
		int idx = schem[root].subckts[i];
		string cellName = "nil";
		if (idx < (int)lst.subckts.size()) {
			cellName = lst.subckts[idx].name;
		}
		cout << cellName << "(" << i << "): {" << position[i].s[0] << " " << position[i].s[1] << "} " << index[i] << " " << schem[root].hilbert[i] << endl;
		
		lib.macros[root].inst.push_back(phy::Instance(idx, vec2i((int)position[i].s[0], (int)position[i].s[1])));
	}

	// 1. draw power grid
	// 2. route to power
	// 3. draw well taps
	// 4. draw filler
}

}
