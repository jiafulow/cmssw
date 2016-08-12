#ifndef _L1ITMu_L1TMuonUpgradedTrackFinder_h_
#define _L1ITMu_L1TMuonUpgradedTrackFinder_h_
//asd
#include <memory>
#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/EDProducer.h"

#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "L1Trigger/L1TMuon/interface/deprecate/SubsystemCollectorFactory.h"

#include "DataFormats/HepMCCandidate/interface/GenParticleFwd.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "L1Trigger/L1TMuon/interface/deprecate/GeometryTranslator.h"
#include "L1Trigger/L1TMuon/interface/deprecate/MuonTriggerPrimitive.h"

#include "L1Trigger/L1TMuonEndCap/interface/MuonInternalTrack.h"
#include "L1Trigger/L1TMuonEndCap/interface/MuonInternalTrackFwd.h"


#include "L1Trigger/L1TMuonEndCap/interface/PhiMemoryImage.h"
#include "L1Trigger/L1TMuonEndCap/interface/EmulatorClasses.h"
#include "L1Trigger/CSCTrackFinder/interface/CSCTFPtLUT.h"
#include "L1Trigger/CSCTrackFinder/interface/CSCSectorReceiverLUT.h"
#include "DataFormats/CSCDigi/interface/CSCCorrelatedLCTDigiCollection.h"

#include "L1Trigger/L1TMuonEndCap/interface/PtAssignment.h"
#include "TTree.h"
#include <TMath.h>
#include <TCanvas.h>
#include <TLorentzVector.h>
#include "DataFormats/L1GlobalMuonTrigger/interface/L1MuGMTReadoutCollection.h"
#include <TStyle.h>
#include <TLegend.h>
#include <TF1.h>
#include <TH2.h>

#include "L1Trigger/L1TMuonEndCap/interface/PrimitiveConverter.h"

typedef edm::ParameterSet PSet;


//class L1TMuonEndCapTrackProducer : public edm::EDAnalyzer {

class L1TMuonEndCapTrackProducer : public edm::EDProducer {
public:
  L1TMuonEndCapTrackProducer(const PSet&);
  ~L1TMuonEndCapTrackProducer() {}
	
 //void analyze(const edm::Event&, const edm::EventSetup&); 
 void produce(edm::Event&, const edm::EventSetup&); 
  void beginJob();
  void endJob();
  
  
  edm::Service<TFileService> fs;
  TTree* outputTree;
  
  TH1F *trackPhi[2], *trackPt[2], *trackEta[2], *trackMode[2];
  
  int _nUpgradedTracks;
  int _nLegacyTracks;
  int _nGenMuons;
  int _nPU;
  double _UpgradedPt[30];
  double _UpgradedPt2[30];
  double _UpgradedEta[30];
  double _UpgradedPhi[30];
  int _UpgradedBX[30];
  int _UpgradedPatt[30][4];
  int _UpgradedQual[30][4];
  int _UpgradedBX_station[30][4];
  int _UpgradedMode[30];
  int _UpgradedMode2[30];
  double _LegacyPt[30];
  double _LegacyEta[30];
  double _LegacyPhi[30];
  int _LegacyQual[30];
  int _UpgradedRank[30];
  int _LegacyMode[30], _genCharge[30], _LegacyCharge[30], _dPhi12[30], _dPhi13[30], _dPhi14[30], _dPhi23[30], _dPhi24[30], _dPhi34[30];
  double _genPt[30], _genEta[30], _genPhi[30];
  
  int _nGMTTracks;
  double _GMTPt[30], _GMTEta[30], _GMTPhi[30];
  int  _GMTQual[30], _GMTBx[30], _GMTRank[30];
  int _GMTCSC[30];
  
  int _triggerSector[30];
  
  ///////////////////////////////////////
  //// For Emulator with timing /////////
  /////  we need all of these ///////////
  ///////////////////////////////////////
 // MatchingOutput Mout;
 // ZonesOutput Zout;
 // ExtenderOutput Eout;
 // PatternOutput Pout;
 // SortingOutput Sout;
 // std::vector<ConvertedHit> ConvHits;
 // std::vector<std::vector<DeltaOutput>> Dout;
  ///////////////////////////////////////
  ///////////////////////////////////////
  ///////////////////////////////////////
  ///////////////////////////////////////
  
  const float ptscale[33] = { 
  	-1.,   0.0,   1.5,   2.0,   2.5,   3.0,   3.5,   4.0,
    4.5,   5.0,   6.0,   7.0,   8.0,  10.0,  12.0,  14.0,  
    16.0,  18.0,  20.0,  25.0,  30.0,  35.0,  40.0,  45.0, 
    50.0,  60.0,  70.0,  80.0,  90.0, 100.0, 120.0, 140.0, 1.E6 };
	
	
   TH1F *FullPhi, *FullTheta, *CLCT, *FullZ;
  

private:
  PrimitiveConverter primConv_;

  edm::EDGetTokenT<CSCCorrelatedLCTDigiCollection> inputTokenCSC;
  
  edm::EDGetTokenT<std::vector<reco::GenParticle>> inputTokenGen;
  edm::EDGetTokenT<L1MuGMTReadoutCollection> inputTokenGMT;
  l1t::EmtfPtAssignment ptAssignment_;

};




#endif
