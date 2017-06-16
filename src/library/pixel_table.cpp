/*
 * DIPlib 3.0
 * This file contains definitions of functions declared in pixel_table.h.
 *
 * (c)2016-2017, Cris Luengo.
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

#include "diplib/pixel_table.h"
#include "diplib/iterators.h"
#include "diplib/overload.h"

namespace dip {

// Construct a pixel table with offsets from a pixel table
PixelTableOffsets::PixelTableOffsets(
      PixelTable const& pt,
      Image const& image
) {
   sizes_ = pt.Sizes();
   origin_ = pt.Origin();
   nPixels_ = pt.NumberOfPixels();
   procDim_ = pt.ProcessingDimension();
   stride_ = image.Stride( procDim_ );
   auto const& inRuns = pt.Runs();
   runs_.resize( inRuns.size() );
   for( dip::uint ii = 0; ii < runs_.size(); ++ii ) {
      runs_[ ii ].offset = image.Offset( inRuns[ ii ].coordinates );
      runs_[ ii ].length = inRuns[ ii ].length;
   }
   weights_ = pt.Weights();
}

// Construct a pixel table from a given shape and size
PixelTable::PixelTable(
      String const& shape,
      FloatArray size, // by copy
      dip::uint procDim
) {
   dip::uint nDims = size.size();
   DIP_THROW_IF( nDims < 1, E::DIMENSIONALITY_NOT_SUPPORTED );
   DIP_THROW_IF( procDim >= nDims, E::PARAMETER_OUT_OF_RANGE );
   procDim_ = procDim;

   if( shape == "line" ) {

      //
      // Construct a pixel table from a Bresenham line
      //
      // Ideally runs go along the longest dimension of the line.
      // Worst case is when they go along the shortest, then all runs have length 1.

      // Initialize sizes and origin, modify `size` to be the total step to take from start to end of line.
      sizes_.resize( nDims, 0 );
      origin_.resize( nDims, 0 );
      for( dip::uint ii = 0; ii < nDims; ++ii ) {
         if( size[ ii ] < 0 ) {
            size[ ii ] = std::min( std::round( size[ ii ] ) + 1.0, 0.0 );
            sizes_[ ii ] = static_cast< dip::uint >( -size[ ii ] ) + 1;
         } else {
            size[ ii ] = std::max( std::round( size[ ii ] ) - 1.0, 0.0 );
            sizes_[ ii ] = static_cast< dip::uint >( size[ ii ] ) + 1;
         }
         origin_[ ii ] = -static_cast< dip::sint >( sizes_[ ii ] ) / 2;
      }

      // Find the number of steps from start to end of line
      dip::uint maxSize = *std::max_element( sizes_.begin(), sizes_.end() ) - 1;
      if( maxSize >= 1 ) {
         // Compute step size along each dimension, and find the start point
         FloatArray stepSize( nDims );
         FloatArray pos( nDims );
         for( dip::uint ii = 0; ii < nDims; ++ii ) {
            stepSize[ ii ] = size[ ii ] / static_cast< dfloat >( maxSize );
            pos[ ii ] = static_cast< dfloat >( origin_[ ii ] ) +
                        ( size[ ii ] < 0 ? static_cast< dfloat >( sizes_[ ii ] - 1 ) : 0.0 ) + 1.0e-8;
                        // we add a very small value here, to force rounding to happen in the right direction.
         }
         // We need the line to go through the origin, which can be done by setting `origin_` properly,
         // but predicting what it needs to be is a little complex, depending on even/odd lengths in combinations
         // of dimensions. For now we just record the coordinates when we reach the origin along the processing
         // dimension, and shift the origin later on.
         IntegerArray shift;
         // Walk the line, extract runs
         IntegerArray coords( nDims );
         for( dip::uint ii = 0; ii < nDims; ++ii ) {
            coords[ ii ] = static_cast< dip::sint >( std::round( pos[ ii ] ));
         }
         dip::uint runLength = 1;
         for( dip::uint step = 0; step < maxSize; ++step ) {
            pos += stepSize;
            // Are all integer coordinates the same except for the one along procDim_?
            bool same = true;
            for( dip::uint ii = 0; ii < nDims; ++ii ) {
               if(( ii != procDim_ ) && ( static_cast< dip::sint >( std::round( pos[ ii ] )) != coords[ ii ] )) {
                  same = false;
                  break;
               }
            }
            if( !same ) {
               // Save run
               runs_.emplace_back( coords, runLength );
               nPixels_ += runLength;
               // Start new run
               for( dip::uint ii = 0; ii < nDims; ++ii ) {
                  coords[ ii ] = static_cast< dip::sint >( std::round( pos[ ii ] ));
               }
               runLength = 1;
            } else {
               ++runLength;
            }
            // Are we at the origin?
            if( std::round( pos[ procDim_ ] ) == 0.0 ) {
               bool needShift = false;
               shift.resize( nDims );
               for( dip::uint ii = 0; ii < nDims; ++ii ) {
                  shift[ ii ] = 0;
                  if(( ii != procDim_ ) && ( coords[ ii ] != 0 )) {
                     shift[ ii ] = coords[ ii ];
                     needShift = true;
                  }
               }
               if( !needShift ) {
                  shift.clear();
               }
            }
         }
         runs_.emplace_back( coords, runLength );
         nPixels_ += runLength;
         if( !shift.empty() ) {
            ShiftOrigin( shift );
         }
      } else {
         // A single point!
         runs_.emplace_back( origin_, 1 );
         nPixels_ = 1;
      }

   } else {

      //
      // Construct a pixel table from a unit circle in different metrics
      //

      // Make sure filter is at least 1px in each dimension
      for( auto& s : size ) {
         s = std::max( 1.0, s );
      }

      if( shape == "rectangular" ) {
         // A rectangle has all runs of the same length, easy!

         // Initialize sizes and origin
         sizes_.resize( nDims, 0 );
         origin_.resize( nDims, 0 );
         for( dip::uint ii = 0; ii < nDims; ++ii ) {
            sizes_[ ii ] = static_cast< dip::uint >( size[ ii ] );
            origin_[ ii ] = -static_cast< dip::sint >( sizes_[ ii ] ) / 2;
         }

         // Determine number of pixel table runs
         dip::uint nRuns = 1;
         for( dip::uint ii = 0; ii < nDims; ++ii ) {
            if( ii != procDim ) {
               nRuns *= sizes_[ ii ];
            }
         }
         runs_.reserve( nRuns );
         dip::uint length = sizes_[ procDim ];
         nPixels_ = nRuns * length;

         // Fill the pixel table runs
         IntegerArray cor = origin_;
         for( ;; ) {

            // Fill next pixel table run
            runs_.emplace_back( cor, length );

            // Some nD looping bookkeeping stuff
            dip::uint ii = 0;
            for( ; ii < nDims; ++ii ) {
               if( ii == procDim ) {
                  continue;
               }
               ++cor[ ii ];
               if( cor[ ii ] >= origin_[ ii ] + static_cast< dip::sint >( sizes_[ ii ] )) {
                  cor[ ii ] = origin_[ ii ];
                  continue;
               }
               break;
            }
            if( ii >= nDims ) {
               break;
            }
         }

      } else if( shape == "elliptic" ) {
         // A unit circle in Euclidean space, normalized by the sizes.

         // Initialize sizes and origin
         sizes_.resize( nDims, 0 );
         origin_.resize( nDims, 0 );
         for( dip::uint ii = 0; ii < nDims; ++ii ) {
            sizes_[ ii ] = ( static_cast< dip::uint >( size[ ii ] ) / 2 ) * 2 + 1;
            origin_[ ii ] = -static_cast< dip::sint >( sizes_[ ii ] ) / 2;
            size[ ii ] /= 2;
         }

         // Fill the pixel table runs
         dfloat sz = size[ procDim ];
         IntegerArray cor = origin_;
         for( ;; ) { // Loop over image lines

            // Find the square distance from the origin for the pixel in the middle of this line
            dfloat distance2 = 0.0;
            for( dip::uint ii = 0; ii < nDims; ++ii ) {
               if( ii != procDim ) {
                  dfloat tmp = static_cast< dfloat >( cor[ ii ] ) / size[ ii ];
                  distance2 += tmp * tmp;
               }
            }
            // If we're still within the radius, this line intersects the ellipsoid
            if( distance2 <= 1.0 ) {
               // Find the distance from the origin, along this line, that we can go and still stay within the ellipsoid
               dip::sint length = static_cast< dip::sint >( std::floor( sz * std::sqrt( 1.0 - distance2 )));
               // Determine and fill the run for this line
               IntegerArray coordinate = cor;
               coordinate[ procDim ] = -length;
               dip::uint len = static_cast< dip::uint >( 2 * length + 1 );
               runs_.emplace_back( coordinate, len );
               nPixels_ += len;
            }

            // Some nD looping bookkeeping stuff
            dip::uint ii = 0;
            for( ; ii < nDims; ++ii ) {
               if( ii == procDim ) {
                  continue;
               }
               ++cor[ ii ];
               if( cor[ ii ] >= origin_[ ii ] + static_cast< dip::sint >( sizes_[ ii ] )) {
                  cor[ ii ] = origin_[ ii ];
                  continue;
               }
               break;
            }
            if( ii >= nDims ) {
               break;
            }
         }

      } else if( shape == "diamond" ) {
         // Same as "elliptic" but with L1 norm.

         // Initialize sizes and origin
         sizes_.resize( nDims, 0 );
         origin_.resize( nDims, 0 );
         for( dip::uint ii = 0; ii < nDims; ++ii ) {
            sizes_[ ii ] = ( static_cast< dip::uint >( size[ ii ] ) / 2 ) * 2 + 1;
            origin_[ ii ] = -static_cast< dip::sint >( sizes_[ ii ] ) / 2;
            size[ ii ] /= 2;
         }

         // Fill the pixel table runs
         dfloat sz = size[ procDim ];
         IntegerArray cor = origin_;
         for( ;; ) { // Loop over image lines

            // Find the L1 distance from the origin for the pixel in the middle of this line
            dfloat distance = 0.0;
            for( dip::uint ii = 0; ii < nDims; ++ii ) {
               if( ii != procDim ) {
                  distance += static_cast< dfloat >( std::abs( cor[ ii ] )) / size[ ii ];
               }
            }
            // If we're still within the radius, this line intersects the diamond-oid
            if( distance <= 1.0 ) {
               // Find the distance from the origin, along this line, that we can go and still stay within the ellipsoid
               dip::sint length = static_cast< dip::sint >( std::floor( sz * ( 1.0 - distance )));
               // Determine and fill the run for this line
               IntegerArray coordinate = cor;
               coordinate[ procDim ] = -length;
               dip::uint len = static_cast< dip::uint >( 2 * length + 1 );
               runs_.emplace_back( coordinate, len );
               nPixels_ += len;
            }

            // some nD looping bookkeeping stuff
            dip::uint ii = 0;
            for( ; ii < nDims; ++ii ) {
               if( ii == procDim ) {
                  continue;
               }
               ++cor[ ii ];
               if( cor[ ii ] >= origin_[ ii ] + static_cast< dip::sint >( sizes_[ ii ] )) {
                  cor[ ii ] = origin_[ ii ];
                  continue;
               }
               break;
            }
            if( ii >= nDims ) {
               break;
            }
         }

      } else {
         DIP_THROW( "Neighborhood shape name not recognized: " + shape );
      }
   }
}

// Construct a pixel table from a binary image
PixelTable::PixelTable(
      Image const& mask,
      IntegerArray const& origin,
      dip::uint procDim
) {
   DIP_THROW_IF( !mask.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( mask.TensorElements() != 1, E::MASK_NOT_SCALAR );
   DIP_THROW_IF( mask.DataType() != DT_BIN, E::MASK_NOT_BINARY );
   dip::uint nDims = mask.Dimensionality();
   DIP_THROW_IF( nDims < 1, E::DIMENSIONALITY_NOT_SUPPORTED );
   DIP_THROW_IF( procDim >= nDims, E::PARAMETER_OUT_OF_RANGE );
   procDim_ = procDim;
   sizes_ = mask.Sizes();
   if( origin.empty() ) {
      origin_.resize( nDims, 0 );
      for( dip::uint ii = 0; ii < nDims; ++ii ) {
         origin_[ ii ] = -static_cast< dip::sint >( sizes_[ ii ] ) / 2;
      }
   } else {
      DIP_THROW_IF( origin.size() != nDims, E::ARRAY_ILLEGAL_SIZE );
      origin_.resize( nDims, 0 );
      for( dip::uint ii = 0; ii < nDims; ++ii ) {
         origin_[ ii ] = -origin[ ii ];
      }
   }
   ImageIterator< dip::bin > it( mask, procDim );
   do {
      IntegerArray position = origin_;
      position += it.Coordinates();
      dip::sint start = position[ procDim ];
      dip::uint length = 0;
      auto data = it.GetLineIterator();
      do {
         if( *data ) {
            ++length;
         } else {
            if( length ) {
               position[ procDim ] = start + static_cast< dip::sint >( data.Coordinate() - length );
               runs_.emplace_back( position, length );
               nPixels_ += length;
            }
            length = 0;
         }
      } while( ++data );
      if( length ) {
         position[ procDim ] = start + static_cast< dip::sint >( data.Coordinate() - length );
         runs_.emplace_back( position, length );
         nPixels_ += length;
      }
   } while( ++it );
}

// Create a binary or grey-value image from a pixel table
void PixelTable::AsImage( Image& out ) const {
   if( HasWeights() ) {
      out.ReForge( sizes_, 1, DT_DFLOAT );
      out.Fill( 0.0 );
      dip::sint stride = out.Stride( procDim_ );
      auto wIt = weights_.begin();
      for( auto& run : runs_ ) {
         IntegerArray position = run.coordinates;
         position -= origin_;
         dfloat* data = static_cast< dfloat* >( out.Pointer( position ));
         for( dip::uint ii = 0; ii < run.length; ++ii ) {
            *data = *wIt;
            ++wIt;
            data += stride;
         }
      }
   } else {
      out.ReForge( sizes_, 1, DT_BIN );
      out.Fill( false );
      dip::sint stride = out.Stride( procDim_ );
      for( auto& run : runs_ ) {
         IntegerArray position = run.coordinates;
         position -= origin_;
         dip::bin* data = static_cast< dip::bin* >( out.Pointer( position ));
         for( dip::uint ii = 0; ii < run.length; ++ii ) {
            *data = true;
            data += stride;
         }
      }
   }
}


template< typename TPI >
void dip__AddWeights(
      Image const& image,
      dip::sint stride,
      std::vector< PixelTable::PixelRun > const& runs,
      std::vector< dfloat >& weights,
      IntegerArray const& origin
) {
   for( auto& run : runs ) {
      IntegerArray position = run.coordinates;
      position -= origin;
      TPI* data = static_cast< TPI* >( image.Pointer( position ));
      for( dip::uint ii = 0; ii < run.length; ++ii ) {
         weights.push_back( *data );
         data += stride;
      }
   }
}

// Add weights from an image
void PixelTable::AddWeights( Image const& image ) {
   DIP_THROW_IF( !image.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( image.TensorElements() != 1, E::IMAGE_NOT_SCALAR );
   DIP_THROW_IF( image.Sizes() != sizes_, E::SIZES_DONT_MATCH );
   DIP_THROW_IF( !image.DataType().IsReal(), E::DATA_TYPE_NOT_SUPPORTED );
   weights_.reserve( nPixels_ );
   dip::sint stride = image.Stride( procDim_ );
   DIP_OVL_CALL_REAL( dip__AddWeights, ( image, stride, runs_, weights_, origin_ ), image.DataType() );
   DIP_ASSERT( weights_.size() == nPixels_ );
}

// Add weights from distances
void PixelTable::AddDistanceToOriginAsWeights() {
   weights_.reserve( nPixels_ );
   for( auto& run : runs_ ) {
      IntegerArray position = run.coordinates;
      dfloat sum2 = position.norm_square();
      weights_.push_back( std::sqrt( sum2 ));
      if( run.length > 1 ) {
         dfloat d = static_cast< dfloat >( position[ procDim_ ] );
         for( dip::uint ii = 1; ii < run.length; ++ii ) {
            dfloat x = static_cast< dfloat >( ii );
            // new sum2 = sum2 - d^2 + (d+x)^2 = sum2 + x^2 - 2*x*d
            weights_.push_back( std::sqrt( sum2 + x * x + 2 * x * d ));
         }
      }
   }
}


} // namespace dip


#ifdef DIP__ENABLE_DOCTEST

DOCTEST_TEST_CASE("[DIPlib] testing the PixelTable class") {
   dip::PixelTable pt( "elliptic", dip::FloatArray{ 10.1, 12.7, 5.3 }, 1 );
   DOCTEST_REQUIRE( pt.Sizes().size() == 3 );
   DOCTEST_CHECK( pt.Sizes()[ 0 ] == 11 );
   DOCTEST_CHECK( pt.Sizes()[ 1 ] == 13 );
   DOCTEST_CHECK( pt.Sizes()[ 2 ] == 5 );
   DOCTEST_REQUIRE( pt.Origin().size() == 3 );
   DOCTEST_CHECK( pt.Origin()[ 0 ] == -5 );
   DOCTEST_CHECK( pt.Origin()[ 1 ] == -6 );
   DOCTEST_CHECK( pt.Origin()[ 2 ] == -2 );
   DOCTEST_CHECK( pt.Runs().size() == 43 );
   DOCTEST_CHECK( pt.NumberOfPixels() == 359 );
   DOCTEST_CHECK( pt.ProcessingDimension() == 1 );
   DOCTEST_CHECK_FALSE( pt.HasWeights() );

   dip::Image img = pt.AsImage(); // convert to image
   dip::PixelTable pt2( img, {}, 1 ); // convert back to pixel table, should be exactly the same table.
   DOCTEST_REQUIRE( pt2.Sizes().size() == 3 );
   DOCTEST_CHECK( pt2.Sizes()[ 0 ] == 11 );
   DOCTEST_CHECK( pt2.Sizes()[ 1 ] == 13 );
   DOCTEST_CHECK( pt2.Sizes()[ 2 ] == 5 );
   DOCTEST_REQUIRE( pt2.Origin().size() == 3 );
   DOCTEST_CHECK( pt2.Origin()[ 0 ] == -5 );
   DOCTEST_CHECK( pt2.Origin()[ 1 ] == -6 );
   DOCTEST_CHECK( pt2.Origin()[ 2 ] == -2 );
   DOCTEST_CHECK( pt2.Runs().size() == 43 );
   DOCTEST_CHECK( pt2.NumberOfPixels() == 359 );
   DOCTEST_CHECK( pt2.ProcessingDimension() == 1 );
   DOCTEST_CHECK_FALSE( pt2.HasWeights() );
   DOCTEST_CHECK( pt.Runs()[ 0 ].coordinates == pt2.Runs()[ 0 ].coordinates );

   dip::PixelTable pt3( "rectangular", dip::FloatArray{ 22.2, 33.3 }, 0 );
   DOCTEST_REQUIRE( pt3.Sizes().size() == 2 );
   DOCTEST_CHECK( pt3.Sizes()[ 0 ] == 22 );
   DOCTEST_CHECK( pt3.Sizes()[ 1 ] == 33 );
   DOCTEST_REQUIRE( pt3.Origin().size() == 2 );
   DOCTEST_CHECK( pt3.Origin()[ 0 ] == -11 );
   DOCTEST_CHECK( pt3.Origin()[ 1 ] == -16 );
   DOCTEST_CHECK( pt3.Runs().size() == 33 );
   DOCTEST_CHECK( pt3.NumberOfPixels() == 22*33 );
   DOCTEST_CHECK( pt3.ProcessingDimension() == 0 );
   DOCTEST_CHECK_FALSE( pt3.HasWeights() );

   dip::PixelTable pt4( "diamond", dip::FloatArray{ 10.1, 12.7, 5.3 }, 2 );
   DOCTEST_REQUIRE( pt4.Sizes().size() == 3 );
   DOCTEST_CHECK( pt4.Sizes()[ 0 ] == 11 );
   DOCTEST_CHECK( pt4.Sizes()[ 1 ] == 13 );
   DOCTEST_CHECK( pt4.Sizes()[ 2 ] == 5 );
   DOCTEST_REQUIRE( pt4.Origin().size() == 3 );
   DOCTEST_CHECK( pt4.Origin()[ 0 ] == -5 );
   DOCTEST_CHECK( pt4.Origin()[ 1 ] == -6 );
   DOCTEST_CHECK( pt4.Origin()[ 2 ] == -2 );
   DOCTEST_CHECK( pt4.Runs().size() == 67 );
   DOCTEST_CHECK( pt4.NumberOfPixels() == 127 );
   DOCTEST_CHECK( pt4.ProcessingDimension() == 2 );
   DOCTEST_CHECK_FALSE( pt4.HasWeights() );

   dip::PixelTable pt5( "line", dip::FloatArray{ 14.1, -4.2, 7.9 }, 0 );
   DOCTEST_REQUIRE( pt5.Sizes().size() == 3 );
   DOCTEST_CHECK( pt5.Sizes()[ 0 ] == 14 );
   DOCTEST_CHECK( pt5.Sizes()[ 1 ] == 4 );
   DOCTEST_CHECK( pt5.Sizes()[ 2 ] == 8 );
   DOCTEST_REQUIRE( pt5.Origin().size() == 3 );
   DOCTEST_CHECK( pt5.Origin()[ 0 ] == -7 );
   DOCTEST_CHECK( pt5.Origin()[ 1 ] == -1 );
   DOCTEST_CHECK( pt5.Origin()[ 2 ] == -4 );
   DOCTEST_CHECK( pt5.Runs().size() == 8 );
   DOCTEST_CHECK( pt5.NumberOfPixels() == 14 );
   DOCTEST_CHECK( pt5.ProcessingDimension() == 0 );
   DOCTEST_CHECK_FALSE( pt5.HasWeights() );
}

#endif // DIP__ENABLE_DOCTEST
