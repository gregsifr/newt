/*
  Functions copied from Numerical Recipes in C
*/

#include <math.h>

/* Random number generator ran1 from Computers in Physics */
/* Volume 6 No. 5, 1992, 522-524, Press and Teukolsky */
/* To generate real random numbers 0.0-1.0 */
/* Should be seeded with a negative integer */
#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NTAB 32
#define EPS (1.2E-07)
#define MAX(a,b) (a>b)?a:b
#define MIN(a,b) (a<b)?a:b

float ran1(long *idum)
{
        int j,k;
	static int iv[NTAB],iy=0;
	void nrerror();
        static float NDIV = 1.0/(1.0+(IM-1.0)/NTAB);
        static float RNMX = (1.0-EPS);
        static float AM = (1.0/IM);

	if ((*idum <= 0) || (iy == 0)) {
	        if (-(*idum) < 1) *idum = 1;
	        else *idum = -(*idum);
		*idum = MAX(-*idum,*idum);
                for(j=NTAB+7;j>=0;j--) {
			k = *idum/IQ;
			*idum = IA*(*idum-k*IQ)-IR*k;
			if(*idum < 0) *idum += IM;
			if(j < NTAB) iv[j] = *idum;
		}
		iy = iv[0];
	}
	k = *idum/IQ;
	*idum = IA*(*idum-k*IQ)-IR*k;
	if(*idum<0) *idum += IM;
	j = iy*NDIV;
	iy = iv[j];
	iv[j] = *idum;
	return MIN(AM*iy,RNMX);
}
#undef IA 
#undef IM 
#undef IQ
#undef IR
#undef NTAB
#undef EPS 
#undef MAX
#undef MIN


/*
  Pseudo-random sample from normal distributiion with mean 0, sd 1.
*/
float gasdev(long *idum)
{
    float ran1(long *idum);
    static int iset=0;
    static double gset;
    double fac,rsq,v1,v2;

    if (iset == 0) {
        do {
            v1=2.0*ran1(idum)-1.0;
            v2=2.0*ran1(idum)-1.0;
            rsq=v1*v1+v2*v2;
        } while (rsq >= 1.0 || rsq == 0.0);
        fac=sqrt(-2.0*log(rsq)/rsq);
        gset=v1*fac;
        iset=1;
        return (float)(v2*fac);
    } else {
        iset=0;
        return (float)gset;
    }
}

