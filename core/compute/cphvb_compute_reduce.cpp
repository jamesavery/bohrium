/*
This file is part of cphVB and copyright (c) 2012 the cphVB team:
http://cphvb.bitbucket.org

cphVB is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

cphVB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the 
GNU Lesser General Public License along with cphVB. 

If not, see <http://www.gnu.org/licenses/>.
*/
#include <cphvb.h>
#include <cphvb_compute.h>
#include "functors.hpp"
#include <complex>

template <typename T, typename Instr>
cphvb_error cphvb_compute_reduce_any( cphvb_array* op_out, cphvb_array* op_in, cphvb_index axis, cphvb_opcode opcode )
{
    Instr opcode_func;                          // Functor-pointer
                                                // Data pointers
    T* data_out = (T*) cphvb_base_array(op_out)->data;
    T* data_in  = (T*) cphvb_base_array(op_in)->data;

    cphvb_index stride      = op_in->stride[axis];
    cphvb_index nelements   = op_in->shape[axis];
    cphvb_index i, j, off0;

    if (op_in->ndim == 1) {                     // 1D special case

        *data_out = *data_in;                   // Initialize pseudo-scalar output
                                                // the value of the first element
                                                // in input.
        for(off0 = op_in->start+1, j=1; j < nelements; j++, off0 += stride ) {
            opcode_func( data_out, data_out, (data_in+off0) );
        }

        return CPHVB_SUCCESS;

    } else {                                    // ND general case

        cphvb_array tmp;                        // Copy the input-array meta-data
        cphvb_instruction instr; 
        cphvb_error err;

        tmp.base    = cphvb_base_array(op_in);
        tmp.type    = op_in->type;
        tmp.ndim    = op_in->ndim-1;
        tmp.start   = op_in->start;

        for(j=0, i=0; i < op_in->ndim; ++i) {        // Copy every dimension except for the 'axis' dimension.
            if(i != axis) {
                tmp.shape[j]    = op_in->shape[i];
                tmp.stride[j]   = op_in->stride[i];
                ++j;
            }
        }
        tmp.data = op_in->data;
       
        instr.status = CPHVB_INST_PENDING;       // Copy the first element to the output.
        instr.opcode = CPHVB_IDENTITY;
        instr.operand[0] = op_out;
        instr.operand[1] = &tmp;
        instr.operand[2] = NULL;

        //err = traverse_aa<T, T, Instr>( instr );// execute the pseudo-instruction
        err = cphvb_compute_apply( &instr );
        if (err != CPHVB_SUCCESS) {
            return err;
        }
        tmp.start += stride;

        instr.status = CPHVB_INST_PENDING;       // Reduce over the 'axis' dimension.
        instr.opcode = opcode;                   // NB: the first element is already handled.
        instr.operand[0] = op_out;
        instr.operand[1] = op_out;
        instr.operand[2] = &tmp;
        
        for(i=1; i<nelements; ++i) {
            //err = traverse_aaa<T, T, T, Instr>( instr );
            err = cphvb_compute_apply( &instr );
            if (err != CPHVB_SUCCESS) {
                return err;
            }
            tmp.start += stride;
        }

        return CPHVB_SUCCESS;
    }

}

