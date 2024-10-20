#include <gtest/gtest.h>

#include <sch/Placer.h>
#include <random>
#include <algorithm>

using namespace sch;
using namespace std;

Subckt genRand(int n, bool dev=false) {
	Subckt ckt;
	ckt.name = "test";

	vector<int> nets;
	nets.resize(n*n, 0);
	for (int i = 0; i < n*n; i++) {
		nets[i] = ckt.pushNet("n" + to_string(i));
	}

	std::default_random_engine rand(0/*std::random_device{}()*/);
	shuffle(nets.begin(), nets.end(), rand);

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

	if (dev) {
		ckt.pushMos(-1, Model::NMOS, nets[rand()%25], nets[rand()%25], nets[rand()%25]);
	}
	return ckt;
}

TEST(iso, canonical_equal)
{
	int n = 5;
	Subckt ckt = genRand(n);
	auto canon = Mapping(ckt).remap(canonicalLabels(ckt)).generate(ckt, "cell");

	int equal = 0;
	int count = 100;
	for (int i = 0; i < count; i++) {
		Subckt test = genRand(n);
		auto canon2 = Mapping(test).remap(canonicalLabels(test)).generate(test, "cell");
		int cmp = canon.compare(canon2);
		equal += (cmp == 0);
	}

	EXPECT_EQ(equal, count);
}

TEST(iso, canonical_not_equal)
{
	int n = 5;
	Subckt ckt = genRand(n);
	auto canon = Mapping(ckt).remap(canonicalLabels(ckt)).generate(ckt, "cell");

	int equal = 0;
	int count = 100;
	for (int i = 0; i < count; i++) {
		Subckt test = genRand(n, true);
		auto canon2 = Mapping(test).remap(canonicalLabels(test)).generate(test, "cell");
		int cmp = canon.compare(canon2);
		equal += (cmp == 0);
	}

	EXPECT_EQ(equal, 0);
}

