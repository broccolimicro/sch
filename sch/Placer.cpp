#include "Placer.h"
#include "Kernel.h"
#include <limits>
#include <random>

#include <CL/opencl.hpp>

using namespace std;

namespace sch {

Schematic::Schematic() {
	cells = std::numeric_limits<uint64_t>::max();
}

Schematic::~Schematic() {
}

void Placement::configure(int platformId, int deviceId, bool debug) {
	configureSource("placementStep", placer_cl_string, platformId, deviceId, debug); 
}

void Placement::configurePath(string kernelName, string kernelPath, int platformId, int deviceId, bool debug) {
	string source;
	FILE *fptr = fopen(kernelPath.c_str(), "r");
	fseek(fptr, 0, SEEK_END);
	int size = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	source.resize(size, '\0');
	fread(source.data(), 1, size, fptr);
	fclose(fptr);

	configureSource(kernelName, source, platformId, deviceId, debug);
}

void Placement::configureSource(string kernelName, string source, int platformId, int deviceId, bool debug) {
	string platformName = "No Platform";
	string deviceName = "No Device";
	size_t maxComputeUnits = 0;
	size_t maxWorkGroupSize = 0;

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
			context = cl::Context({devices[deviceId]});
		}
	}
	
	if (debug) {
		cout << "Configured for " << platformName << ", " << deviceName << endl;
		cout << "CL_DEVICE_MAX_COMPUTE_UNITS: " << maxComputeUnits << endl;
		cout << "CL_DEVICE_MAX_WORK_GROUP_SIZE: " << maxWorkGroupSize << endl;
	}

	queue = cl::CommandQueue(context);
	cl::Program::Sources sources;
	sources.push_back(source.c_str());

	program = cl::Program(context, sources);
	program.build();
	
	cl::Kernel kernel(program, kernelName.c_str());

	size_t kernelMaxWorkGroupSize = 0;
	size_t preferredWorkGroupMultiple = 0;
	if (deviceId < (int)devices.size()) {
		kernel.getWorkGroupInfo(devices[deviceId], CL_KERNEL_WORK_GROUP_SIZE, &kernelMaxWorkGroupSize);
		kernel.getWorkGroupInfo(devices[deviceId], CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, &preferredWorkGroupMultiple);
	}

	if (debug) {
		std::cout << "CL_KERNEL_WORK_GROUP_SIZE: " << kernelMaxWorkGroupSize << std::endl;
		std::cout << "CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: " << preferredWorkGroupMultiple << std::endl;
	}
}

