// n is at most 32
inline ulong hilbertFromCartesian(uint2 v, char n) {
	uchar state = 0;
	ulong s = 0;
	for (char i = n-1; i >= 0; i--) {
		uchar row = 4*state | 2*((v.x >> i) & 1) | (v.y >> i) & 1;
		s = (s << 2) | (0x361E9CB4 >> 2*row) & 3;
		state = (0x8FE65831 >> 2*row) & 3;
	}
	return s;
}

// n is at most 32
inline uint2 cartesianFromHilbert(ulong s, char n) {
	uint2 vp = {0,0};
	uchar state = 0;
	for (char i = 2*(n-1); i >= 0; i -= 2) {
		uchar row = 4*state | (uchar)((s >> i) & 3);
		vp.x = (vp.x << 1) | (0x936C >> row) & 1;
		vp.y = (vp.y << 1) | (0x39C6 >> row) & 1;
		state = (0x3E6B94C1 >> 2*row) & 3;
	}
	return vp;
}


inline uint isqrt(ulong x) {
	// Limits and midpoint
	ulong a, b, m;
	a = 1;
	b = (x >> 5) + 8;
	if (b > UINT_MAX) {
		b = UINT_MAX;
	}
	do {
		m = (a + b) >> 1;
		if (m*m > x) {
			b = m - 1;
		} else {
			a = m + 1;
		}
	} while (b >= a);
	return (uint)(a - 1);
}

inline ulong2 isqrt2(ulong2 x) {
	x.x = isqrt(x.x);
	x.y = isqrt(x.y);
	return x;
}

// https://codingforspeed.com/using-faster-exponential-approximation/
// v - value to exp()
// n - number of bits below decimal
inline ulong fixedExp(ulong v, char n)
{
	v = (v >> 10) + (1ul<<(n-10));
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	v = (v*v) >> n;
	return v;
}

inline ulong2 fixedExp2(ulong2 v, char n)
{
	v.x = fixedExp(v.x, n);
	v.y = fixedExp(v.y, n);
	return v;
}

kernel void globalStep(
	global uint3* position,
	global ulong* hilbert,
	const uint num,
	const ulong total,
	const uint scale
) {
	uint i = get_global_id(0);
	if (i >= num) return;

	ulong h = hilbert[i] * (ULONG_MAX / total);
	if (scale == 0) {
		position[i].s0 = 0;
		position[i].s1 = 0;
	} else {
		position[i].s01 = cartesianFromHilbert(h, 32) / scale;
	}
	position[i].s2 = i;
}

// Delauny Triangulation
//	global uint* neighbors, // At most 3*numCells-6 elements


kernel void detailStep0(
	// INPUTS
	global uint* netsToCells,// (index into areas)[nets[numNets]]
	global uint* nets,       // (index into netsToCells)[numNets+1]
	const int numNets,
	global uint* cellsToNets,// (cell area)[numCells]
	global uint* cells,      // (cell area)[numCells]
	global uint* areas,      // (cell area)[numCells]
	const int numCells,
	global uint4* positions, // cell-x, cell-y, hilbert, index

	// OUTPUTS
	global ulong4* wirelse,
	global ulong4* cluster, // QuadTree (mean-x, mean-y, var-x, var-y)[(2^treeLevel)^2]
	global ulong2* clusterArea, // QuadTree (amplitude, amplitude2)[(2^treeLevel)^2]
	const char treeLevel
) {
	uint i = get_global_id(0);
	if (i < numCells) {
		// Local variables for managing the workgroup's data
		//uint groupSize = numCells / get_num_groups(0);
		//uint groupStart = get_group_id(0) * groupSize;  // Start of this workgroup's segment of the array
		//uint localSize = groupSize / get_local_size(0); 
		//uint localStart = get_local_id(0)*localSize + groupStart;  // Local start index within the workgroup
		//uint localEnd = localStart + localSize;  // Local end index within the workgroup

		// TODO
		char gamma = 0;

		uint j = positions[i].s3;
		uint2 pos = positions[i].s01;

		// 1. identify cluster index 
		uint area = areas[j];

		uint2 node = pos >> (31 - treeLevel);
		uint index = hilbertFromCartesian(node, treeLevel);
		ulong2 mean = (ulong)area*convert_ulong2(pos);
		ulong2 var = mean*convert_ulong2(pos);

		// This would be the proper thing to do
		// 0. sort by hilbert position to cluster nodes in a quad into workgroups
		// 2. compute mean, variance, and amplitude
		// 3. reduce-sum to quad-tree

		// Do the stupid thing first (will need to figure out atomicity)
		cluster[index].s01 += mean.s01;
		cluster[index].s23 += var.s01;
		clusterArea[index].s0 += area;

		ulong4 lse;
		lse.s0 = fixedExp(pos.s0, gamma);
		lse.s1 = fixedExp(-pos.s0, gamma);
		lse.s2 = fixedExp(pos.s1, gamma);
		lse.s3 = fixedExp(-pos.s1, gamma);

		for (int k = cells[j]; k < cells[j+1]; k++) {
			// Do the stupid thing first (will need to figure out atomicity)
			wirelse[cellsToNets[k]] += lse;
		}
	}
}

