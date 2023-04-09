#include "Helios.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    hve::Helios helios_game{};

    try
    {
        helios_game.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}
