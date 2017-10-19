/*
 * DIPlib 3.0
 * This file contains binary propagation.
 *
 * (c)2017, Erik Schuitema.
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
#include "diplib/binary.h"
#include "diplib/neighborlist.h"
#include "diplib/iterators.h"
#include "binary_support.h"

namespace dip {

void BinaryPropagation(
   Image const& c_inSeed,
   Image const& c_inMask,
   Image& out,
   dip::sint connectivity,
   dip::uint iterations,
   String const& s_edgeCondition
) {
   // Verify that the mask image is forged, scalar and binary
   DIP_THROW_IF( !c_inMask.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( !c_inMask.DataType().IsBinary(), E::IMAGE_NOT_BINARY );
   DIP_THROW_IF( !c_inMask.IsScalar(), E::IMAGE_NOT_SCALAR );

   // If the seed image is not raw, check that it is scalar and binary and of the same size as inMask
   if( c_inSeed.IsForged() ) {
      DIP_THROW_IF( !c_inSeed.DataType().IsBinary(), E::IMAGE_NOT_BINARY );
      DIP_THROW_IF( !c_inSeed.IsScalar(), E::IMAGE_NOT_SCALAR );
      DIP_THROW_IF( c_inMask.Sizes() != c_inSeed.Sizes(), E::SIZES_DONT_MATCH );
   }

   // Check connectivity validity
   dip::uint nDims = c_inMask.Dimensionality();
   DIP_THROW_IF( connectivity > static_cast< dip::sint >( nDims ), E::PARAMETER_OUT_OF_RANGE );

   // Edge condition: true means object, false means background
   bool outsideImageIsObject = BooleanFromString( s_edgeCondition, S::OBJECT, S::BACKGROUND );

   // Make out equal to inMask
   Image inMask = c_inMask; // temporary copy of input image headers, so we can strip/reforge out
   Image inSeed = c_inSeed;
   if( out.Aliases( inMask )) { // make sure we don't overwrite the mask image
      std::cout << "stripping out\n";
      out.Strip();
   }
   out.ReForge( inMask.Sizes(), 1, DT_BIN );
   // Copy inSeed plane to output plane if it is non-empty, otherwise clear it
   // Operation takes place directly in the output plane.
   if( inSeed.IsForged() ) {
      out.Copy( inSeed );     // if &c_inSeed == &out, we get here too. Copy won't do anything.
   } else {
      out = 0; // No seed data means: initialize all samples with false
   }
   if( inSeed.HasPixelSize() ) {
      out.SetPixelSize( inSeed.PixelSize() );
   } else {
      out.SetPixelSize( inMask.PixelSize() );
   }

   // Zero iterations means: continue until propagation is done
   if( iterations == 0 )
      iterations = std::numeric_limits< dip::uint >::max();

   // Negative connectivity means: alternate between two different connectivities

   // Create arrays with offsets to neighbours for even iterations
   dip::uint iterConnectivity0 = GetAbsBinaryConnectivity( nDims, connectivity, 0 );
   NeighborList neighborList0( { Metric::TypeCode::CONNECTED, iterConnectivity0 }, nDims );
   IntegerArray neighborOffsetsOut0 = neighborList0.ComputeOffsets( out.Strides() );

   // Create arrays with offsets to neighbours for odd iterations
   dip::uint iterConnectivity1 = GetAbsBinaryConnectivity( nDims, connectivity, 1 );
   NeighborList neighborList1( { Metric::TypeCode::CONNECTED, iterConnectivity1 }, nDims );
   IntegerArray neighborOffsetsOut1 = neighborList1.ComputeOffsets( out.Strides() );

   // Data mask: the pixel data is in the first 'plane'
   uint8 dataBitmask = 1;

   // Define the mask bitmask
   const int maskPlane = 3;
   uint8 maskBitmask = uint8( 1 << maskPlane );

   // Use border mask to mark pixels of the image border
   const int borderPlane = 2;
   uint8 borderBitmask = uint8( 1 << borderPlane );
   ApplyBinaryBorderMask( out, borderBitmask );

   // Define the seed bitmask (equals data bitmask)
   const int seedPlane = 0;
   uint8 seedBitmask = uint8( 1 << seedPlane );

   // Add mask plane to out image
   JointImageIterator< dip::bin, dip::bin > itInMaskOut( { inMask, out } );
   do {
      if ( itInMaskOut.In() )
         static_cast< uint8& >( itInMaskOut.Out() ) |= maskBitmask;
   } while( ++itInMaskOut );

   // The edge pixel queue
   dip::queue< dip::bin* > edgePixels;

   // Initialize the queue by finding all edge pixels of type 'background'
   bool findObjectPixels = false;
   FindBinaryEdgePixels( out, findObjectPixels, neighborList0, neighborOffsetsOut0, dataBitmask, borderBitmask, outsideImageIsObject, &edgePixels );

   const uint8 maskOrSeedBitmask = seedBitmask | maskBitmask;

   // First iteration: process all elements in the queue a first time
   if( iterations > 0 ) {
      // Process all elements currently in the queue
      dip::sint count = static_cast< dip::sint >( edgePixels.size() );

      while( --count >= 0 ) {
         dip::bin* pPixel = edgePixels.front();
         uint8& pixelByte = static_cast< uint8& >( *pPixel );
         if(( pixelByte & maskOrSeedBitmask ) == maskBitmask ) {
            pixelByte |= seedBitmask;
            edgePixels.push_back( pPixel );
         }
         // Remove the front pixel from the queue
         edgePixels.pop_front();
      }
   }

   // Create a coordinates computer for bounds checking of border pixels
   const CoordinatesComputer coordsComputer = out.OffsetToCoordinatesComputer();

   // Second and further iterations. Loop also stops if the queue is empty
   for( dip::uint iDilIter = 1; iDilIter < iterations; ++iDilIter ) {
      // Obtain neighbor list and offsets for this iteration
      NeighborList const& neighborList = iDilIter & 1 ? neighborList1 : neighborList0;
      IntegerArray const& neighborOffsetsOut = iDilIter & 1 ? neighborOffsetsOut1 : neighborOffsetsOut0;

      // Process all elements currently in the queue
      dip::sint count = static_cast< dip::sint >( edgePixels.size() );

      while( --count >= 0 ) {
         // Get front pixel from the queue
         dip::bin* pPixel = edgePixels.front();
         uint8& pixelByte = static_cast< uint8& >( *pPixel );
         bool isBorderPixel = pixelByte & borderBitmask;

         // Propagate to all neighbours which are not yet processed
         dip::IntegerArray::const_iterator itNeighborOffset = neighborOffsetsOut.begin();
         for( NeighborList::Iterator itNeighbor = neighborList.begin(); itNeighbor != neighborList.end(); ++itNeighbor, ++itNeighborOffset ) {
            if( !isBorderPixel || itNeighbor.IsInImage( coordsComputer( pPixel - static_cast< dip::bin* >( out.Origin() )), out.Sizes() ) ) { // IsInImage() is not evaluated for non-border pixels
               dip::bin* pNeighbor = pPixel + *itNeighborOffset;
               uint8& neighborByte = static_cast< uint8& >( *pNeighbor );
               //bool neighborIsObject = neighborByte & dataBitmask;
               // If the neighbor has the mask-bit (means: propagation allowed)
               // but not the seed-bit (means: not yet processed),
               // process this neighbor.
               if(( neighborByte & maskOrSeedBitmask ) == maskBitmask ) {
                  // Propagate to the neighbor pixel
                  neighborByte |= seedBitmask;
                  // Add neighbor to the queue
                  edgePixels.push_back( pNeighbor );
               }
            }
         }

         // Remove the front pixel from the queue
         edgePixels.pop_front();
      }

      // Iteration is done if the queue is empty
      if( edgePixels.empty() )
         break;
   }

   // Final step: pixels have their data bit set iff they have both seed-bit and mask-bit
   // The result is stored in a way that resets all bits except bit 0
   // This means that the border mask is also removed
   ImageIterator< dip::bin > itOut( out );
   do {
      uint8& pixelByte = static_cast< uint8& >( *itOut );
      pixelByte = static_cast< uint8 >(( pixelByte & maskOrSeedBitmask ) == maskOrSeedBitmask );
   } while( ++itOut );
}

void EdgeObjectsRemove(
      Image const& in,
      Image& out,
      dip::uint connectivity
) {
   // Propagate with empty seed mask, iteration until done and treating outside the image as object
   BinaryPropagation( Image(), in, out, static_cast< dip::sint >( connectivity ), 0, S::OBJECT );

   // The out-image now contains the edge objects
   // Remove them by toggling these bits in the in-image and writing the result in out
   out ^= in;
}

} // namespace dip
