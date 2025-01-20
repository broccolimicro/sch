kernel void placementStep(
	global float2* positions,    // Node positions (x, y)
	global float2* velocities,   // Node velocities (vx, vy)
	global float2* forces,       // Computed forces (fx, fy)
	global ulong* ports,       // Adjacency list (flattened)
	global size_t* nets, // Start index for each node in adjacency list
	const int numNets,         // Number of nodes
	const float k,               // Optimal edge length
	const float damping,         // Damping factor
	const float time_step        // Simulation time step
) {
	int i = get_global_id(0);
	if (i >= numNets) return;

	/*
	float2 pos_i = positions[i];
	float2 force = (float2)(0.0f, 0.0f);

	// Repulsive forces (Coulomb-like repulsion)
	for (int j = 0; j < num_nodes; j++) {
		if (i == j) continue;

		float2 pos_j = positions[j];
		float2 delta = pos_i - pos_j;
		float dist2 = dot(delta, delta) + 0.01f;  // Avoid division by zero
		float dist = sqrt(dist2);
		float repulsion = k * k / dist2;

		force += repulsion * normalize(delta);
	}

	// Attractive forces (Hooke's law)
	int start = adjacency_start[i];
	int end = adjacency_start[i + 1];
	for (int j = start; j < end; j++) {
		int neighbor = adjacency[j];
		float2 pos_j = positions[neighbor];
		float2 delta = pos_j - pos_i;
		float dist = length(delta);
		float attraction = (dist * dist) / k;

		force += attraction * normalize(delta);
	}

	// Update velocity and position
	float2 velocity = velocities[i] * damping + force * time_step;
	float2 new_pos = pos_i + velocity * time_step;

	// Store results
	velocities[i] = velocity;
	positions[i] = new_pos;
	forces[i] = force;
	*/
}

