/*
 * DIPlib 3.0
 * This file contains definitions of functions that implement the percentile filter.
 *
 * (c)2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "diplib.h"
#include "diplib/nonlinear.h"
#include "diplib/framework.h"
#include "diplib/pixel_table.h"
#include "diplib/overload.h"

namespace dip {

namespace {

template< typename TPI >
class PercentileLineFilter : public Framework::FullLineFilter {
   public:
      PercentileLineFilter( dfloat percentile ) : fraction_( percentile / 100.0 ) {}
      void SetNumberOfThreads( dip::uint threads ) override {
         buffers_.resize( threads );
      }
      virtual void Filter( Framework::FullLineFilterParameters const& params ) override {
         TPI* in = static_cast< TPI* >( params.inBuffer.buffer );
         dip::sint inStride = params.inBuffer.stride;
         TPI* out = static_cast< TPI* >( params.outBuffer.buffer );
         dip::sint outStride = params.outBuffer.stride;
         dip::uint length = params.bufferLength;
         PixelTableOffsets const& pixelTable = params.pixelTable;
         dip::uint N = pixelTable.NumberOfPixels();
         buffers_[ params.thread ].resize( N );
         dip::sint rank = static_cast< dip::sint >( static_cast< dfloat >( N ) * fraction_ );
         for( dip::uint ii = 0; ii < length; ++ii ) {
            TPI* buffer = buffers_[ params.thread ].data();
            for( auto offset : pixelTable ) {
               *buffer = in[ offset ];
               ++buffer;
            }
            auto ourGuy = buffers_[ params.thread ].begin() + rank;
            std::nth_element( buffers_[ params.thread ].begin(), ourGuy, buffers_[ params.thread ].end() );
            *out = *ourGuy;
            in += inStride;
            out += outStride;
         }
      }
   private:
      dfloat fraction_;
      std::vector< std::vector< TPI >> buffers_;
};

} // namespace

void PercentileFilter(
      Image const& in,
      Image& out,
      dfloat percentile,
      Kernel const& kernel,
      StringArray const& boundaryCondition
) {
   DIP_THROW_IF( !in.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( kernel.HasWeights(), E::KERNEL_NOT_BINARY );
   DIP_THROW_IF(( percentile < 0.0 ) || ( percentile > 100.0 ), E::PARAMETER_OUT_OF_RANGE );
   DIP_START_STACK_TRACE
      BoundaryConditionArray bc = StringArrayToBoundaryConditionArray( boundaryCondition );
      DataType dtype = in.DataType();
      std::unique_ptr< Framework::FullLineFilter > lineFilter;
      DIP_OVL_NEW_NONCOMPLEX( lineFilter, PercentileLineFilter, ( percentile ), dtype );
      Framework::Full( in, out, dtype, dtype, dtype, 1, bc, kernel, *lineFilter, Framework::Full_AsScalarImage );
   DIP_END_STACK_TRACE
}

} // namespace dip
