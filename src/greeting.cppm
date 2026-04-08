module;
#include <iostream>

export module greeting;

export void greet(std::string name) {
    std::cout << "Hello, " << name << "!\n";
}
