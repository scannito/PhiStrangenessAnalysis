#include <cstdlib>
#include <iostream>
#include <stdexcept>
inline int g = std::rand();   // a global initialiser that runs code at load time
void thrower() { throw std::runtime_error("x"); }
void work() { try { thrower(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << std::endl; } }