void Placement::load(const Netlist &lst, int root, bool debug) {
	this->root = root;
	schem.resize(lst.subckts.size());

	vector<int> stack(1, root);
	while (not stack.empty()) {
		int curr = stack.back();
		auto currCkt = lst.subckts.begin()+curr;
		
		bool done = true;
		for (auto i = currCkt->inst.begin(); i != currCkt->inst.end(); i++) {
			if (schem[i->subckt].cells == std::numeric_limits<uint64_t>::max()) {
				done = false;
				stack.erase(remove(stack.begin(), stack.end(), i->subckt), stack.end());
				stack.push_back(i->subckt);
			}
		}

		if (done) {
			schem[curr].cells = 0;
			auto currSch = schem.begin()+curr;

			// Start by placing the nets for this cell
			for (int i = 0; i < (int)currCkt->nets.size(); i++) {
				currSch->nets.push_back(currSch->ports.size());
				for (int j = 0; j < (int)currCkt->nets[i].portOf.size(); j++) {
					int index = currCkt->nets[i].portOf[j];
					auto inst = currCkt->inst.begin()+index;
					auto nextSch = schem.begin()+inst->subckt;
					auto nextCkt = lst.subckts.begin()+inst->subckt;

					if (nextSch->cells == 0) {
						currSch->ports.push_back(index);
					} else {
						for (int k = 0; k < (int)inst->ports.size(); k++) {
							if (inst->ports[k] == i) {
								int net = nextCkt->ports[k];
								for (size_t port = nextSch->nets[net]; port < nextSch->nets[net+1]; port++) {
									currSch->ports.push_back(nextSch->ports[port]*currCkt->inst.size()+index);
								}
							}
						}
					}
				}
			}

			for (int i = 0; i < (int)currCkt->inst.size(); i++) {
				int next = currCkt->inst[i].subckt;
				auto nextSch = schem.begin()+next;
				auto nextCkt = lst.subckts.begin()+next;

				if (nextSch->cells == 0) {
					++currSch->cells;
				} else {
					currSch->cells += nextSch->cells;

					// Then place the nets of the instances
					for (int j = 0; j < (int)nextCkt->nets.size(); j++) {
						if (not nextCkt->nets[j].isIO) {
							currSch->nets.push_back(currSch->ports.size());
							for (size_t port = nextSch->nets[j]; port < nextSch->nets[j+1]; port++) {
								currSch->ports.push_back(nextSch->ports[port]*currCkt->inst.size()+i);
							}
						}
					}
				}
			}
			currSch->nets.push_back(currSch->ports.size());

			stack.pop_back();
		}
	}

	cout << schem[root].cells << endl;
	for (int i = 0; i+1 < (int)schem[root].nets.size(); i++) {
		size_t start = schem[root].nets[i];
		size_t end = schem[root].nets[i+1];
		cout << "Net " << i << ": {";
		for (size_t j = start; j < end; j++) {
			cout << schem[root].ports[j] << " ";
		}
		cout << "}" << endl;
	}

	velocity.resize(schem[root].cells, {0.0f,0.0f});
	force.resize(schem[root].cells, {0.0f,0.0f});
	
	position.reserve(schem[root].cells);
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(0.0f, 10.0f);
	for (size_t i = 0; i < (size_t)schem[root].cells; i++) {
		position.push_back({dist(gen), dist(gen)});
	}
}

void Placement::run() {
	for (int i = 0; i < (int)position.size(); i++) {
		cout << "Node " << i << ": {" << position[i].s[0] << " " << position[i].s[1] << "}" << endl;
	}

	size_t dataSize = schem[root].cells * sizeof(cl_float2);
	cl::Buffer positionBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, position.data());
	cl::Buffer velocityBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, velocity.data());
	cl::Buffer forceBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, dataSize, force.data());

	size_t portsSize = schem[root].ports.size() * sizeof(uint64_t);
	size_t netsSize = schem[root].nets.size() * sizeof(size_t);
	cl::Buffer portsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, portsSize, schem[root].ports.data());
	cl::Buffer netsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, netsSize, schem[root].nets.data());
	
	size_t nets = schem[root].nets.size()-1;
	float k = 1.0f;
	float damping = 0.85f;
	float step = 0.1f;
	float end = 100.0f;

	kernel.setArg(0, positionBuffer);
	kernel.setArg(1, velocityBuffer);
	kernel.setArg(2, forceBuffer);
	kernel.setArg(3, portsBuffer);
	kernel.setArg(4, netsBuffer);
	kernel.setArg(5, nets);
	kernel.setArg(6, k);
	kernel.setArg(7, damping);
	kernel.setArg(8, step);

	int i = 0;
	for (float t = 0.0f; t < end; t+=step) {
		cout << "step " << i << " " << t << "/" << end << endl;
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(nets), cl::NullRange);
		queue.finish();
		i++;
	}

	cout << "Simulated " << i << " steps" << endl << endl;

	queue.enqueueReadBuffer(positionBuffer, CL_TRUE, 0, dataSize, position.data());

	for (int i = 0; i < (int)position.size(); i++) {
		cout << "Node " << i << ": {" << position[i].s[0] << " " << position[i].s[1] << "}" << endl;
	}
}

void Placement::save(phy::Layout &layout, const sch::Netlist &lst) {
	for (int i = 0; i < (int)position.size(); i++) {
		layout.inst.push_back(phy::Instance(lst.cellAt(root, i), vec2i((int)(position[i].s[0]*100), (int)(position[i].s[1]*100))));
	}
}

}
