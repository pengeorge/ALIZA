/*
Alize is a free, open tool for speaker recognition

Alize is a development project initiated by the ELISA consortium
  [www.lia.univ-avignon.fr/heberges/ALIZE/ELISA] and funded by the
  French Research Ministry in the framework of the
  TECHNOLANGUE program [www.technolangue.net]
  [www.technolangue.net]

The Alize project team wants to highlight the limits of voice 
  authentication in a forensic context.
  The following paper proposes a good overview of this point:
  [Bonastre J.F., Bimbot F., Boe L.J., Campbell J.P., Douglas D.A., 
  Magrin-chagnolleau I., Person  Authentification by Voice: A Need of 
  Caution, Eurospeech 2003, Genova]
  The conclusion of the paper of the paper is proposed bellow:
  [Currently, it is not possible to completely determine whether the 
  similarity between two recordings is due to the speaker or to other 
  factors, especially when: (a) the speaker does not cooperate, (b) there 
  is no control over recording equipment, (c) recording conditions are not 
  known, (d) one does not know whether the voice was disguised and, to a 
  lesser extent, (e) the linguistic content of the message is not 
  controlled. Caution and judgment must be exercised when applying speaker 
  recognition techniques, whether human or automatic, to account for these 
  uncontrolled factors. Under more constrained or calibrated situations, 
  or as an aid for investigative purposes, judicious application of these 
  techniques may be suitable, provided they are not considered as infallible.
  At the present time, there is no scientific process that enables one to 
  uniquely characterize a person=92s voice or to identify with absolute 
  certainty an individual from his or her voice.]
  Contact Jean-Francois Bonastre for more information about the licence or
  the use of Alize

Copyright (C) 2003-2005
  Laboratoire d'informatique d'Avignon [www.lia.univ-avignon.fr]
  Frederic Wils [frederic.wils@lia.univ-avignon.fr]
  Jean-Francois Bonastre [jean-francois.bonastre@lia.univ-avignon.fr]
      
This file is part of Alize.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#if !defined(ALIZE_MixtureGFStat_cpp)
#define ALIZE_MixtureGFStat_cpp

#ifdef WIN32
#pragma warning( disable : 4291 )
#endif

#include <new>
#include "MixtureGFStat.h"
#include "alizeString.h"
#include "Feature.h"
#include "DistribGF.h"
#include "Mixture.h"
#include "MixtureGF.h"
#include "DistribRefVector.h"
#include "Config.h"
#include "Exception.h"
#include "StatServer.h"

using namespace alize;
typedef MixtureGFStat M;

//-------------------------------------------------------------------------
M::MixtureGFStat(const K&, StatServer& ss, const MixtureGF& m, const Config& c)
:MixtureStat(ss, m, c), _pMixForAccumulation(NULL), _pMixtureForEM(NULL) {}
//-------------------------------------------------------------------------
MixtureGFStat& M::create(const K&, StatServer& ss,
                                     const MixtureGF& m, const Config& c)
{
  MixtureGFStat* p = new (std::nothrow) MixtureGFStat(K::k, ss, m, c);
  assertMemoryIsAllocated(p, __FILE__, __LINE__);
  return *p;
}
//-------------------------------------------------------------------------
void M::resetEM()
{
  assert(_pMixture->getDistribCount() == _distribCount);
  resetOcc();

  // copy the original mixture and its ditributions
  if (_pMixtureForEM != NULL)
    delete _pMixtureForEM;
  _pMixtureForEM = &static_cast<MixtureGF&>(_pMixture->duplicate(K::k,
                                            DUPL_DISTRIB));
  // create and initialize a temporary mixtureGF to accumulate cov and mean
  if (_pMixForAccumulation != NULL)
    delete _pMixForAccumulation;
  _pMixForAccumulation = &MixtureGF::create(K::k, "",
                      _pMixtureForEM->getVectSize(),
                      _pMixtureForEM->getDistribCount());
  unsigned long vectSize = _pMixture->getVectSize();
  for (unsigned long c=0; c<_distribCount; c++)
  {
    DistribGF& d = _pMixForAccumulation->getDistrib(c);
    real_t* m = d.getMeanVect().getArray();
	
	// Directive de compilation pour probl�me windows
	// error C2040: 'c'�: les niveaux d'indirection de 'alize::real_t *' et de 'unsigned long' sont diff�rents
    #pragma message("A this place ln 114 a problem of compatibility for windows had badly solved bye a ifndef")
	#ifndef WIN32
    real_t* c = d.getCovMatrix().getArray();
	
    for (unsigned long i=0; i<vectSize; i++)
    {
      m[i] = 0.0;
      for (unsigned long j=0; j<vectSize; j++)
        c[i + j*vectSize] = 0.0;
      c[0 + 0*vectSize] = 1e200; // to avoid throwing exception "Matrix is not positive definite"
    }
	#endif
	
  }
  _featureCounterForEM = 0.0;
  _resetedEM = true;
}
//-------------------------------------------------------------------------
occ_t M::computeAndAccumulateEM(const Feature& f, double w)
{
  assertResetEMDone();
  real_t sum = computeAndAccumulateOcc(f, w);
  Feature::data_t* dataVect = f.getDataVector();
  unsigned long vectSize = _pMixture->getVectSize();
  unsigned long vectSize2 = vectSize*vectSize;

  for (unsigned long c=0; c<_distribCount; c++)
  {
    DistribGF& d = _pMixForAccumulation->getDistrib(c);
    real_t* dTmpMeanVect = d.getMeanVect().getArray();
    real_t* dTmpCovMatr  = d.getCovMatrix().getArray();
    
    for (unsigned long i=0; i<vectSize; i++)
    {
      real_t mean = _occVect[c] * dataVect[i];
      dTmpMeanVect[i] += mean;
      for (unsigned long j=i*vectSize; j<vectSize2; j += vectSize)
        dTmpCovMatr[i+j]  += mean * dataVect[j];
    }
  }
    _featureCounterForEM += w;
  return sum;
}
//-------------------------------------------------------------------------
void M::addAccEM(const MixtureStat& mx)
{
  const MixtureGFStat* p = dynamic_cast<const MixtureGFStat*>(&mx);
  if (p == NULL)
    throw Exception("MixtureStat incompatibility", __FILE__, __LINE__);
  if (p->_distribCount != _distribCount)
    throw Exception("MixtureStat incompatibility", __FILE__, __LINE__);
  //const MixtureGFStat& m = static_cast<const MixtureGFStat&>(mx);

  throw Exception("unimplemented method :o(", __FILE__, __LINE__);
  // TODO : implement this method
}
//-------------------------------------------------------------------------
const Mixture& M::getEM()
{
  assertResetEMDone();
  unsigned long vectSize = _pMixture->getVectSize();
  unsigned long c, idx, vectSize2 = vectSize*vectSize;
  occ_t occ, totOcc = 0.0;
  real_t* dTmpCovMatr;
  real_t* dTmpMeanVect;
  real_t* dCovMatr;
  real_t* dMeanVect;
  real_t mean, mean2, cov;

  for (c=0; c<_distribCount; c++)
    totOcc += _accumulatedOccVect[c];

  for (c=0; c<_distribCount; c++)
  {
    occ = _accumulatedOccVect[c];
    if (occ > 0.0)
    {
      DistribGF& dTmp = _pMixForAccumulation->getDistrib(c);
      dTmpCovMatr  = dTmp.getCovMatrix().getArray();
      dTmpMeanVect = dTmp.getMeanVect().getArray();

      DistribGF& d = _pMixtureForEM->getDistrib(c);
      dCovMatr  = d.getCovMatrix().getArray();
      dMeanVect = d.getMeanVect().getArray();


      for (unsigned long i=0; i<vectSize; i++)
      {
        dMeanVect[i] = mean = dTmpMeanVect[i] / occ;
        mean2 = mean*mean;
        for (unsigned long j=0; j<vectSize2; j += vectSize)
        {
          idx = i+j;
          cov  = dTmpCovMatr[idx] / occ - mean2;
          if (cov >= MIN_COV )
            dCovMatr[idx] = cov;
          else
            dCovMatr[idx] = MIN_COV;
        }
      }
      _pMixtureForEM->weight(c) = occ/totOcc;
      d.computeAll();
    }
  }
  return *_pMixtureForEM;
}
//-------------------------------------------------------------------------
MixtureGF& M::getInternalAccumEM()
{
  assertResetEMDone();
  assert(_pMixForAccumulation != NULL);
  return *_pMixForAccumulation;
}
//-------------------------------------------------------------------------
String M::getClassName() const { return "MixtureGFStat"; }
//-------------------------------------------------------------------------
M::~MixtureGFStat()
{
  if (_pMixForAccumulation != NULL)
    delete _pMixForAccumulation;
  if (_pMixtureForEM != NULL)
    delete _pMixtureForEM;
}
//-------------------------------------------------------------------------

#endif // !defined(ALIZE_MixtureGFStat_cpp)

