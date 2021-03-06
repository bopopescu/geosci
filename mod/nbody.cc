#include <cassert>
#include <cmath>
#include <cstdio>
#include <mkl_vsl.h>
#include <mpi.h>
#include <omp.h>

struct ParticleSet { 
  float *x, *y, *z;
  float *vx, *vy, *vz; 
};

void MoveParticles(const int nParticles, ParticleSet& particle, const float dt, const int mpiRank, const int mpiWorldSize, const int myParticles) {

  const int startParticle = (mpiRank    )*myParticles;
  const int endParticle   = (mpiRank + 1)*myParticles;

#ifdef __MIC__
  const int tileSize = 16;
#else
  const int tileSize = 8;
#endif

  assert(myParticles%tileSize == 0);

#pragma omp parallel for schedule(guided)
  for (int ii = startParticle; ii < endParticle; ii += tileSize) { 
  
      float Fx[tileSize], Fy[tileSize], Fz[tileSize];
      Fx[:] = Fy[:] = Fz[:] = 0;
    
      const float softening = 1e-20;
  
      for (int j = 0; j < nParticles; j++) { 
    
#pragma vector aligned
	for (int i = ii; i < ii + tileSize; i++) {

	  const float dx = particle.x[j] - particle.x[i];
	  const float dy = particle.y[j] - particle.y[i];
	  const float dz = particle.z[j] - particle.z[i];
	  const float rr = 1.0f/sqrtf(dx*dx + dy*dy + dz*dz + softening);
	  const float drPowerN32  = rr*rr*rr;
	
	  Fx[i-ii] += dx * drPowerN32;  
	  Fy[i-ii] += dy * drPowerN32;  
	  Fz[i-ii] += dz * drPowerN32;
	}
      }

      particle.vx[ii:tileSize] += dt*Fx[0:tileSize]; 
      particle.vy[ii:tileSize] += dt*Fy[0:tileSize]; 
      particle.vz[ii:tileSize] += dt*Fz[0:tileSize];
  }

#pragma simd
#pragma vector aligned
  for (int i = 0 ; i < nParticles; i++) { 
    particle.x[i]  += particle.vx[i]*dt;
    particle.y[i]  += particle.vy[i]*dt;
    particle.z[i]  += particle.vz[i]*dt;
  }

  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.x,  myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.y,  myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.z,  myParticles, MPI_FLOAT, MPI_COMM_WORLD);

}

int main(const int argc, const char** argv) {

  int mpiWorldSize, mpiRank;
  MPI_Init((int*)&argc, (char***)&argv);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  
  const int nParticles = (argc > 1 ? atoi(argv[1]) : 16384);
  assert(nParticles%mpiWorldSize == 0);
  const int myParticles = nParticles/mpiWorldSize;
  const int nSteps = 100000;  
  const float dt = 0.01f; 

  ParticleSet particle;
  particle.x  = new float[nParticles];
  particle.y  = new float[nParticles];
  particle.z  = new float[nParticles];
  particle.vx = new float[nParticles];
  particle.vy = new float[nParticles];
  particle.vz = new float[nParticles];

  VSLStreamStatePtr rnStream;  
  vslNewStream( &rnStream, VSL_BRNG_MT19937, 1 );
  vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, nParticles, particle.x,  -1.0f, 1.0f);
  vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, nParticles, particle.y,  -1.0f, 1.0f);
  vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, nParticles, particle.z,  -1.0f, 1.0f);
  vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, nParticles, particle.vx, -1.0f, 1.0f);
  vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, nParticles, particle.vy, -1.0f, 1.0f);
  vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, nParticles, particle.vz, -1.0f, 1.0f);

  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.x,  myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.y,  myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.z,  myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.vx, myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.vy, myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, particle.vz, myParticles, MPI_FLOAT, MPI_COMM_WORLD);
  
  if (mpiRank == 0) {
    printf("\nPropagating %d particles using %d processes with %d threads each on %s...\n\n", 
	   nParticles, mpiWorldSize, omp_get_max_threads(),
#ifndef __MIC__
	   "CPU"
#else
	   "MIC"
#endif
	   );
  }
  double rate = 0, dRate = 0; 
  const int skipSteps = 3; 

  if (mpiRank==0) 
    printf("\033[1m%5s %10s %10s %8s\033[0m\n", "Step", "Time, s", "Interact/s", "GFLOP/s"); fflush(stdout);
  for (int step = 1; step <= nSteps; step++) {

    const double tStart = omp_get_wtime(); 
    MoveParticles(nParticles, particle, dt, mpiRank, mpiWorldSize, myParticles);
    const double tEnd = omp_get_wtime();

    const float HztoInts   = float(nParticles)*float(nParticles-1) ;
    const float HztoGFLOPs = 20.0*1e-9*float(nParticles)*float(nParticles-1);

    if (step > skipSteps) { 
      rate  += HztoGFLOPs/(tEnd - tStart); 
      dRate += HztoGFLOPs*HztoGFLOPs/((tEnd - tStart)*(tEnd-tStart)); 
    }

    if (mpiRank==0)
      printf("%5d %10.3e %10.3e %8.1f %s\n", 
	     step, (tEnd-tStart), HztoInts/(tEnd-tStart), HztoGFLOPs/(tEnd-tStart), (step<=skipSteps?"*":""));
    fflush(stdout);
  }
  rate/=(double)(nSteps-skipSteps); 
  dRate=sqrt(dRate/(double)(nSteps-skipSteps)-rate*rate);
  if (mpiRank==0) {
    printf("-----------------------------------------------------\n");
    printf("\033[1m%s %4s \033[42m%10.1f +- %.1f GFLOP/s\033[0m\n",
	 "Average performance:", "", rate, dRate);
    printf("-----------------------------------------------------\n");
    printf("* - warm-up, not included in average\n\n");
  }
  delete particle.x;
  delete particle.y;
  delete particle.z;
  delete particle.vx;
  delete particle.vy;
  delete particle.vz;

  MPI_Finalize();
}
