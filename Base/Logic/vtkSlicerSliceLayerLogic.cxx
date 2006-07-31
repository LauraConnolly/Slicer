/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See Doc/copyright/copyright.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkSlicerSliceLayerLogic.cxx,v $
  Date:      $Date: 2006/01/06 17:56:48 $
  Version:   $Revision: 1.58 $

=========================================================================auto=*/

#include "vtkObjectFactory.h"
#include "vtkCallbackCommand.h"

#include "vtkSlicerSliceLayerLogic.h"

#include "vtkMRMLVolumeDisplayNode.h"
#include "vtkMRMLTransformNode.h"


vtkCxxRevisionMacro(vtkSlicerSliceLayerLogic, "$Revision: 1.9.12.1 $");
vtkStandardNewMacro(vtkSlicerSliceLayerLogic);


//----------------------------------------------------------------------------
vtkSlicerSliceLayerLogic::vtkSlicerSliceLayerLogic()
{
  this->VolumeNode = NULL;
  this->VolumeDisplayNode = NULL;
  this->SliceNode = NULL;

  this->XYToIJKTransform = vtkTransform::New();

  this->Reslice = vtkImageReslice::New();
  this->MapToRGBA = vtkImageMapToRGBA::New();
  this->MapToWindowLevelColors = vtkImageMapToWindowLevelColors::New();

  this->Reslice->SetBackgroundLevel(128);
  this->Reslice->AutoCropOutputOff();
  this->Reslice->SetOptimization(1);
  this->Reslice->SetOutputOrigin( 0, 0, 0 );
  this->Reslice->SetOutputSpacing( 1, 1, 1 );
  this->Reslice->SetOutputDimensionality( 2 );

  this->MapToWindowLevelColors->SetInput( this->Reslice->GetOutput() );
  this->MapToRGBA->SetInput( this->MapToWindowLevelColors->GetOutput() );

}

//----------------------------------------------------------------------------
vtkSlicerSliceLayerLogic::~vtkSlicerSliceLayerLogic()
{
  if ( this->SliceNode ) 
  {
    this->SetAndObserveMRML( vtkObjectPointer(&this->SliceNode), NULL );
  }
  if ( this->VolumeNode ) 
  {
    this->SetAndObserveMRML( vtkObjectPointer(&this->VolumeNode), NULL );
    this->VolumeNode->RemoveObservers ( vtkMRMLTransformableNode::TransformModifiedEvent, this->MRMLCallbackCommand );
  }
  if ( this->VolumeDisplayNode )
  {
    this->SetAndObserveMRML( vtkObjectPointer( &this->VolumeDisplayNode ), NULL );
  }

  this->SetSliceNode(NULL);
  this->SetVolumeNode(NULL);
  this->XYToIJKTransform->Delete();

  this->MapToWindowLevelColors->SetInput( NULL );
  this->MapToRGBA->SetInput( NULL );

  this->Reslice->Delete();
  this->MapToRGBA->Delete();
  this->MapToWindowLevelColors->Delete();
}

//----------------------------------------------------------------------------
void vtkSlicerSliceLayerLogic::ProcessMRMLEvents()
{
  this->UpdateTransforms();
}

//----------------------------------------------------------------------------
void vtkSlicerSliceLayerLogic::SetSliceNode(vtkMRMLSliceNode *sliceNode)
{
  if ( sliceNode != this->SliceNode )
    {
    this->SetAndObserveMRML( vtkObjectPointer(&this->SliceNode), sliceNode );

    // Update the reslice transform to move this image into XY
    this->UpdateTransforms();
    }
}

//----------------------------------------------------------------------------
void vtkSlicerSliceLayerLogic::SetVolumeNode(vtkMRMLVolumeNode *volumeNode)
{
  if (this->VolumeNode != NULL)
    {
    this->VolumeNode->RemoveObservers ( vtkMRMLTransformableNode::TransformModifiedEvent, this->MRMLCallbackCommand );
    }

  this->SetAndObserveMRML( vtkObjectPointer( &this->VolumeNode ), volumeNode );
 if (this->VolumeNode != NULL)
    {
    this->VolumeNode->AddObserver ( vtkMRMLTransformableNode::TransformModifiedEvent, this->MRMLCallbackCommand );
    }

  // Update the reslice transform to move this image into XY
  this->UpdateTransforms();
}

