#include "Constraint.h"

namespace sch {

Index::Index() {
	type = -1;
	pin = -1;
}

Index::Index(int type, int pin) {
	this->type = type;
	this->pin = pin;
}

Index::~Index() {
}

bool operator<(const Index &i0, const Index &i1) {
	return i0.type < i1.type or (i0.type == i1.type and i0.pin < i1.pin);
}

bool operator==(const Index &i0, const Index &i1) {
	return i0.type == i1.type and i0.pin == i1.pin;
}

bool operator!=(const Index &i0, const Index &i1) {
	return i0.type != i1.type or i0.pin != i1.pin;
}

PinConstraint::PinConstraint() {
	from = -1;
	to = -1;
}

PinConstraint::PinConstraint(int from, int to) {
	this->from = from;
	this->to = to;
}

PinConstraint::~PinConstraint() {
}

bool operator==(const PinConstraint &c0, const PinConstraint &c1) {
	return c0.from == c1.from and c0.to == c1.to;
}

bool operator<(const PinConstraint &c0, const PinConstraint &c1) {
	return c0.from < c1.from or (c0.from == c1.from and c0.to < c1.to);
}

RouteConstraint::RouteConstraint() {
	wires[0] = -1;
	wires[1] = -1;
	select = -1;
	off[0] = 0;
	off[1] = 0;
}

RouteConstraint::RouteConstraint(int a, int b, int off0, int off1, int select) {
	this->wires[0] = a;
	this->wires[1] = b;
	this->select = select;
	this->off[0] = off0;
	this->off[1] = off1;
}

RouteConstraint::~RouteConstraint() {
}

bool operator==(const RouteConstraint &c0, const RouteConstraint &c1) {
	return c0.wires[0] == c1.wires[0] and c0.wires[1] == c1.wires[1];
}

bool operator<(const RouteConstraint &c0, const RouteConstraint &c1) {
	return c0.wires[0] < c1.wires[0] or (c0.wires[0] == c1.wires[0] and c0.wires[1] < c1.wires[1]);
}

ViaConstraint::ViaConstraint() {
}

ViaConstraint::ViaConstraint(Index idx) {
	this->idx = idx;
}

ViaConstraint::~ViaConstraint() {
}

RouteGroupConstraint::RouteGroupConstraint() {
	wire = -1;
}

RouteGroupConstraint::RouteGroupConstraint(int wire, Index pin) {
	this->wire = wire;
	this->pin = pin;
}

RouteGroupConstraint::~RouteGroupConstraint() {
}

}
