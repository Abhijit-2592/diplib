/*
 * DIPlib 3.0
 * This file contains declarations for functions that work with labeled images.
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

#ifndef DIP_REGIONS_H
#define DIP_REGIONS_H

#include "diplib.h"


/// \file
/// \brief Declares image processing functions that work with labeled images.


namespace dip {


/// \defgroup regions Connected component analysis
/// \brief Label connected component and process labeled images.
///
/// Labeled images are of any unsigned integer type.
/// \{

// TODO: functions to port:
/*
   dip_GrowRegions (dip_regions.h)
   dip_GrowRegionsWeighted (dip_regions.h)
*/

/// \brief Labels the connected components in a binary image
///
/// The output is an unsigned integer image. Each object (respecting the connectivity,
/// see \ref connectivity) in the input image receives a unique number. This number ranges
/// from 1 to the number of objects in the image. The pixels in the output image corresponding
/// to a given object are set to this number (label). The remaining pixels in the output image
/// are set to 0.
///
/// The `minSize` and `maxSize` set limits on the size of the objects: Objects smaller than `minSize`
/// or larger than `maxSize` do not receive a label and the corresponding pixels in the output
/// image are set to zero. Setting either to zero disables the corresponding check. Setting both
/// to zero causes all objects to be labeled, irrespective of size.
///
/// The boundary conditions are generally ignored (labeling stops at the boundary). The exception
/// is `"periodic"`, which is the only one that makes sense for this algorithm.
DIP_EXPORT dip::uint Label(
      Image const& binary,
      Image& out,
      dip::uint connectivity,
      dip::uint minSize = 0,
      dip::uint maxSize = 0,
      StringArray const& boundaryCondition = {}
);

inline Image Label(
      Image const& binary,
      dip::uint connectivity,
      dip::uint minSize = 0,
      dip::uint maxSize = 0,
      StringArray const& boundaryCondition = {}
) {
   Image out;
   Label( binary, out, connectivity, minSize, maxSize, boundaryCondition );
   return out;
}

/// \brief Gets a list of object labels in the labeled image. A labeled image must be of an unsigned type.
DIP_EXPORT UnsignedArray GetObjectLabels(
      Image const& label,
      Image const& mask,
      bool nullIsObject
);

/// \brief Removes small objects from a labeled or binary image.
///
/// If `in` is an unsigned integer image, it is assumed to be a labeled image. The size of the objects
/// are measured using `dip::MeasurementTool`, and the labels for the objects with fewer than `threshold`
/// pixels are removed. The `connectivity` parameter is ignored.
///
/// If `in` is a binary image, `dip::Label` is called with `minSize` set to `threshold`, and the result
/// is binarized again. `connectivity` is passed to the labeling function.
///
/// The operation on a binary image is equivalent to an area opening with parameter `threshold`
/// (see `dip::AreaOpening`). The same is not true for the labeled image case, if labeled regions
/// are touching.
DIP_EXPORT void SmallObjectsRemove(
      Image const& in,
      Image& out,
      dip::uint threshold,
      dip::uint connectivity = 1
);
inline Image SmallObjectsRemove(
      Image const& in,
      dip::uint threshold,
      dip::uint connectivity = 1
) {
   Image out;
   SmallObjectsRemove( in, out, threshold, connectivity );
   return out;
}

/// \}

} // namespace dip

#endif // DIP_REGIONS_H
