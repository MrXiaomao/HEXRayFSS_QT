//
// MATLAB Compiler: 8.1 (R2020b)
// Date: Fri Aug 22 14:06:49 2025
// Arguments:
// "-B""macro_default""-W""cpplib:func_waveCorrect""-T""link:lib""func_waveCorre
// ct.m""-C"
//

#ifndef func_waveCorrect_h
#define func_waveCorrect_h 1

#if defined(__cplusplus) && !defined(mclmcrrt_h) && defined(__linux__)
#  pragma implementation "mclmcrrt.h"
#endif
#include "mclmcrrt.h"
#include "mclcppclass.h"
#ifdef __cplusplus
extern "C" { // sbcheck:ok:extern_c
#endif

/* This symbol is defined in shared libraries. Define it here
 * (to nothing) in case this isn't a shared library. 
 */
#ifndef LIB_func_waveCorrect_C_API 
#define LIB_func_waveCorrect_C_API /* No special import/export declaration */
#endif

/* GENERAL LIBRARY FUNCTIONS -- START */

extern LIB_func_waveCorrect_C_API 
bool MW_CALL_CONV func_waveCorrectInitializeWithHandlers(
       mclOutputHandlerFcn error_handler, 
       mclOutputHandlerFcn print_handler);

extern LIB_func_waveCorrect_C_API 
bool MW_CALL_CONV func_waveCorrectInitialize(void);

extern LIB_func_waveCorrect_C_API 
void MW_CALL_CONV func_waveCorrectTerminate(void);

extern LIB_func_waveCorrect_C_API 
void MW_CALL_CONV func_waveCorrectPrintStackTrace(void);

/* GENERAL LIBRARY FUNCTIONS -- END */

/* C INTERFACE -- MLX WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- START */

extern LIB_func_waveCorrect_C_API 
bool MW_CALL_CONV mlxFunc_waveCorrect(int nlhs, mxArray *plhs[], int nrhs, mxArray 
                                      *prhs[]);

/* C INTERFACE -- MLX WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- END */

#ifdef __cplusplus
}
#endif


/* C++ INTERFACE -- WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- START */

#ifdef __cplusplus

/* On Windows, use __declspec to control the exported API */
#if defined(_MSC_VER) || defined(__MINGW64__)

#ifdef EXPORTING_func_waveCorrect
#define PUBLIC_func_waveCorrect_CPP_API __declspec(dllexport)
#else
#define PUBLIC_func_waveCorrect_CPP_API __declspec(dllimport)
#endif

#define LIB_func_waveCorrect_CPP_API PUBLIC_func_waveCorrect_CPP_API

#else

#if !defined(LIB_func_waveCorrect_CPP_API)
#if defined(LIB_func_waveCorrect_C_API)
#define LIB_func_waveCorrect_CPP_API LIB_func_waveCorrect_C_API
#else
#define LIB_func_waveCorrect_CPP_API /* empty! */ 
#endif
#endif

#endif

extern LIB_func_waveCorrect_CPP_API void MW_CALL_CONV func_waveCorrect(int nargout, mwArray& wave, mwArray& wave_rms, const mwArray& dataAll, const mwArray& rom);

/* C++ INTERFACE -- WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- END */
#endif

#endif
