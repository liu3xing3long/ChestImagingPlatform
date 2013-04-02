/**
 *
 *  $Date: 2012-09-17 20:45:23 -0400 (Mon, 17 Sep 2012) $
 *  $Revision: 270 $
 *  $Author: jross $
 *
 */

#include "cipAirwayParticleConnectedComponentFilter.h"
#include "vtkPointData.h"
#include "vtkFloatArray.h"
#include "vtkSmartPointer.h"
#include <cfloat>
#include "itkImageFileWriter.h"
#include "vnl/algo/vnl_symmetric_eigensystem.h"


cipAirwayParticleConnectedComponentFilter::cipAirwayParticleConnectedComponentFilter()
{
  this->ScaleRatioThreshold       = 1.0;
  this->ParticleDistanceThreshold = this->InterParticleSpacing;
}


vtkPolyData* cipAirwayParticleConnectedComponentFilter::GetOutput()
{
  return this->OutputPolyData;
}

void cipAirwayParticleConnectedComponentFilter::SetScaleRatioThreshold( double threshold )
{
  this->ScaleRatioThreshold = threshold;
}


double cipAirwayParticleConnectedComponentFilter::GetScaleRatioThreshold()
{
  return this->ScaleRatioThreshold;
}


bool cipAirwayParticleConnectedComponentFilter::EvaluateParticleConnectedness( unsigned int particleIndex1, unsigned int particleIndex2 )
{
  //
  // Evaluate whether or not the two particls are sufficiently of the
  // same scale
  //
  double scale1 = this->InternalInputPolyData->GetFieldData()->GetArray( "scale" )->GetTuple( particleIndex1 )[0];
  double scale2 = this->InternalInputPolyData->GetFieldData()->GetArray( "scale" )->GetTuple( particleIndex2 )[0];

  double maxScale;  (scale1>scale2) ? (maxScale = scale1) : (maxScale = scale2);

  if ( vcl_abs(scale1 - scale2)/maxScale > this->ScaleRatioThreshold )
    {
      return false;
    }

  //
  // Determine the vector connecting the two particles
  //
  double point1[3];
    point1[0] = this->InternalInputPolyData->GetPoint( particleIndex1 )[0];
    point1[1] = this->InternalInputPolyData->GetPoint( particleIndex1 )[1];
    point1[2] = this->InternalInputPolyData->GetPoint( particleIndex1 )[2];

  double point2[3];
    point2[0] = this->InternalInputPolyData->GetPoint( particleIndex2 )[0];
    point2[1] = this->InternalInputPolyData->GetPoint( particleIndex2 )[1];
    point2[2] = this->InternalInputPolyData->GetPoint( particleIndex2 )[2];

  double connectingVec[3];
    connectingVec[0] = point1[0] - point2[0];
    connectingVec[1] = point1[1] - point2[1];
    connectingVec[2] = point1[2] - point2[2];

  double particle1Hevec2[3];
    particle1Hevec2[0] = this->InternalInputPolyData->GetFieldData()->GetArray( "hevec2" )->GetTuple( particleIndex1 )[0];
    particle1Hevec2[1] = this->InternalInputPolyData->GetFieldData()->GetArray( "hevec2" )->GetTuple( particleIndex1 )[1];
    particle1Hevec2[2] = this->InternalInputPolyData->GetFieldData()->GetArray( "hevec2" )->GetTuple( particleIndex1 )[2];

  double particle2Hevec2[3];
    particle2Hevec2[0] = this->InternalInputPolyData->GetFieldData()->GetArray( "hevec2" )->GetTuple( particleIndex2 )[0];
    particle2Hevec2[1] = this->InternalInputPolyData->GetFieldData()->GetArray( "hevec2" )->GetTuple( particleIndex2 )[1];
    particle2Hevec2[2] = this->InternalInputPolyData->GetFieldData()->GetArray( "hevec2" )->GetTuple( particleIndex2 )[2];

  if ( this->GetVectorMagnitude( connectingVec ) > this->ParticleDistanceThreshold )
    {
    return false;
    }

  double theta1 = this->GetAngleBetweenVectors( particle1Hevec2, connectingVec, true );
  double theta2 = this->GetAngleBetweenVectors( particle2Hevec2, connectingVec, true );

  if ( theta1 > this->ParticleAngleThreshold || theta2 > this->ParticleAngleThreshold )
    {
    return false;
    } 

  return true;
}


