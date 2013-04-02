/** \file
 *  \ingroup commandLineTools 
 *  \details This program accepts as input a VTK polydata file
 *  corresponding to airway particles data and produces a VTK polydata
 *  file containing filtered particles labeled by generation.
 * 
 * Impose topology / treating each particle as a graph node, first
 * determine connections between nodes, then determine direction of
 * flow. Graph is assumed to be acyclic, but we do not assume that it
 * is connected. The framework allows for disconnected subgraphs to
 * exist (providing for the case in which we see airway obstruction,
 * due to mucous plugs, e.g., that 
 *
 * This program accepts as input a VTK polydata file corresponding to
 * airway particles data and produces a VTK polydata file containing
 * filtered particles labeled by generation. The input particles are
 * first processed by imposing a topology on the particles point
 * set. This topology is represented by a graph bidirectional edges
 * are formed between a pair of particles provided the pair are both
 * spatially close to one another and sufficiently aligned, where
 * alignment is defined according to the vector connecting the two
 * particles and the angles formed between that vector and the
 * particles' minor eigenvectors. (The user can specify the spatial
 * distance threshold using the -pdt flag and the angle threshold
 * using the -pat flag). Both the distance and angle thresholds are
 * learned from training data. 
 *
 * Training data / kernel density estimation / HMM\n";
 *
 *
 *
 *  Usage: FilterAndLabelAirwayParticlesByGeneration \<options\> where \<options\> is one or more of the following:\n
 *    \<-h\>     Display (this) usage information\n
 *    \<-i\>     Input airway particles file name\n
 *    \<-o\>     Output airway particles file name\n
 */


#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <tclap/CmdLine.h>
#include "vtkSmartPointer.h"
#include "vtkPolyDataReader.h"
#include "vtkPolyDataWriter.h"
#include "vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter.h"
#include "itkIdentityTransform.h"
#include "cipConventions.h"
#include "vtkFieldData.h"
#include "vtkFloatArray.h"
#include <cfloat>
#include <math.h>
#include <fstream>

void SetEmissionProbabilityStatsFromFile( vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter >, std::string );
void SetTransitionProbabilitiesFromFile( vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter >, std::string );
void SetTransitionProbabilityStatsFromFile( vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter >, std::string );

