#include <fstream>
#include <iostream>

#include "generated_test_interface.h"

int main(int argc, char** argv)
{
	int err = 0;

	err += test_matrix();
	err += test_intersection();

	if (err != 0)
		std::cout << err << " failed tests!" << std::endl;
	else
		std::cout << "All tests passed :)" << std::endl;

	return err;
}
