#include <gtest/gtest.h>

#include <sch/Placer.h>
#include <random>
#include <algorithm>

using namespace sch;
using namespace std;

Subckt genRand(int n, bool dev=false, bool swap=false) {
	Subckt ckt;
	ckt.name = "test";

	vector<int> nets;
	nets.resize(n*n, 0);
	for (int i = 0; i < n*n; i++) {
		nets[i] = ckt.pushNet("n" + to_string(i));
	}
	int base = ckt.pushNet("base");

	std::default_random_engine rand(0/*std::random_device{}()*/);
	shuffle(nets.begin(), nets.end(), rand);

	// Create a highly symmetric graph for testing. Every permutation of this
	// graph should create the same canonical labelling.
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			int select = swap ? rand()%2 : 0;
			ckt.pushMos(-1, Model::NMOS, (select ? nets[((i+1)%n)*n+j] : nets[i*n+j]), nets[i*n+j], (select ? nets[i*n+j] : nets[((i+1)%n)*n+j]), base);
			select = swap ? rand()%2 : 0;
			ckt.pushMos(-1, Model::NMOS, (select ? nets[((i+n-1)%n)*n+j] : nets[i*n+j]), nets[i*n+j], (select ? nets[i*n+j] : nets[((i+n-1)%n)*n+j]), base);
			select = swap ? rand()%2 : 0;
			ckt.pushMos(-1, Model::NMOS, (select ? nets[i*n+(j+1)%n] : nets[i*n+j]), nets[i*n+j], (select ? nets[i*n+j] : nets[i*n+(j+1)%n]), base);
			select = swap ? rand()%2 : 0;
			ckt.pushMos(-1, Model::NMOS, (select ? nets[i*n+(j+n-1)%n] : nets[i*n+j]), nets[i*n+j], (select ? nets[i*n+j] : nets[i*n+(j+n-1)%n]), base);
		}
	}

	if (dev) {
		ckt.pushMos(-1, Model::NMOS, nets[rand()%25], nets[rand()%25], nets[rand()%25], base);
	}
	return ckt;
}

TEST(iso, canonical_equal)
{
	std::default_random_engine rand(0/*std::random_device{}()*/);
	int n = 5;
	Subckt ckt = genRand(n);
	ckt.canonicalize();

	int equal = 0;
	int count = 100;
	for (int i = 0; i < count; i++) {
		bool swap = rand()%2;
		Subckt test = genRand(n, false, swap);
		test.canonicalize();
		int cmp = ckt.compare(test);
		equal += (cmp == 0);
	}

	EXPECT_EQ(equal, count);
}

TEST(iso, canonical_not_equal)
{
	std::default_random_engine rand(0/*std::random_device{}()*/);
	int n = 5;
	Subckt ckt = genRand(n);
	ckt.canonicalize();

	int equal = 0;
	int count = 100;
	for (int i = 0; i < count; i++) {
		bool swap = rand()%2;
		Subckt test = genRand(n, true, swap);
		test.canonicalize();
		int cmp = ckt.compare(test);
		equal += (cmp == 0);
	}

	EXPECT_EQ(equal, 0);
}