int main( int argc, char *argv[] )
{
  //
  // Begin by defining the arguments to be passed
  //
  std::string inParticlesFileName                 = "NA";
  std::string outParticlesFileName                = "NA";
  std::string emissionProbabilityStatsFileName    = "NA";
  std::string transitionProbabilitiesFileName     = "NA";
  std::string transitionProbabilityStatsFileName  = "NA";
  double      particleDistanceThreshold           = 2.0;
  double      kernelDensityEstimationROIRadius    = DBL_MAX;
  bool        printDiceScores                     = false;
  std::vector< std::string > airwayGenerationLabeledAtlasFileNames;

  //
  // Argument descriptions for user help
  //
  std::string programDesc = "This program takes an input airway particles dataset \
and assigns airway generation labels to each particle. The assigned labels are \
coded in the ChestType field data arrays in the output particles data set. \
The algorithm uses a Hidden Markov Model framework work to perform the generation \
labeling.";

  std::string inParticlesFileNameDesc  = "Input particles file name";
  std::string outParticlesFileNameDesc = "Output particles file name with airway generation labels";
  std::string emissionProbabilityStatsFileNameDesc = "csv file containing statistics needed to compute \
emission probabilities. These files are genereated by the GenerateStatisticsForAirwayGenerationLabeling \
program";
  std::string transitionProbabilitiesFileNameDesc = "Transition probabilities file name. \
These files are generated by the GenerateStatisticsForAirwayGenerationLabeling program.";
  std::string transitionProbabilityStatsFileNameDesc = "Transition probability stats file \
name. These files include computed mean and variances of scale differences and angles at \
branching locations. The are computed by the GenerateStatisticsForAirwayGenerationLabeling \
program.";
  std::string particleDistanceThresholdDesc = "Particle distance threshold. If two particles are \
farther apart than this threshold, they will not considered connected. Otherwise, a graph edge \
will be formed between the particles where the edge weight is a function of the distance \
between the particles. The weighted graph is then fed to a minimum spanning tree algorithm, the \
output of which is used to establish directionality throught the particles for HMM analysis.";
  std::string kernelDensityEstimationROIRadiusDesc = "The spherical radius region of interest \
over which contributions to the kernel density estimation are made. Only atlas particles that \
are within this physical distance will contribute to the estimate. By default, all atlas \
particles will contribute to the estimate.";
  std::string airwayGenerationLabeledAtlasFileNamesDesc =  "Airway generation labeled atlas file name. \
An airway generation labeled atlas is a particles data set that has field data array field named \
'ChestType' that, for each particle, has a correctly labeled airway generation label. \
Labeling must conform to the standards set forth in 'cipConventions.h'. \
The atlas must be in the same coordinate frame as the input dataset that \
is to be labeled. Multiple atlases may be specified. These atlases are \
used to compute the emission probabilities (see descriptions of the HMM \
algorithm) using kernel density estimation.";
  std::string printDiceScoresDesc = "Print Dice scores. Setting this flag assumes that the input particles \
have been labeled. This option can be used for debugging and for quality assessment.";

  //
  // Parse the input arguments
  //
  try
    {
    TCLAP::CmdLine cl( programDesc, ' ', "$Revision: 383 $" );

    TCLAP::ValueArg<std::string> inParticlesFileNameArg ( "i", "inPart", inParticlesFileNameDesc, true, inParticlesFileName, "string", cl );
    TCLAP::ValueArg<std::string> emissionProbabilityStatsFileNameArg ( "e", "", emissionProbabilityStatsFileNameDesc, false, emissionProbabilityStatsFileName, "string", cl );
    TCLAP::ValueArg<std::string> transitionProbabilitiesFileNameArg ( "", "tp", transitionProbabilitiesFileNameDesc, false, transitionProbabilitiesFileName, "string", cl );
    TCLAP::ValueArg<std::string> transitionProbabilityStatsFileNameArg ( "", "tps", transitionProbabilityStatsFileNameDesc, false, transitionProbabilityStatsFileName, "string", cl );
    TCLAP::ValueArg<std::string> outParticlesFileNameArg ( "o", "outPart", outParticlesFileNameDesc, true, outParticlesFileName, "string", cl );
    TCLAP::ValueArg<double> particleDistanceThresholdArg ( "d", "distThresh", particleDistanceThresholdDesc, false, particleDistanceThreshold, "double", cl );
    TCLAP::ValueArg<double> kernelDensityEstimationROIRadiusArg ( "", "kdeROI", kernelDensityEstimationROIRadiusDesc, false, kernelDensityEstimationROIRadius, "double", cl );
    TCLAP::MultiArg<std::string>  airwayGenerationLabeledAtlasFileNamesArg( "a", "atlas", airwayGenerationLabeledAtlasFileNamesDesc, true, "string", cl );
    TCLAP::SwitchArg printDiceScoresArg("","dice", printDiceScoresDesc, cl, false);

    cl.parse( argc, argv );

    inParticlesFileName                = inParticlesFileNameArg.getValue();
    outParticlesFileName               = outParticlesFileNameArg.getValue();
    emissionProbabilityStatsFileName   = emissionProbabilityStatsFileNameArg.getValue();
    transitionProbabilityStatsFileName = transitionProbabilityStatsFileNameArg.getValue();
    transitionProbabilitiesFileName    = transitionProbabilitiesFileNameArg.getValue();
    particleDistanceThreshold          = particleDistanceThresholdArg.getValue();
    kernelDensityEstimationROIRadius   = kernelDensityEstimationROIRadiusArg.getValue();
    printDiceScores                    = printDiceScoresArg.getValue();
    for ( unsigned int i=0; i<airwayGenerationLabeledAtlasFileNamesArg.getValue().size(); i++ )
      {
	airwayGenerationLabeledAtlasFileNames.push_back( airwayGenerationLabeledAtlasFileNamesArg.getValue()[i] );
      }
    }
  catch ( TCLAP::ArgException excp )
    {
    std::cerr << "Error: " << excp.error() << " for argument " << excp.argId() << std::endl;
    return cip::ARGUMENTPARSINGERROR;
    }

  //
  // Read the particles to which generation labels are to be assigned
  //
  std::cout << "Reading airway particles..." << std::endl;
  vtkSmartPointer< vtkPolyDataReader > particlesReader = vtkSmartPointer< vtkPolyDataReader >::New();
    particlesReader->SetFileName( inParticlesFileName.c_str() );
    particlesReader->Update();

  //
  // Read the atlas particles. TODO: Do we assume they will already be
  // registered to the input particle's reference frame, or do we also
  // read a transform file and do the mapping here? We will need and will 
  // a separate module to perform particles-to-particles registration.
  //
//   std::cout << "Reading kernel density estimation airway particles..." << std::endl;
//   vtkSmartPointer< vtkPolyDataReader > kdeParticlesReader = vtkSmartPointer< vtkPolyDataReader >::New();
//     kdeParticlesReader->SetFileName( kernelDensityEstimationParticlesFileName );
//     kdeParticlesReader->Update();

  
  //
  // Read transform file that will be used to map atlas images to input
  //

  //
  // Map all atlas members to input's coordinate frame
  //

  //
  // Apply 'vtkCIPAirwayParticlesToConnectedAirwayParticlesFilter' to 
  // establish connectivity / directionality. This filter should implement
  // minimum spanning tree (and should probably be renamed)
  //
//   std::cout << "Connecting airway particles..." << std::endl;
  vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter > particlesToGenLabeledParticles = 
    vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter >::New();
    particlesToGenLabeledParticles->SetInput( particlesReader->GetOutput() );
    particlesToGenLabeledParticles->SetParticleDistanceThreshold( particleDistanceThreshold );
    particlesToGenLabeledParticles->SetKernelDensityEstimationROIRadius( kernelDensityEstimationROIRadius );
    // Set the transition probabilities
    if ( transitionProbabilitiesFileName.compare( "NA" ) != 0 )
      {
	std::cout << "Setting transition probabilities..." << std::endl;
	SetTransitionProbabilitiesFromFile( particlesToGenLabeledParticles, transitionProbabilitiesFileName );
      }
    // Set the emission probability stats
    if ( emissionProbabilityStatsFileName.compare( "NA" ) != 0 )
      {
	std::cout << "Setting emission probability statistics..." << std::endl;
	SetEmissionProbabilityStatsFromFile( particlesToGenLabeledParticles, emissionProbabilityStatsFileName );
      }
    // Set the transition probability stats
    if ( transitionProbabilityStatsFileName.compare( "NA" ) != 0 )
      {
	std::cout << "Setting transition probability statistics..." << std::endl;
	SetTransitionProbabilityStatsFromFile( particlesToGenLabeledParticles, transitionProbabilityStatsFileName );
      }

  for ( unsigned int i=0; i<airwayGenerationLabeledAtlasFileNames.size(); i++ )
    {
    std::cout << "Reading atlas..." << std::endl;
    vtkSmartPointer< vtkPolyDataReader > atlasReader = vtkSmartPointer< vtkPolyDataReader >::New();
      atlasReader->SetFileName( airwayGenerationLabeledAtlasFileNames[i].c_str() );
      atlasReader->Update();

    particlesToGenLabeledParticles->AddAirwayGenerationLabeledAtlas( atlasReader->GetOutput() );
    }
    particlesToGenLabeledParticles->Update();

  std::cout << "Writing generation-labeled airway particles..." << std::endl;
  std::cout << outParticlesFileName << std::endl;
  vtkSmartPointer< vtkPolyDataWriter > particlesWriter = vtkSmartPointer< vtkPolyDataWriter >::New();
    particlesWriter->SetFileName( outParticlesFileName.c_str() );
    particlesWriter->SetInput( particlesToGenLabeledParticles->GetOutput() ); 
    particlesWriter->Update();

  //
  // Optionally compute DICE scores 
  //
  if ( printDiceScores )
    {
      // Also create a confusion matrix
      std::vector< std::vector< unsigned int > > confusionMatrix;      
      for ( unsigned int i=0; i<10; i++ )
	{
	  std::vector< unsigned int > tmp;
	  for ( unsigned int j=0; j<10; j++ )
	    {
	      tmp.push_back( 0 );
	    }
	  confusionMatrix.push_back(tmp);
	}

      std::vector< unsigned char > statesVec;
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION0 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION1 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION2 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION3 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION4 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION5 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION6 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION7 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION8 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION9 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::AIRWAYGENERATION10 ) );
      statesVec.push_back( static_cast< unsigned char >( cip::UNDEFINEDTYPE ) );
      
      std::map< unsigned char, unsigned int > intersectionCounter;
      std::map< unsigned char, unsigned int > inTypeCounter;
      std::map< unsigned char, unsigned int > outTypeCounter;
      
      for ( unsigned int i=0; i<statesVec.size(); i++ )
	{
	  intersectionCounter[statesVec[i]] = 0;
	  inTypeCounter[statesVec[i]]       = 0;
	  outTypeCounter[statesVec[i]]      = 0;
	}
      
      unsigned char inType, outType;
      for ( unsigned int i=0; i<particlesReader->GetOutput()->GetNumberOfPoints(); i++ )
	{
	  inType  = static_cast< unsigned char >( particlesReader->GetOutput()->GetFieldData()->GetArray( "ChestType" )->GetTuple(i)[0] );
	  outType = static_cast< unsigned char >( particlesToGenLabeledParticles->GetOutput()->GetFieldData()->GetArray( "ChestType" )->GetTuple(i)[0] );
	  confusionMatrix[inType-38][outType-38] += 1;
 
	  if ( inType == outType )
	    {
	      intersectionCounter[inType]++;
	    }
	  inTypeCounter[inType]++;
	  outTypeCounter[outType]++;
	}
      
      ChestConventions conventions;
      for ( unsigned int i=0; i<statesVec.size(); i++ )
	{
	  double num    = static_cast< float >( intersectionCounter[statesVec[i]] );
	  double denom1 = static_cast< float >( inTypeCounter[statesVec[i]] );
	  double denom2 = static_cast< float >( outTypeCounter[statesVec[i]] );
	  double denom  = denom1 + denom2;
	  
	  if ( denom > 0.0 )
	    {
	      std::cout << "Dice for " << conventions.GetChestTypeName( statesVec[i] ) << ":\t" << 2.0*num/(denom ) << std::endl;
	    }
	}

      std::cout << "----------------- Confusion Matrix -----------------------" << std::endl;
      for ( unsigned int i=0; i<10; i++ )
	{
	  for ( unsigned int j=0; j<10; j++ )
	    {
	      std::cout << confusionMatrix[i][j] << "\t";
	    }
	  std::cout << std::endl;
	}
    }

  std::cout << "DONE." << std::endl;

  return cip::EXITSUCCESS;
}


