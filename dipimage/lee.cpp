/*
 * DIPimage 3.0
 * This MEX-file implements the `lee` function
 *
 * (c)2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 * Based on original DIPimage code: (c)1999-2014, Delft University of Technology.
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

#undef DIP__ENABLE_DOCTEST
#include "dip_matlab_interface.h"
#include "diplib/morphology.h"

void mexFunction( int /*nlhs*/, mxArray* plhs[], int nrhs, const mxArray* prhs[] ) {
   try {

      DML_MIN_ARGS( 1 );
      DML_MAX_ARGS( 6 );

      dml::MatlabInterface mi;
      dip::Image const in = dml::GetImage( prhs[ 0 ] );
      dip::Image out = mi.NewImage();

      if( nrhs > 1 ) {
         if( mxIsNumeric( prhs[ 1 ] ) && ( mxGetNumberOfElements( prhs[ 1 ] ) <= in.Dimensionality() )) {
            // This looks like a sizes vector
            auto filterParam = dml::GetFloatArray( prhs[ 1 ] );
            if( nrhs > 2 ) {
               auto filterShape = dml::GetString( prhs[ 2 ] );
               dip::String edgeType = "texture";
               dip::String sign = "unsigned";
               dip::StringArray bc;
               if( nrhs > 3 ) {
                  edgeType = dml::GetString( prhs[ 3 ] );
                  if( nrhs > 4 ) {
                     sign = dml::GetString( prhs[ 4 ] );
                     if( nrhs > 5 ) {
                        bc = dml::GetStringArray( prhs[ 5 ] );
                     }
                  }
               }
               dip::Lee( in, out, { filterParam, filterShape }, edgeType, sign, bc );
            } else {
               dip::Lee( in, out, filterParam );
            }
         } else {
            // Assume it's an image?
            DML_MAX_ARGS( 5 );
            dip::Image const se = dml::GetImage( prhs[ 1 ] );
            dip::String edgeType = "texture";
            dip::String sign = "unsigned";
            dip::StringArray bc;
            if( nrhs > 2 ) {
               edgeType = dml::GetString( prhs[ 2 ] );
               if( nrhs > 3 ) {
                  sign = dml::GetString( prhs[ 3 ] );
                  if( nrhs > 4 ) {
                     bc = dml::GetStringArray( prhs[ 4 ] );
                  }
               }
            }
            dip::Lee( in, out, se, edgeType, sign, bc );
         }
      } else {
         dip::Lee( in, out );
      }

      plhs[ 0 ] = mi.GetArray( out );

   } catch( const dip::Error& e ) {
      mexErrMsgTxt( e.what() );
   }
}
