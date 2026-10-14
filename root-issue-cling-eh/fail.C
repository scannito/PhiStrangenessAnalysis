#include <iostream>
#include <stdexcept>
void thrower() { throw std::runtime_error("x"); }
void fail() { try { thrower(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << std::endl; } }