cphvb_error cphvb_compute_reduce(cphvb_userfunc *arg, void* ve_arg)
{
    cphvb_reduce_type *a = (cphvb_reduce_type *) arg;   // Grab function arguments

    cphvb_opcode opcode = a->opcode;                    // Opcode
    cphvb_index axis    = a->axis;                      // The axis to reduce "around"

    cphvb_array *op_out = a->operand[0];                // Output operand
    cphvb_array *op_in  = a->operand[1];                // Input operand

                                                        //  Sanity checks.
    if (cphvb_operands(opcode) != 3) {
        fprintf(stderr, "ERR: opcode: %ld is not a binary ufunc, hence it is not suitable for reduction.\n", opcode);
        return CPHVB_ERROR;
    }

	if (cphvb_base_array(a->operand[1])->data == NULL) {
        fprintf(stderr, "ERR: cphvb_compute_reduce; input-operand ( op[1] ) is null.\n");
        return CPHVB_ERROR;
	}

    if (op_in->type != op_out->type) {
        fprintf(stderr, "ERR: cphvb_compute_reduce; input and output types are mixed."
                        "Probable causes include reducing over 'LESS', just dont...\n");
        return CPHVB_ERROR;
    }
    
    if (cphvb_data_malloc(op_out) != CPHVB_SUCCESS) {
        fprintf(stderr, "ERR: cphvb_compute_reduce; No memory for reduction-result.\n");
        return CPHVB_OUT_OF_MEMORY;
    }

    long int poly = opcode + (op_in->type << 8);

    switch(poly) {

        case CPHVB_ADD + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, add_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_COMPLEX128 << 8):
            return cphvb_compute_reduce_any<std::complex<double>, add_functor<std::complex<double>, std::complex<double>, std::complex<double> > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_COMPLEX64 << 8):
            return cphvb_compute_reduce_any<std::complex<float>, add_functor<std::complex<float>, std::complex<float>, std::complex<float> > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, add_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, add_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, add_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, add_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, add_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, add_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, add_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, add_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, add_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_ADD + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, add_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, subtract_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_COMPLEX128 << 8):
            return cphvb_compute_reduce_any<std::complex<double>, subtract_functor<std::complex<double>, std::complex<double>, std::complex<double> > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_COMPLEX64 << 8):
            return cphvb_compute_reduce_any<std::complex<float>, subtract_functor<std::complex<float>, std::complex<float>, std::complex<float> > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, subtract_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, subtract_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, subtract_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, subtract_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, subtract_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, subtract_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, subtract_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, subtract_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, subtract_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_SUBTRACT + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, subtract_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, multiply_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_COMPLEX128 << 8):
            return cphvb_compute_reduce_any<std::complex<double>, multiply_functor<std::complex<double>, std::complex<double>, std::complex<double> > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_COMPLEX64 << 8):
            return cphvb_compute_reduce_any<std::complex<float>, multiply_functor<std::complex<float>, std::complex<float>, std::complex<float> > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, multiply_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, multiply_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, multiply_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, multiply_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, multiply_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, multiply_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, multiply_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, multiply_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, multiply_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MULTIPLY + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, multiply_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_COMPLEX128 << 8):
            return cphvb_compute_reduce_any<std::complex<double>, divide_functor<std::complex<double>, std::complex<double>, std::complex<double> > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_COMPLEX64 << 8):
            return cphvb_compute_reduce_any<std::complex<float>, divide_functor<std::complex<float>, std::complex<float>, std::complex<float> > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, divide_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, divide_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, divide_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, divide_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, divide_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, divide_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, divide_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, divide_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, divide_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_DIVIDE + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, divide_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, power_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, power_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, power_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, power_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, power_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, power_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, power_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, power_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, power_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_POWER + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, power_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, greater_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, greater_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, greater_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, greater_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, greater_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, greater_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, greater_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, greater_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, greater_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, greater_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, greater_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, greater_equal_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, greater_equal_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, greater_equal_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, greater_equal_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, greater_equal_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, greater_equal_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, greater_equal_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, greater_equal_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, greater_equal_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, greater_equal_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_GREATER_EQUAL + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, greater_equal_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, less_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, less_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, less_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, less_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, less_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, less_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, less_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, less_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, less_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, less_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, less_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, less_equal_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, less_equal_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, less_equal_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, less_equal_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, less_equal_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, less_equal_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, less_equal_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, less_equal_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, less_equal_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, less_equal_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LESS_EQUAL + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, less_equal_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, equal_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_COMPLEX128 << 8):
            return cphvb_compute_reduce_any<std::complex<double>, equal_functor<std::complex<double>, std::complex<double>, std::complex<double> > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_COMPLEX64 << 8):
            return cphvb_compute_reduce_any<std::complex<float>, equal_functor<std::complex<float>, std::complex<float>, std::complex<float> > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, equal_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, equal_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, equal_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, equal_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, equal_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, equal_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, equal_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, equal_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, equal_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_EQUAL + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, equal_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, not_equal_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_COMPLEX128 << 8):
            return cphvb_compute_reduce_any<std::complex<double>, not_equal_functor<std::complex<double>, std::complex<double>, std::complex<double> > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_COMPLEX64 << 8):
            return cphvb_compute_reduce_any<std::complex<float>, not_equal_functor<std::complex<float>, std::complex<float>, std::complex<float> > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, not_equal_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, not_equal_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, not_equal_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, not_equal_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, not_equal_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, not_equal_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, not_equal_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, not_equal_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, not_equal_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_NOT_EQUAL + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, not_equal_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, logical_and_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, logical_and_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, logical_and_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, logical_and_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, logical_and_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, logical_and_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, logical_and_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, logical_and_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, logical_and_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, logical_and_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_AND + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, logical_and_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, logical_or_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, logical_or_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, logical_or_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, logical_or_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, logical_or_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, logical_or_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, logical_or_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, logical_or_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, logical_or_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, logical_or_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_OR + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, logical_or_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, logical_xor_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, logical_xor_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, logical_xor_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, logical_xor_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, logical_xor_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, logical_xor_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, logical_xor_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, logical_xor_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, logical_xor_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, logical_xor_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LOGICAL_XOR + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, logical_xor_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, maximum_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, maximum_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, maximum_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, maximum_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, maximum_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, maximum_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, maximum_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, maximum_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, maximum_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, maximum_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MAXIMUM + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, maximum_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, minimum_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, minimum_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, minimum_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, minimum_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, minimum_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, minimum_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, minimum_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, minimum_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, minimum_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, minimum_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MINIMUM + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, minimum_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, bitwise_and_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, bitwise_and_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, bitwise_and_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, bitwise_and_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, bitwise_and_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, bitwise_and_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, bitwise_and_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, bitwise_and_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_AND + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, bitwise_and_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, bitwise_or_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, bitwise_or_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, bitwise_or_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, bitwise_or_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, bitwise_or_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, bitwise_or_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, bitwise_or_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, bitwise_or_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_OR + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, bitwise_or_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_BOOL << 8):
            return cphvb_compute_reduce_any<cphvb_bool, bitwise_xor_functor<cphvb_bool, cphvb_bool, cphvb_bool > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, bitwise_xor_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, bitwise_xor_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, bitwise_xor_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, bitwise_xor_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, bitwise_xor_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, bitwise_xor_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, bitwise_xor_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_BITWISE_XOR + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, bitwise_xor_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, left_shift_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, left_shift_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, left_shift_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, left_shift_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, left_shift_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, left_shift_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, left_shift_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_LEFT_SHIFT + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, left_shift_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, right_shift_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, right_shift_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, right_shift_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, right_shift_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, right_shift_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, right_shift_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, right_shift_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_RIGHT_SHIFT + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, right_shift_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );
        case CPHVB_ARCTAN2 + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, arctan2_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_ARCTAN2 + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, arctan2_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_FLOAT32 << 8):
            return cphvb_compute_reduce_any<cphvb_float32, mod_functor<cphvb_float32, cphvb_float32, cphvb_float32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_FLOAT64 << 8):
            return cphvb_compute_reduce_any<cphvb_float64, mod_functor<cphvb_float64, cphvb_float64, cphvb_float64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_INT16 << 8):
            return cphvb_compute_reduce_any<cphvb_int16, mod_functor<cphvb_int16, cphvb_int16, cphvb_int16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_INT32 << 8):
            return cphvb_compute_reduce_any<cphvb_int32, mod_functor<cphvb_int32, cphvb_int32, cphvb_int32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_INT64 << 8):
            return cphvb_compute_reduce_any<cphvb_int64, mod_functor<cphvb_int64, cphvb_int64, cphvb_int64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_INT8 << 8):
            return cphvb_compute_reduce_any<cphvb_int8, mod_functor<cphvb_int8, cphvb_int8, cphvb_int8 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_UINT16 << 8):
            return cphvb_compute_reduce_any<cphvb_uint16, mod_functor<cphvb_uint16, cphvb_uint16, cphvb_uint16 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_UINT32 << 8):
            return cphvb_compute_reduce_any<cphvb_uint32, mod_functor<cphvb_uint32, cphvb_uint32, cphvb_uint32 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_UINT64 << 8):
            return cphvb_compute_reduce_any<cphvb_uint64, mod_functor<cphvb_uint64, cphvb_uint64, cphvb_uint64 > >( op_out, op_in, axis, opcode );
        case CPHVB_MOD + (CPHVB_UINT8 << 8):
            return cphvb_compute_reduce_any<cphvb_uint8, mod_functor<cphvb_uint8, cphvb_uint8, cphvb_uint8 > >( op_out, op_in, axis, opcode );

        default:
            
            return CPHVB_ERROR;

    }

}
