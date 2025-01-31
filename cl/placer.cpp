inline ulong hilbertFromCartesian(uint2 v) {
	uchar state = 0;
	ulong s = 0;
	for (char i = 31; i > 0; i--) {
		uchar row = 4*state | 2*((v.x >> i) & 1) | (v.y >> i) & 1;
		s = (s << 2) | (0x361E9CB4 >> 2*row) & 3;
		state = (0x8FE65831 >> 2*row) & 3;
	}
	return s;
}

inline uint2 cartesianFromHilbert(ulong s) {
	uint2 vp = {0,0};
	uchar state = 0;
	for (char i = 62; i >= 0; i -= 2) {
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

kernel void initPlacement(
	global uint2* position,
	global ulong* hilbert,
	const uint num,
	const ulong total,
	const uint scale
) {
	uint i = get_global_id(0);
	if (i >= num) return;

	ulong h = hilbert[i] * (ULONG_MAX / total);
	position[i] = cartesianFromHilbert(h) / scale;
}

kernel void stepPlacement(
	// Nets to Cells
	global uint* netsToCells,
	global uint* nets,
	// Cells to Nets
	global uint* cellsToNets,
	global uint* cells,
	// Delauny Triangulation
	global uint* neighbors, // At most 3*numCells-6 elements
	const int numNets,
	const int numCells,
	// Current positions of Cells
	global float2* positions,
	global float2* velocities,
	global float2* forces
) {
	uint i = get_global_id(0);
	if (i >= numCells) return;

  // Local variables for managing the workgroup's data
	uint groupSize = numCells / get_num_groups(0);
  uint groupStart = get_group_id(0) * groupSize;  // Start of this workgroup's segment of the array
	uint localSize = groupSize / get_local_size(0); 
  uint localStart = get_local_id(0)*localSize + groupStart;  // Local start index within the workgroup
  uint localEnd = localStart + localSize;  // Local end index within the workgroup
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
