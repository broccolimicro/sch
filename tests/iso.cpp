#include <gtest/gtest.h>

#include <sch/Placer.h>
#include <random>
#include <algorithm>

using namespace sch;
using namespace std;

Subckt genRand(int n) {
	Subckt ckt;
	ckt.name = "test";

	vector<int> nets;
	nets.resize(n*n, 0);
	for (int i = 0; i < n*n; i++) {
		nets[i] = ckt.pushNet("n" + to_string(i));
	}

	random_shuffle(nets.begin(), nets.end());

	// Create a highly symmetric graph for testing. Every permutation of this
	// graph should create the same canonical labelling.
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			ckt.pushMos(-1, Model::NMOS, nets[i*n+j], nets[i*n+j], nets[((i+1)%n)*n+j]);
			ckt.pushMos(-1, Model::NMOS, nets[i*n+j], nets[i*n+j], nets[((i+n-1)%n)*n+j]);
			ckt.pushMos(-1, Model::NMOS, nets[i*n+j], nets[i*n+j], nets[i*n+(j+1)%n]);
			ckt.pushMos(-1, Model::NMOS, nets[i*n+j], nets[i*n+j], nets[i*n+(j+n-1)%n]);
		}
	}
	return ckt;
}

TEST(iso, canonical)
{
	int n = 5;
	Subckt ckt = genRand(n);
	auto canon = ckt.canonicalLabeling().generate(ckt);

	int equal = 0;
	int count = 100;
	for (int i = 0; i < count; i++) {
		Subckt test = genRand(n);
		auto canon2 = test.canonicalLabeling().generate(test);
		int cmp = canon.compare(canon2);
		printf("result %d\n", cmp);
		equal += (cmp == 0);
	}

	EXPECT_EQ(equal, count);
}


