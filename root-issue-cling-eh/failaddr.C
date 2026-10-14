#include <iostream>
#include <stdexcept>
void thrower() { throw std::runtime_error("x"); }
void failaddr() {
  std::cerr << "[macro] address of failaddr  = " << (void*)&failaddr << std::endl;
  std::cerr << "[macro] address of thrower   = " << (void*)&thrower << std::endl;
  try { thrower(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << std::endl; } }
