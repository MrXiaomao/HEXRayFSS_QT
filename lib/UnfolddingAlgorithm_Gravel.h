//
// MATLAB Compiler: 8.1 (R2020b)
// Date: Wed Sep 24 11:19:43 2025
// Arguments:
// "-B""macro_default""-W""cpplib:UnfolddingAlgorithm_Gravel""-T""link:lib""Unfo
// lddingAlgorithm_Gravel.m""-C"
//

#ifndef UnfolddingAlgorithm_Gravel_h
#define UnfolddingAlgorithm_Gravel_h 1

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
#ifndef LIB_UnfolddingAlgorithm_Gravel_C_API 
#define LIB_UnfolddingAlgorithm_Gravel_C_API /* No special import/export declaration */
#endif

/* GENERAL LIBRARY FUNCTIONS -- START */

extern LIB_UnfolddingAlgorithm_Gravel_C_API 
bool MW_CALL_CONV UnfolddingAlgorithm_GravelInitializeWithHandlers(
       mclOutputHandlerFcn error_handler, 
       mclOutputHandlerFcn print_handler);

extern LIB_UnfolddingAlgorithm_Gravel_C_API 
bool MW_CALL_CONV UnfolddingAlgorithm_GravelInitialize(void);

extern LIB_UnfolddingAlgorithm_Gravel_C_API 
void MW_CALL_CONV UnfolddingAlgorithm_GravelTerminate(void);

extern LIB_UnfolddingAlgorithm_Gravel_C_API 
void MW_CALL_CONV UnfolddingAlgorithm_GravelPrintStackTrace(void);

/* GENERAL LIBRARY FUNCTIONS -- END */

/* C INTERFACE -- MLX WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- START */

extern LIB_UnfolddingAlgorithm_Gravel_C_API 
bool MW_CALL_CONV mlxUnfolddingAlgorithm_Gravel(int nlhs, mxArray *plhs[], int nrhs, 
                                                mxArray *prhs[]);

/* C INTERFACE -- MLX WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- END */

#ifdef __cplusplus
}
#endif


/* C++ INTERFACE -- WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- START */

#ifdef __cplusplus

/* On Windows, use __declspec to control the exported API */
#if defined(_MSC_VER) || defined(__MINGW64__)

#ifdef EXPORTING_UnfolddingAlgorithm_Gravel
#define PUBLIC_UnfolddingAlgorithm_Gravel_CPP_API __declspec(dllexport)
#else
#define PUBLIC_UnfolddingAlgorithm_Gravel_CPP_API __declspec(dllimport)
#endif

#define LIB_UnfolddingAlgorithm_Gravel_CPP_API PUBLIC_UnfolddingAlgorithm_Gravel_CPP_API

#else

#if !defined(LIB_UnfolddingAlgorithm_Gravel_CPP_API)
#if defined(LIB_UnfolddingAlgorithm_Gravel_C_API)
#define LIB_UnfolddingAlgorithm_Gravel_CPP_API LIB_UnfolddingAlgorithm_Gravel_C_API
#else
#define LIB_UnfolddingAlgorithm_Gravel_CPP_API /* empty! */ 
#endif
#endif

#endif

extern LIB_UnfolddingAlgorithm_Gravel_CPP_API void MW_CALL_CONV UnfolddingAlgorithm_Gravel(int nargout, mwArray& unfold_seq, mwArray& unfold_spec, const mwArray& t, const mwArray& seq, const mwArray& data, const mwArray& responce_matrix);

/* C++ INTERFACE -- WRAPPERS FOR USER-DEFINED MATLAB FUNCTIONS -- END */
#endif

#endif
