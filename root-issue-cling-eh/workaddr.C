#include <cstdlib>
#include <iostream>
#include <stdexcept>
inline int g = std::rand();
void thrower() { throw std::runtime_error("x"); }
void workaddr() {
  std::cerr << "[macro] address of workaddr  = " << (void*)&workaddr << std::endl;
  std::cerr << "[macro] address of thrower   = " << (void*)&thrower << std::endl;
  try { thrower(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << std::endl; } }
