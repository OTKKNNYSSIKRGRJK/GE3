#include<pybind11/pybind11.h>
#include<Python.h>

namespace py = pybind11;

namespace Test {
	int Add(int i, int j) { return i + j; }
}

PYBIND11_MODULE(example, m) {
	m.doc() = "";

	m.def("foo", &Test::Add, "ADD");
}