//----------------------------------------------------------------------------
void vtkSlicerSliceLayerLogic::UpdateNodeReferences ()
{
  // if there's a display node, observe it
  vtkMRMLVolumeDisplayNode *displayNode = NULL;
  if ( this->VolumeNode )
    {
    const char *id = this->VolumeNode->GetDisplayNodeID();
    if (id)
      {
      displayNode = vtkMRMLVolumeDisplayNode::SafeDownCast (this->MRMLScene->GetNodeByID(id));
      }
    }

    if ( displayNode == this->VolumeDisplayNode )
      {
      return;
      }

    if ( displayNode )
      {
      this->SetAndObserveMRML( vtkObjectPointer( &this->VolumeDisplayNode ), displayNode );
      }
    else
      {
      this->SetMRML( vtkObjectPointer( &this->VolumeDisplayNode ), NULL );
      }
}

//----------------------------------------------------------------------------
void vtkSlicerSliceLayerLogic::UpdateTransforms()
{

  // Ensure display node matches the one we are observing
  this->UpdateNodeReferences();

  unsigned int dimensions[3];
  dimensions[0] = 100;  // dummy values until SliceNode is set
  dimensions[1] = 100;
  dimensions[2] = 100;

  vtkMatrix4x4 *m = vtkMatrix4x4::New();
  m->Identity();

  if (this->SliceNode)
    {
    vtkMatrix4x4::Multiply4x4(this->SliceNode->GetXYToRAS(), m, m);
    this->SliceNode->GetDimensions(dimensions);
    }

  if (this->VolumeNode)
    {

    vtkMRMLTransformNode *transformNode = this->VolumeNode->GetParentTransformNode();
    if ( transformNode != NULL ) 
      {
      if ( !transformNode->IsTransformToWorldLinear() )
        {
        vtkErrorMacro ("non linear transforms not yet supported");
        }
      else
        {
        vtkMatrix4x4 *rasToRAS = vtkMatrix4x4::New();
        transformNode->GetMatrixTransformToWorld( rasToRAS );
        vtkMatrix4x4::Multiply4x4(rasToRAS, m, m); 
        rasToRAS->Delete();
        }
      }

    vtkMatrix4x4 *rasToIJK = vtkMatrix4x4::New();
    this->VolumeNode->GetRASToIJKMatrix(rasToIJK);
    vtkMatrix4x4::Multiply4x4(rasToIJK, m, m); 
    rasToIJK->Delete();


    this->Reslice->SetInput( this->VolumeNode->GetImageData() ); 
    }
  else
    {
    this->Reslice->SetInput( NULL ); 
    }

  if (this->VolumeDisplayNode)
    {
    this->MapToWindowLevelColors->SetWindow(this->VolumeDisplayNode->GetWindow());
    this->MapToWindowLevelColors->SetLevel(this->VolumeDisplayNode->GetLevel());

      // TODO: update the pipeline with other display values
        //double UpperThreshold;
        //double LowerThreshold;
        // Booleans
        //int Interpolate;
        //int AutoWindowLevel;
        //int ApplyThreshold;
        //int AutoThreshold;

    }

  this->XYToIJKTransform->SetMatrix( m );
  m->Delete();
  this->Reslice->SetResliceTransform( this->XYToIJKTransform );

  this->Reslice->SetOutputExtent( 0, dimensions[0]-1,
                                  0, dimensions[1]-1,
                                  0, dimensions[2]-1);

  this->Modified();
}


//----------------------------------------------------------------------------
void vtkSlicerSliceLayerLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);

  os << indent << "SlicerSliceLayerLogic:             " << this->GetClassName() << "\n";

  os << indent << "VolumeNode: " <<
    (this->VolumeNode ? this->VolumeNode->GetID() : "(none)") << "\n";
  os << indent << "SliceNode: " <<
    (this->SliceNode ? this->SliceNode->GetID() : "(none)") << "\n";
  // TODO: fix printing of vtk objects
  os << indent << "Reslice: " <<
    (this->Reslice ? "this->Reslice" : "(none)") << "\n";
  os << indent << "MapToRGBA: " <<
    (this->MapToRGBA ? "this->MapToRGBA" : "(none)") << "\n";
  os << indent << "MapToWindowLevelColors: " <<
    (this->MapToWindowLevelColors ? "this->MapToWindowLevelColors" : "(none)") << "\n";
}

