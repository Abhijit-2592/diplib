/*
 * DIPimage 3.0
 * This MEX-file implements the `unif` function
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


#define DOCTEST_CONFIG_IMPLEMENT

#include "dip_matlab_interface.h"
#include "diplib/linear.h"

void mexFunction( int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[] ) {
   try {

      DML_MIN_ARGS( 1 );
      DML_MAX_ARGS( 4 );

      dml::MatlabInterface mi;
      dip::Image const in = dml::GetImage( prhs[ 0 ] );
      dip::Image out = mi.NewImage();

      if( nrhs > 1 ) {
         if( mxIsNumeric( prhs[ 1 ] ) && ( mxGetNumberOfElements( prhs[ 1 ] ) <= in.Dimensionality() )) {
            // This looks like a sizes vector
            auto filterParam = dml::GetFloatArray( prhs[ 1 ] );
            if( nrhs > 2 ) {
               auto filterShape = dml::GetString( prhs[ 2 ] );
               dip::StringArray bc;
               if( nrhs > 3 ) {
                  bc = dml::GetStringArray( prhs[ 3 ] );
               }
               dip::Uniform( in, out, filterParam, filterShape, bc );
            } else {
               dip::Uniform( in, out, filterParam );
            }
         } else {
            // Assume it's an image?
            DML_MAX_ARGS( 3 );
            dip::Image const neighborhood = dml::GetImage( prhs[ 1 ] );
            dip::StringArray bc;
            if( nrhs > 2 ) {
               bc = dml::GetStringArray( prhs[ 2 ] );
            }
            dip::Uniform( in, neighborhood, out, bc );
         }
      } else {
         dip::Uniform( in, out );
      }

      plhs[ 0 ] = mi.GetArray( out );

   } catch( const dip::Error& e ) {
      mexErrMsgTxt( e.what() );
   }
}
