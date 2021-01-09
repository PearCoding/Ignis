#include <fstream>
#include <iostream>

#include "generated_test_interface.h"

int main(int argc, char** argv)
{
	int err = test_main();

	if (err != 0)
		std::cout << err << " failed tests!" << std::endl;
	else
		std::cout << "All tests passed :)" << std::endl;

	return err;
}
