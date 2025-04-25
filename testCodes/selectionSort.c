void selectionSort(int *arr, int n)
{
  int i,j,m,mi,t;
  for(i=0;i<n;i++)
    {
      m = arr[i];
      mi = i;
      for(j=i+1; j<n; j++)
	{
	  if(arr[j] < m)
	    {
	      m = arr[j];
	      mi = j;
	    } 
	}
      t = arr[i];
      arr[i] = m;
      arr[mi] = t;
   }
}

#ifdef __TEST__
#include <stdio.h>
int main(int argc, char *argv[])
{
  int i;
  int arr[4] = {9,3,1,12};
  selectionSort(arr, 4);
  for(i=0;i<4;i++)
    {
      printf("arr[%d]=%d\n",i,arr[i]);
    }
  return 0;
}
#endif
