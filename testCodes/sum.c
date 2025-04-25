int sum(int *arr, int n)
{
  int i,s = 0;
  for(i=0;i<n;i++)
    s+=arr[i];
  return s;
}
