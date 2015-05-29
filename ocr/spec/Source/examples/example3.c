
/* Example usage of Finish EDT in FFT.
 *
 * Implements the following dependence graph:
 *
 * MainEdt
 *    |
 *
 * FinishEdt
 * {
 *       DFT
 *      /   \
 * FFT-odd FFT-even
 *      \   /
 *     Twiddle
 * }
 *    |
 * Shutdown
 *
 */


#include "ocr.h"
#include "math.h"

#define N          256
#define BLOCK_SIZE 16

void ditfft2(double *X_real, double *X_imag, double *x_in, u32 size, u32 step) {
  if(size == 1) {
    X_real[0] = x_in[0];
    X_imag[0] = 0;
  } else {
    ditfft2(X_real, X_imag, x_in, size/2, 2 * step);
    ditfft2(X_real+size/2, X_imag+size/2, x_in+step, size/2, 2 * step);
    u32 k;
    for(k=0;k<size/2;k++) {
      double t_real = X_real[k];
      double t_imag = X_imag[k];
      double twiddle_real = cos(-2 * M_PI * k / size);
      double twiddle_imag = sin(-2 * M_PI * k / size);
      double xr = X_real[k+size/2];
      double xi = X_imag[k+size/2];

      // (a+bi)(c+di) = (ac - bd) + (bc + ad)i
      X_real[k] = t_real +
          (twiddle_real*xr - twiddle_imag*xi);
      X_imag[k] = t_imag +
          (twiddle_imag*xr + twiddle_real*xi);
      X_real[k+size/2] = t_real -
          (twiddle_real*xr - twiddle_imag*xi);
      X_imag[k+size/2] = t_imag -
          (twiddle_imag*xr + twiddle_real*xi);
    }
  }
}

ocrGuid_t fftComputeEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
  u32 i;
  ocrGuid_t computeGuid = paramv[0];
  ocrGuid_t twiddleGuid = paramv[1];
  double *data = (double*)depv[0].ptr;
  ocrGuid_t dataGuid = depv[0].guid;
  u64 size = paramv[2];
  u64 step = paramv[3];
  u64 offset = paramv[4];
  u64 step_offset = paramv[5];
  u64 blockSize = paramv[6];
  double *x_in = (double*)data;
  double *X_real = (double*)(data+offset + size*step);
  double *X_imag = (double*)(data+offset + 2*size*step);

  PRINTF("COMPUTE START %lu\n", size);
  if(size <= blockSize) {
    ditfft2(X_real, X_imag, x_in+step_offset, size, step);
  } else {
    // DFT even side
    u64 childParamv[7] = { computeGuid, twiddleGuid, size/2, 2 * step,
               0 + offset, step_offset, blockSize };
    u64 childParamv2[7] = { computeGuid, twiddleGuid, size/2, 2 * step,
                size/2 + offset, step_offset + step, blockSize };

    ocrGuid_t edtGuid, edtGuid2, twiddleEdtGuid, finishEventGuid, finishEventGuid2;

    ocrEdtCreate(&edtGuid, computeGuid, EDT_PARAM_DEF, childParamv,
         EDT_PARAM_DEF, NULL_GUID, EDT_PROP_FINISH, NULL_GUID,
         &finishEventGuid);
    ocrEdtCreate(&edtGuid2, computeGuid, EDT_PARAM_DEF, childParamv2,
         EDT_PARAM_DEF, NULL_GUID, EDT_PROP_FINISH, NULL_GUID,
         &finishEventGuid2);

    ocrGuid_t twiddleDependencies[3] = { dataGuid, finishEventGuid, finishEventGuid2 };
    ocrEdtCreate(&twiddleEdtGuid, twiddleGuid, EDT_PARAM_DEF, paramv, 3,
         twiddleDependencies, EDT_PROP_FINISH, NULL_GUID, NULL);

    ocrAddDependence(dataGuid, edtGuid, 0, DB_MODE_RW);
    ocrAddDependence(dataGuid, edtGuid2, 0, DB_MODE_RW);
  }

PRINTF("COMPUTE END %lu\n", size);
  return NULL_GUID;
}

ocrGuid_t fftTwiddleEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
  double *data = (double*)depv[0].ptr;
  u64 size = paramv[2];
  u64 step = paramv[3];
  u64 offset = paramv[4];
  double *x_in = (double*)data+offset;
  double *X_real = (double*)(data+offset + size*step);
  double *X_imag = (double*)(data+offset + 2*size*step);

  PRINTF("TWIDDLE START %lu\n", size);
  ditfft2(X_real, X_imag, x_in, size, step);
  PRINTF("TWIDDLE END %lu\n", size);

  return NULL_GUID;
}

ocrGuid_t endEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t dataGuid = paramv[0];

  PRINTF("END\n");
  ocrDbDestroy(dataGuid);
  ocrShutdown();
  return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

  ocrGuid_t computeTempGuid, twiddleTempGuid, endTempGuid;
  ocrEdtTemplateCreate(&computeTempGuid, &fftComputeEdt, 7, 1);
  ocrEdtTemplateCreate(&twiddleTempGuid, &fftTwiddleEdt, 7, 3);
  ocrEdtTemplateCreate(&endTempGuid, &endEdt, 1, 1);
  u32 i;
  double *x;

  ocrGuid_t dataGuid;
  ocrDbCreate(&dataGuid, (void **) &x, sizeof(double) * N * 3, DB_PROP_NONE, NULL_GUID, NO_ALLOC);

  // Cook up some arbitrary data
  for(i=0;i<N;i++) {
      x[i] = 0;
  }
  x[0] = 1;

  u64 edtParamv[7] = { computeTempGuid, twiddleTempGuid, N, 1, 0, 0, BLOCK_SIZE };
  ocrGuid_t edtGuid, eventGuid, endGuid;

  // Launch compute EDT
  ocrEdtCreate(&edtGuid, computeTempGuid, EDT_PARAM_DEF, edtParamv,
               EDT_PARAM_DEF, NULL_GUID, EDT_PROP_FINISH, NULL_GUID,
               &eventGuid);

  // Launch finish EDT
  ocrEdtCreate(&endGuid, endTempGuid, EDT_PARAM_DEF, &dataGuid,
               EDT_PARAM_DEF, NULL_GUID, EDT_PROP_FINISH, NULL_GUID,
               NULL);

  ocrAddDependence(dataGuid, edtGuid, 0, DB_MODE_RW);
  ocrAddDependence(eventGuid, endGuid, 0, DB_MODE_RW);

  return NULL_GUID;
}
