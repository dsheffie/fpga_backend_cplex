int insertSort(int *arr, int n)
{
  int i,j;
  int t;
 
  for(i = 1; i < n; i++)
    {
      j = i;
      while(arr[j] < arr[j-1])
	{
	  t = arr[j];
	  arr[j] = arr[j-1];
	  arr[j-1] = t;
	  j--;
	  if(j < 1) break;
	}
     }

  return arr[n-1];
}
