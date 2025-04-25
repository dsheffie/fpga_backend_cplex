void fvvadd(float *y, float *a, float *b, int n)
{
  int i;
  for(i=0;i<n;i++)
    {
      y[i] = a[i] + b[i];
    }
} 