kernel void detailStep1(
	// Nets to Cells
	global uint* netsToCells,
	global uint* nets,
	const int numNets,
	// Cells to Nets
	global uint* cellsToNets,
	global uint* cells,
	global uint* areas,
	const int numCells,
	// Quad Tree
	global ulong4* wirelse,
	global ulong4* cluster,
	global ulong2* clusterArea,
	const ulong side,
	const char treeLevel,

	// Current positions of Cells
	global uint4* positions, // cell-x, cell-y, hilbert, index
	global uint2* velocities,
	global uint2* forces
) {
	uint i = get_global_id(0);
	if (i >= numCells) return;

	uint j = positions[i].s3;

  // Local variables for managing the workgroup's data
	//uint groupSize = numCells / get_num_groups(0);
  //uint groupStart = get_group_id(0) * groupSize;  // Start of this workgroup's segment of the array
	//uint localSize = groupSize / get_local_size(0); 
  //uint localStart = get_local_id(0)*localSize + groupStart;  // Local start index within the workgroup
  //uint localEnd = localStart + localSize;  // Local end index within the workgroup

	uint2 pos = positions[i].s01;
	uint area = areas[j];

	// 1. identify cluster index 
	uint2 node = pos.s01 >> (31 - treeLevel);
	uint c = hilbertFromCartesian(node, treeLevel);

	// 2. apply repulsive force from cluster
	ulong demand = clusterArea[c].s0;
	uint2 mean = convert_uint2(cluster[c].s01 / demand);
	uint2 var = convert_uint2(cluster[c].s23 / demand) - mean*mean;

	ulong nodeWidth = side >> treeLevel;
	ulong capacity = nodeWidth*nodeWidth;

	// Compute gradient of overflow
	uint2 off = pos - mean;

	// y = exp(-off*off/(2*var))/isqrt(var*2*pi)
	// dy/dx = -y*off/var

	// TODO
	uint gamma = 0;
	uint pi = 3; // lol
	uint lambda = 1;

	// fill = (demand/capacity)*y
	ulong2 fill = (demand*fixedExp2(convert_ulong2(-off*off/(2*var)), gamma))/(capacity*isqrt2(convert_ulong2(var*2*pi)));
	ulong2 push = -fill*convert_ulong2(off/var);

	// Maybe apply force from neighboring cluster/s as well?

	
	// 3. apply attractive forces from nets
	ulong2 pull;
	pull.s0 = 0;
	pull.s1 = 0;

	ulong4 lse;
	lse.s0 = fixedExp(pos.s0, gamma);
	lse.s1 = fixedExp(-pos.s0, gamma);
	lse.s2 = fixedExp(pos.s1, gamma);
	lse.s3 = fixedExp(-pos.s1, gamma);

	for (uint k = cells[j]; k < cells[j+1]; k++) {
		ulong4 total = wirelse[netsToCells[k]];
		pull.s0 += lse.s0/total.s0 - lse.s1/total.s1;
		pull.s1 += lse.s2/total.s2 - lse.s3/total.s3;
	}
	
	// 4. apply forces to cells
	forces[j] = -convert_uint2(pull + lambda*push);
	velocities[j] += forces[j]/areas[j];
	positions[i].s01 += velocities[j];
	positions[i].s2 = hilbertFromCartesian(positions[i].s01, 32);
}

kernel void partitionCols(
	global uint3* position,
	global uint2* bound,
	const uint num,
	global uint* col,
	global uint* colHeight,
	global uint* colTotalWidth,
	global uint* colCount,
	const uint colWidth
) {
	uint i = get_global_id(0);
	if (i >= num) return;

	uint c = position[i].x / colWidth;
	atomic_add(&colHeight[c], bound[i].y);
	atomic_add(&colTotalWidth[c], bound[i].x);
	atomic_inc(&colCount[c]);
	col[i] = c;
}

kernel void partitionRows(
	global uint2* position,
	const uint num,
	global uint* col,
	global uint* colHeight,
	global uint* colCount,
	global uint* row
) {
	uint i = get_global_id(0);
	if (i >= num) return;

	uint colIndex = col[i];
	uint doubleAverageHeight = (2 * colHeight[colIndex]) / colCount[colIndex];
	row[i] = 2*(position[i].y / doubleAverageHeight);
}
