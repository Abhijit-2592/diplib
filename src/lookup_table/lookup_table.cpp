/*
 * DIPlib 3.0
 * This file contains definitions for look-up tables and related functionality
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
#include "diplib/lookup_table.h"
#include "diplib/framework.h"
#include "diplib/overload.h"

namespace dip {

namespace {

template< typename TPI >
inline void FillPixel( TPI* out, dip::uint length, dip::sint stride, TPI value ) {
   for( dip::uint ii = 0; ii < length; ++ii ) {
      *out = value;
      out += stride;
   }
}

template< typename TPI >
inline void CopyPixel( TPI const* in, TPI* out, dip::uint length, dip::sint inStride, dip::sint outStride ) {
   for( dip::uint ii = 0; ii < length; ++ii ) {
      *out = *in;
      in += inStride;
      out += outStride;
   }
}

template< typename TPI >
inline void CopyPixelWithInterpolation( TPI const* in, TPI* out, dip::uint length, dip::sint inStride,
                                        dip::sint outStride, dfloat fraction, dip::sint interpStride ) {
   for( dip::uint ii = 0; ii < length; ++ii ) {
      *out = static_cast< TPI >( *in * ( 1 - fraction ) + *( in + interpStride ) * fraction );
      in += inStride;
      out += outStride;
   }
}

template< typename TPI >
class dip__DirectLUT_Integer : public Framework::ScanLineFilter {
      // Applies the LUT with data type TPI, and no index, to an input image of type uint32.
   public:
      virtual void Filter( Framework::ScanLineFilterParameters const& params ) override {
         uint32 const* in = static_cast< uint32 const* >( params.inBuffer[ 0 ].buffer );
         auto bufferLength = params.bufferLength;
         auto inStride = params.inBuffer[ 0 ].stride;
         TPI* out = static_cast< TPI* >( params.outBuffer[ 0 ].buffer );
         auto outStride = params.outBuffer[ 0 ].stride;
         auto tensorLength = params.outBuffer[ 0 ].tensorLength;
         auto outTensorStride = params.outBuffer[ 0 ].tensorStride;
         TPI const* values = static_cast< TPI const* >( values_.Origin() );
         auto valuesStride = values_.Stride( 0 );
         auto valuesTensorStride = values_.TensorStride();
         DIP_ASSERT( values_.DataType() == DataType( TPI( 0 )));
         DIP_ASSERT( values_.TensorElements() == tensorLength );
         dip::uint maxIndex = values_.Size( 0 ) - 1;
         for( dip::uint ii = 0; ii < bufferLength; ++ii ) {
            dip::uint index = *in;
            if( index > maxIndex ) {
               switch( outOfBoundsMode_ ) {
                  case LookupTable::OutOfBoundsMode::USE_OUT_OF_BOUNDS_VALUE:
                     FillPixel( out, tensorLength, outTensorStride, outOfBoundsValue_ );
                     break;
                  case LookupTable::OutOfBoundsMode::KEEP_INPUT_VALUE:
                     FillPixel( out, tensorLength, outTensorStride, clamp_cast< TPI >( index ) );
                     break;
                  //case LookupTable::OutOfBoundsMode::CLAMP_TO_RANGE:
                  default:
                     CopyPixel( values + static_cast< dip::sint >( maxIndex ) * valuesStride, out, tensorLength, valuesTensorStride, outTensorStride );
                     break;
               }
            } else {
               CopyPixel( values + static_cast< dip::sint >( index ) * valuesStride, out, tensorLength, valuesTensorStride, outTensorStride );
            }
            in += inStride;
            out += outStride;
         }
      }
      dip__DirectLUT_Integer( Image const& values, LookupTable::OutOfBoundsMode outOfBoundsMode, dfloat outOfBoundsValue )
            : values_( values ), outOfBoundsMode_( outOfBoundsMode ), outOfBoundsValue_( clamp_cast< TPI >( outOfBoundsValue )) {}
   private:
      Image const& values_;
      LookupTable::OutOfBoundsMode outOfBoundsMode_;
      TPI outOfBoundsValue_;
};

template< typename TPI >
class dip__DirectLUT_Float : public Framework::ScanLineFilter {
      // Applies the LUT with data type TPI, and no index, to an input image of type dfloat.
   public:
      virtual void Filter( Framework::ScanLineFilterParameters const& params ) override {
         dfloat const* in = static_cast< dfloat const* >( params.inBuffer[ 0 ].buffer );
         auto bufferLength = params.bufferLength;
         auto inStride = params.inBuffer[ 0 ].stride;
         TPI* out = static_cast< TPI* >( params.outBuffer[ 0 ].buffer );
         auto outStride = params.outBuffer[ 0 ].stride;
         auto tensorLength = params.outBuffer[ 0 ].tensorLength;
         auto outTensorStride = params.outBuffer[ 0 ].tensorStride;
         TPI const* values = static_cast< TPI const* >( values_.Origin() );
         auto valuesStride = values_.Stride( 0 );
         auto valuesTensorStride = values_.TensorStride();
         DIP_ASSERT( values_.DataType() == DataType( TPI( 0 )));
         DIP_ASSERT( values_.TensorElements() == tensorLength );
         dip::uint maxIndex = values_.Size( 0 ) - 1;
         for( dip::uint ii = 0; ii < bufferLength; ++ii ) {
            if(( *in < 0 ) || ( *in > static_cast< dfloat >( maxIndex ))) {
               switch( outOfBoundsMode_ ) {
                  case LookupTable::OutOfBoundsMode::USE_OUT_OF_BOUNDS_VALUE:
                     FillPixel( out, tensorLength, outTensorStride, outOfBoundsValue_ );
                     break;
                  case LookupTable::OutOfBoundsMode::KEEP_INPUT_VALUE:
                     FillPixel( out, tensorLength, outTensorStride, clamp_cast< TPI >( *in ) );
                     break;
                  //case LookupTable::OutOfBoundsMode::CLAMP_TO_RANGE:
                  default:
                     TPI const* tval = values + ( *in < 0.0 ? 0 : static_cast< dip::sint >( maxIndex )) * valuesStride;
                     CopyPixel( tval, out, tensorLength, valuesTensorStride, outTensorStride );
                     break;
               }
            } else {
               switch( interpolation_ ) {
                  case LookupTable::InterpolationMode::LINEAR: {
                     dip::uint index = clamp_cast< dip::uint >( *in );
                     dfloat fraction = *in - static_cast< dfloat >( index );
                     if( fraction == 0.0 ) {
                        // not just to avoid the extra computation, it especially avoids out-of-bounds indexing if in points at the last LUT element.
                        CopyPixel( values + static_cast< dip::sint >( index ) * valuesStride, out, tensorLength,
                                   valuesTensorStride, outTensorStride );
                     } else {
                        CopyPixelWithInterpolation( values + static_cast< dip::sint >( index ) * valuesStride,
                                                    out, tensorLength, valuesTensorStride, outTensorStride,
                                                    fraction, valuesStride );
                     }
                     break;
                  }
                  case LookupTable::InterpolationMode::NEAREST_NEIGHBOR: {
                     dip::uint index = clamp_cast< dip::uint >( std::round( *in ));
                     CopyPixel( values + static_cast< dip::sint >( index ) * valuesStride, out, tensorLength,
                                valuesTensorStride, outTensorStride );
                     break;
                  }
                  case LookupTable::InterpolationMode::ZERO_ORDER_HOLD: {
                     dip::uint index = clamp_cast< dip::uint >( *in );
                     CopyPixel( values + static_cast< dip::sint >( index ) * valuesStride, out, tensorLength,
                                valuesTensorStride, outTensorStride );
                     break;
                  }
               }
            }
            in += inStride;
            out += outStride;
         }
      }
      dip__DirectLUT_Float( Image const& values, LookupTable::OutOfBoundsMode outOfBoundsMode,
                            dfloat outOfBoundsValue, LookupTable::InterpolationMode interpolation )
            : values_( values ), outOfBoundsMode_( outOfBoundsMode ),
              outOfBoundsValue_( clamp_cast< TPI >( outOfBoundsValue )), interpolation_( interpolation ) {}
   private:
      Image const& values_;
      LookupTable::OutOfBoundsMode outOfBoundsMode_;
      TPI outOfBoundsValue_;
      LookupTable::InterpolationMode interpolation_;
};

template< typename TPI >
class dip__IndexedLUT_Float : public Framework::ScanLineFilter {
      // Applies the LUT with data type TPI, and an index, to an input image of type dfloat.
   public:
      virtual void Filter( Framework::ScanLineFilterParameters const& params ) override {
         dfloat const* in = static_cast< dfloat const* >( params.inBuffer[ 0 ].buffer );
         auto bufferLength = params.bufferLength;
         auto inStride = params.inBuffer[ 0 ].stride;
         TPI* out = static_cast< TPI* >( params.outBuffer[ 0 ].buffer );
         auto outStride = params.outBuffer[ 0 ].stride;
         auto tensorLength = params.outBuffer[ 0 ].tensorLength;
         auto outTensorStride = params.outBuffer[ 0 ].tensorStride;
         TPI const* values = static_cast< TPI const* >( values_.Origin() );
         auto valuesStride = values_.Stride( 0 );
         auto valuesTensorStride = values_.TensorStride();
         DIP_ASSERT( values_.DataType() == DataType( TPI( 0 )));
         DIP_ASSERT( values_.TensorElements() == tensorLength );
         dip::uint maxIndex = values_.Size( 0 ) - 1;
         for( dip::uint ii = 0; ii < bufferLength; ++ii ) {
            if(( *in < index_.front() ) || ( *in > index_.back() )) {
               switch( outOfBoundsMode_ ) {
                  case LookupTable::OutOfBoundsMode::USE_OUT_OF_BOUNDS_VALUE:
                     FillPixel( out, tensorLength, outTensorStride, outOfBoundsValue_ );
                     break;
                  case LookupTable::OutOfBoundsMode::KEEP_INPUT_VALUE:
                     FillPixel( out, tensorLength, outTensorStride, clamp_cast< TPI >( *in ) );
                     break;
                  //case LookupTable::OutOfBoundsMode::CLAMP_TO_RANGE:
                  default:
                     TPI const* tval = values + ( *in < index_.front() ? 0 : static_cast< dip::sint >( maxIndex )) * valuesStride;
                     CopyPixel( tval, out, tensorLength, valuesTensorStride, outTensorStride );
                     break;
               }
            } else {
               auto upper = std::upper_bound( index_.begin(), index_.end(), *in ); // index_[upper] > *in
               dip::uint index = static_cast< dip::uint >( std::distance( index_.begin(), upper )) - 1; // index_[index] <= *in
               // Because *in >= index_.front(), we can always subtract 1 from the distance.
               switch( interpolation_ ) {
                  case LookupTable::InterpolationMode::LINEAR:
                     if( *in == index_[ index ] ) {
                        CopyPixel( values + static_cast< dip::sint >( index ) * valuesStride, out, tensorLength,
                                   valuesTensorStride, outTensorStride );
                     } else {
                        dfloat fraction = ( *in - index_[ index ] ) / ( index_[ index + 1 ] - index_[ index ] );
                        CopyPixelWithInterpolation( values + static_cast< dip::sint >( index ) * valuesStride,
                                                    out, tensorLength, valuesTensorStride, outTensorStride,
                                                    fraction, valuesStride );
                     }
                     break;
                  case LookupTable::InterpolationMode::NEAREST_NEIGHBOR:
                     if(( *in != index_[ index ] ) && (( *in - index_[ index ] ) > ( index_[ index + 1 ] - *in ))) {
                        // (the `!=` test above is to avoid out-of-bounds indexing with `index+1`)
                        ++index;
                     }
                     // fall through on purpose!
                  case LookupTable::InterpolationMode::ZERO_ORDER_HOLD:
                     CopyPixel( values + static_cast< dip::sint >( index ) * valuesStride, out, tensorLength,
                                valuesTensorStride, outTensorStride );
                     break;
               }
            }
            in += inStride;
            out += outStride;
         }
      }
      dip__IndexedLUT_Float( Image const& values, FloatArray const& index, LookupTable::OutOfBoundsMode outOfBoundsMode,
                             dfloat outOfBoundsValue, LookupTable::InterpolationMode interpolation )
            : values_( values ), index_( index ), outOfBoundsMode_( outOfBoundsMode ),
              outOfBoundsValue_( clamp_cast< TPI >( outOfBoundsValue )), interpolation_( interpolation ) {}
   private:
      Image const& values_;
      FloatArray const& index_;
      LookupTable::OutOfBoundsMode outOfBoundsMode_;
      TPI outOfBoundsValue_;
      LookupTable::InterpolationMode interpolation_;
};

}

void LookupTable::Apply( Image const& in, Image& out, InterpolationMode interpolation ) const {
   DIP_THROW_IF( !in.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( !in.IsScalar(), E::IMAGE_NOT_SCALAR );
   DIP_THROW_IF( !in.DataType().IsReal(), E::DATA_TYPE_NOT_SUPPORTED );
   ImageRefArray outar{ out };
   std::unique_ptr< Framework::ScanLineFilter >scanLineFilter;
   dip::DataType inBufType;
   if( HasIndex() ) {
      DIP_OVL_NEW_REAL( scanLineFilter, dip__IndexedLUT_Float, ( values_, index_, outOfBoundsMode_, outOfBoundsValue_, interpolation ), values_.DataType() );
      inBufType = DT_DFLOAT;
   } else {
      if( in.DataType().IsUnsigned() ) {
         DIP_OVL_NEW_REAL( scanLineFilter, dip__DirectLUT_Integer, ( values_, outOfBoundsMode_, outOfBoundsValue_ ), values_.DataType() );
         inBufType = DT_UINT32;
      } else {
         DIP_OVL_NEW_REAL( scanLineFilter, dip__DirectLUT_Float, ( values_, outOfBoundsMode_, outOfBoundsValue_, interpolation ), values_.DataType() );
         inBufType = DT_DFLOAT;
      }
   }
   DIP_START_STACK_TRACE
      Scan( { in }, outar, { inBufType }, { values_.DataType() }, { values_.DataType() }, { values_.TensorElements() }, *scanLineFilter );
   DIP_END_STACK_TRACE
   out.ReshapeTensor( values_.Tensor() );
   out.SetColorSpace( values_.ColorSpace() );
}

Image::Pixel LookupTable::Apply( dfloat value, InterpolationMode interpolation ) const {
   std::unique_ptr< Framework::ScanLineFilter >scanLineFilter;
   if( HasIndex() ) {
      DIP_OVL_NEW_REAL( scanLineFilter, dip__IndexedLUT_Float, ( values_, index_, outOfBoundsMode_, outOfBoundsValue_, interpolation ), values_.DataType() );
   } else {
      DIP_OVL_NEW_REAL( scanLineFilter, dip__DirectLUT_Float, ( values_, outOfBoundsMode_, outOfBoundsValue_, interpolation ), values_.DataType() );
   }
   Image::Pixel out( values_.DataType(), values_.TensorElements() );
   out.ReshapeTensor( values_.Tensor() );
   std::vector< Framework::ScanBuffer > inBuffers( 1 );
   inBuffers[ 0 ] = { &value, 1, 1, 1 };
   std::vector< Framework::ScanBuffer > outBuffers( 1 );
   outBuffers[ 0 ] = { out.Origin(), 1, out.TensorStride(), out.TensorElements() };
   Framework::ScanLineFilterParameters params{ inBuffers, outBuffers, 1 /* bufferLength = 1 pixel */, 0, {}, false, 0 };
   scanLineFilter->Filter( params );
   return out;
}

} // namespace dip


