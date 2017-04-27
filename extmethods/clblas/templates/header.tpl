/**********************************************************
 * Do not edit this file. It has been auto generated by   *
 * ../core/codegen/gen_extmethod.py at @!timestamp!@.  *
 **********************************************************/
#include <bh_extmethod.hpp>
#include "../ve/opencl/engine_opencl.hpp"

#include <clBLAS.h>

#include <stdio.h>
#include <map>

using namespace bohrium;
using namespace extmethod;
using namespace std;

namespace {

    void clblasSgemmt(clblasOrder layout, clblasTranspose TransA, clblasTranspose TransB, const int M, const int N, const int K, const bh_float32 alpha, const cl_mem A, const int offA, const int lda, const cl_mem B, const int offB, const int ldb, const bh_float32 beta, cl_mem C, const int offC, const int ldc, const int queueNum, cl_command_queue *queue, const int eventNum, const cl_event* eventWaitList, cl_event *event) {
        clblasSgemm(layout, TransA, TransB, K, N, M, alpha, A, offA, K, B, offB, K, beta, C, offC, ldc, queueNum, queue, eventNum, eventWaitList, event);
    }

    void clblasDgemmt(clblasOrder layout, clblasTranspose TransA, clblasTranspose TransB, const int M, const int N, const int K, const bh_float64 alpha, const cl_mem A, const int offA, const int lda, const cl_mem B, const int offB, const int ldb, const bh_float32 beta, cl_mem C, const int offC, const int ldc, const int queueNum, cl_command_queue *queue, const int eventNum, const cl_event* eventWaitList, cl_event *event) {
        clblasDgemm(layout, TransA, TransB, K, N, M, alpha, A, offA, K, B, offB, K, beta, C, offC, ldc, queueNum, queue, eventNum, eventWaitList, event);
    }

    void clblasCgemmt(clblasOrder layout, clblasTranspose TransA, clblasTranspose TransB, const int M, const int N, const int K, const cl_float2 alpha, const cl_mem A, const int offA, const int lda, const cl_mem B, const int offB, const int ldb, const cl_float2 beta, cl_mem C, const int offC, const int ldc, const int queueNum, cl_command_queue *queue, const int eventNum, const cl_event* eventWaitList, cl_event *event) {
        clblasCgemm(layout, TransA, TransB, K, N, M, alpha, A, offA, K, B, offB, K, beta, C, offC, ldc, queueNum, queue, eventNum, eventWaitList, event);
    }

    void clblasZgemmt(clblasOrder layout, clblasTranspose TransA, clblasTranspose TransB, const int M, const int N, const int K, const cl_double2 alpha, const cl_mem A, const int offA, const int lda, const cl_mem B, const int offB, const int ldb, const cl_double2 beta, cl_mem C, const int offC, const int ldc, const int queueNum, cl_command_queue *queue, const int eventNum, const cl_event* eventWaitList, cl_event *event) {
        clblasZgemm(layout, TransA, TransB, K, N, M, alpha, A, offA, K, B, offB, K, beta, C, offC, ldc, queueNum, queue, eventNum, eventWaitList, event);
    }

    @!body!@
} /* end of namespace */

@!footer!@