void SetEmissionProbabilityStatsFromFile( vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter > particlesToGenLabeledParticles, 
					  std::string fileName )
{
  ChestConventions conventions;

  char wholeLine[100000];

  std::ifstream file( fileName.c_str() );

  unsigned int commaLocOld, commaLocNew;

  // Gobble up the header
  file.getline( wholeLine, 100000 );

  bool keepChecking = true;

  // Now read and parse the remainder of the file
  do
    {
      commaLocOld = 0;
  
      file.getline( wholeLine, 100000 );
      if ( !file.eof() )
	{	  
	  std::string strLine( wholeLine, 100000 );

	  commaLocNew = strLine.find( ',', 0 );
	  std::string cipTypeName = strLine.substr( 0, commaLocNew-commaLocOld+1).c_str();
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double scaleDiffMean = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double scaleDiffSTD = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double distanceMean = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double distanceSTD = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double angleMean = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double angleSTD = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double numSamples = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  unsigned char cipType = conventions.GetChestTypeValueFromName( cipTypeName );

	  if ( int(cipType) > 0 && int(cipType) < conventions.GetNumberOfEnumeratedChestRegions() )
	    {
	      particlesToGenLabeledParticles->SetScaleStandardDeviation( cipType, scaleDiffSTD );
	      particlesToGenLabeledParticles->SetDistanceStandardDeviation( cipType, distanceSTD );
	      particlesToGenLabeledParticles->SetAngleStandardDeviation( cipType, angleSTD );
	      particlesToGenLabeledParticles->SetScaleMean( cipType, scaleDiffMean );
	      particlesToGenLabeledParticles->SetDistanceMean( cipType, distanceMean );
	      particlesToGenLabeledParticles->SetAngleMean( cipType, angleMean );
	    }
	  else
	    {
	      keepChecking = false;
	    }
	}
    }
  while ( !file.eof() && keepChecking );
}


