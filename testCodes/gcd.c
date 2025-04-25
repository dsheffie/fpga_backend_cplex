unsigned gcd(unsigned a, unsigned b)
{
  if(a==0)
    return b;
  
  while(b!=0)
    {
      if(a>b)
	a = a-b;
      else
	b = b-a;
    }
  return a;
}


#ifdef DBG
#include <stdio.h>
int main()
{
  unsigned a=6,b=9; 
  unsigned u = gcd(6,9);
  printf("gcd(%u,%u)=%u\n",a,b,u);
  return 0;
}
#endif
