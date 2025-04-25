#include "util.h"
#include <sstream>

size_t lg2(size_t n)
{
  size_t c = 0;
  if(n <= 1) 
    return 0;
  
  while( (1 << c) <= n)
    {
      c++;
    }
  
  return c;
}


std::string int2string(int n)
{
  std::stringstream ss;
  ss << n;
  return ss.str();
}