#ifdef DIP__ENABLE_DOCTEST
#include "doctest.h"
#include "diplib/iterators.h"

DOCTEST_TEST_CASE( "[DIPlib] testing dip::LookupTable" ) {
   // Case 1: uint image
   dip::Image lutIm1( { 10 }, 3, dip::DT_UINT8 );
   dip::ImageIterator< dip::uint8 > lutIt1( lutIm1 );
   dip::uint8 v = 10;
   do {
      *lutIt1 = v;
      ++v;
   } while( ++lutIt1 );
   dip::LookupTable lut1( lutIm1 );
   lut1.SetOutOfBoundsValue( 255 );
   DOCTEST_CHECK( lut1.HasIndex() == false );
   DOCTEST_CHECK( lut1.DataType() == dip::DT_UINT8 );
   dip::Image img1( { 5, 3 }, 1, dip::DT_UINT16 );
   dip::ImageIterator< dip::uint16 > imgIt1( img1 );
   dip::uint16 ii = 0;
   do {
      *imgIt1 = ii;
      ++ii;
   } while( ++imgIt1 );
   dip::Image out1 = lut1.Apply( img1 );
   DOCTEST_REQUIRE( out1.DataType() == dip::DT_UINT8 );
   DOCTEST_REQUIRE( out1.TensorElements() == 3 );
   DOCTEST_REQUIRE( out1.Sizes() == img1.Sizes() );
   dip::ImageIterator< dip::uint8 > outIt1( out1 );
   ii = 0;
   do {
      if( ii < 10 ) {
         DOCTEST_CHECK( *outIt1 == ii + 10 );
      } else {
         DOCTEST_CHECK( *outIt1 == 255 );
      }
      ++ii;
   } while( ++outIt1 );

   // Case 2: float image
   // TODO

   // Case 3: float image with index
   // TODO

}

#endif // DIP__ENABLE_DOCTEST