void SetTransitionProbabilitiesFromFile( vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter > particlesToGenLabeledParticles, 
					 std::string fileName )
{
  ChestConventions conventions;
  
  char wholeLine[100000];

  std::ifstream file( fileName.c_str() );

  unsigned int commaLocOld, commaLocNew;
  double prob;

  for ( unsigned int i=0; i<=10; i++ )
    {
      unsigned char fromType = (unsigned char)(i) + 38;

      commaLocOld = 0;
  
      file.getline( wholeLine, 100000 );
      std::string strLine( wholeLine, 100000 );

      for ( unsigned int j=0; j<=10; j++ )
	{
	  unsigned char toType = (unsigned char)(j) + 38;

	  if ( j==0 )
	    {	    
	      commaLocNew = strLine.find( ',', 0 );
	      prob = atof(strLine.substr( 0, commaLocNew-commaLocOld+1).c_str());
	      commaLocOld = commaLocNew;
	    }
	  else
	    {
	      commaLocNew = strLine.find( ',', commaLocOld+1 );
	      prob = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	      commaLocOld = commaLocNew;
	    }

	  particlesToGenLabeledParticles->SetTransitionProbability( fromType, toType, prob );
	}
    }
}


void SetTransitionProbabilityStatsFromFile( vtkSmartPointer< vtkCIPAirwayParticlesToGenerationLabeledAirwayParticlesFilter > particlesToGenLabeledParticles, 
					    std::string fileName )
{
  std::cout << fileName << std::endl;

  ChestConventions conventions;

  char wholeLine[100000];

  std::ifstream file( fileName.c_str() );

  unsigned int commaLocOld, commaLocNew;

  // Gobble up the header
  file.getline( wholeLine, 100000 );

  // Now read and parse the remainder of the file
  do
    {
      commaLocOld = 0;
  
      file.getline( wholeLine, 100000 );
      std::cout << wholeLine << std::endl;
      if ( !file.eof() )
	{	  
	  std::string strLine( wholeLine, 100000 );

	  commaLocNew = strLine.find( ',', 0 );
	  std::string fromTypeName = strLine.substr( 0, commaLocNew-commaLocOld).c_str();
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  std::string toTypeName = strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str();
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double scaleDiffMean = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double scaleDiffSTD = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double angleMean = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double angleSTD = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  commaLocNew = strLine.find( ',', commaLocOld+1 );
	  double numSamples = atof( strLine.substr( commaLocOld+1, commaLocNew-commaLocOld-1).c_str() );
	  commaLocOld = commaLocNew;

	  unsigned char fromType = conventions.GetChestTypeValueFromName( fromTypeName );
	  unsigned char toType   = conventions.GetChestTypeValueFromName( toTypeName );

	  double scaleDiffVar = scaleDiffSTD*scaleDiffSTD;
	  double angleVar     = angleSTD*angleSTD;

	  std::cout << "FROM TYPE NAME:\t" << fromTypeName << std::endl;
	  std::cout << "TO TYPE NAME:\t" << toTypeName << std::endl; 
	  std::cout << "fromType:\t" << int(fromType) << std::endl;
	  std::cout << "toType:\t" << int(toType) << std::endl;
	  if ( numSamples > 10 )
	    {
	      std::cout << "HERE" << std::endl;
	      particlesToGenLabeledParticles->SetNormalTransitionProbabilityMeansAndVariances( fromType, toType, scaleDiffMean, scaleDiffVar,
											       angleMean, angleVar );
	    }
	}
    }
  while ( !file.eof() );
}

#endif
