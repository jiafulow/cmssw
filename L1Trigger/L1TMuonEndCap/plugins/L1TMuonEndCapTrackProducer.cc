///////////////////////////////////////////////////////////////
// Upgraded Encdap Muon Track Finding Algorithm		    	//
//							   								//
// Info: A human-readable version of the firmware based     //
//       track finding algorithm which will be implemented  //
//       in the upgraded endcaps of CMS. DT and RPC inputs  //
//	     are not considered in this algorithm.      		//
//								   							//
// Author: M. Carver (UF)				    				//
//////////////////////////////////////////////////////////////

#define NUM_SECTORS 12

#include "L1Trigger/L1TMuonEndCap/plugins/L1TMuonEndCapTrackProducer.h"
#include "L1Trigger/CSCCommonTrigger/interface/CSCPatternLUT.h"
#include "L1Trigger/CSCTrackFinder/test/src/RefTrack.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>

#include "L1Trigger/L1TMuonEndCap/interface/BXAnalyzer.h"
#include "L1Trigger/L1TMuonEndCap/interface/ZoneCreation.h"
#include "L1Trigger/L1TMuonEndCap/interface/PatternRecognition.h"
#include "L1Trigger/L1TMuonEndCap/interface/SortSector.h"
#include "L1Trigger/L1TMuonEndCap/interface/Matching.h"
#include "L1Trigger/L1TMuonEndCap/interface/Deltas.h"
#include "L1Trigger/L1TMuonEndCap/interface/BestTracks.h"
#include "L1Trigger/L1TMuonEndCap/interface/PtAssignment.h"
#include "L1Trigger/L1TMuonEndCap/interface/ChargeAssignment.h"
#include "L1Trigger/L1TMuonEndCap/interface/MakeRegionalCand.h"

// New EDM output for detailed track and hit information - AWB 01.04.16
#include "L1Trigger/L1TMuonEndCap/interface/EMTFTrackTools.h"
#include "L1Trigger/L1TMuonEndCap/interface/EMTFHitTools.h"

using namespace L1TMuon;


L1TMuonEndCapTrackProducer::L1TMuonEndCapTrackProducer(const PSet& p) {

  inputTokenCSC = consumes<CSCCorrelatedLCTDigiCollection>(p.getParameter<edm::InputTag>("CSCInput"));
  inputTokenGen = consumes<std::vector<reco::GenParticle>>(p.getParameter<edm::InputTag>("GenInput"));
  inputTokenGMT = consumes<L1MuGMTReadoutCollection>(p.getParameter<edm::InputTag>("GMTInput"));
  
  produces<l1t::RegionalMuonCandBxCollection >("EMTF");
  produces< l1t::EMTFTrackCollection >("EMTF");
  produces< l1t::EMTFHitCollection >("EMTF");  
  produces< l1t::EMTFTrackExtraCollection >("EMTF");
  produces< l1t::EMTFHitExtraCollection >("EMTF");  

}


void L1TMuonEndCapTrackProducer::produce(edm::Event& ev,
			       const edm::EventSetup& es) {

  //bool verbose = false;
  
  
  //std::cout<<"1047794192 = "<<ptAssignment_.calculatePt(1047794192)*2.8 + 1<<", and 1022628368 = "<<ptAssignment_.calculatePt(1022628368)*2.8 + 1<<", and 1049891344 = "<<ptAssignment_.calculatePt(1049891344)*2.8 + 1<<".\n";

  //fprintf (write,"12345\n"); //<-- part of printing text file to send verilog code, not needed if George's package is included

	//std::cout<<"12345\n12345\n12345\n12345\n12345\n12345\n";

  //std::auto_ptr<L1TMuon::InternalTrackCollection> FoundTracks (new L1TMuon::InternalTrackCollection);
  std::auto_ptr<l1t::RegionalMuonCandBxCollection > OutputCands (new l1t::RegionalMuonCandBxCollection);
  std::auto_ptr<l1t::EMTFTrackCollection> OutTracks (new l1t::EMTFTrackCollection);
  std::auto_ptr<l1t::EMTFHitCollection> OutHits (new l1t::EMTFHitCollection);
  std::auto_ptr<l1t::EMTFTrackExtraCollection> OutputTracks (new l1t::EMTFTrackExtraCollection);
  std::auto_ptr<l1t::EMTFHitExtraCollection> OutputHits (new l1t::EMTFHitExtraCollection);

  std::vector<BTrack> PTracks[NUM_SECTORS];
  std::vector<BTrack> PTracks_BX[NUM_SECTORS][3];

  std::vector<TriggerPrimitive> tester;
  //std::vector<InternalTrack> FoundTracks;
  
  _nUpgradedTracks = 0;
	_nLegacyTracks = 0;
	_nGenMuons = 0;
	
	for(int i=0;i<30;i++){
	
		_genCharge[i]= 0;
		_LegacyCharge[i] = 0;
		_UpgradedPt[i] = 0;
		_UpgradedPt2[i] = 0;
		_LegacyPt[i] = 0;
		_LegacyQual[i] = 0;
		
		_UpgradedMode[i] = 0;
		_UpgradedMode2[i] = 0;
		_LegacyMode[i] = 0;
		
		_UpgradedRank[i] = -1;
		_UpgradedBX[i] = 20;
		for(int j=0;j<4;j++){
			_UpgradedQual[i][j] = -1;
			_UpgradedPatt[i][j] = -1;
			_UpgradedBX_station[i][j] = 20;
		}	
		
		_genPt[i] = 0;
		
		_GMTPt[i] = 0;
		_GMTRank[i] = 0;
		_GMTQual[i] = 0;
		_GMTCSC[i] = 0;
		_GMTBx[i] = -999;
		
	
	}
	
	float genPt = 0, genPhi = -5, genEta = -5;
  
 // if(!isData){
 /*
  edm::Handle<std::vector<reco::GenParticle>> genMuons;
  ev.getByToken(inputTokenGen,genMuons);
  
  
  for(std::vector<reco::GenParticle>::const_iterator gmi = genMuons->begin();gmi != genMuons->end();gmi++){
  
  		//std::cout<<"gen pt = "<<gmi->pt()<<", phi = "<<gmi->phi()<<" and eta = "<<gmi->eta()<<"\n";
  
  		if(fabs(gmi->pdgId()) == 13){
  		genPt = gmi->pt();
 		genEta = gmi->eta();
  		genPhi = gmi->phi();
		
		
		//std::cout<<"gen pt/eta/phi = "<<gm->pt()<<"/"<<gm->eta()<<"/"<<gm->phi()<<"\n";
		//	_genPt[_nGenMuons] = gmi->pt();
 		//	_genEta[_nGenMuons] = gmi->eta();
  		//	_genPhi[_nGenMuons] = gmi->phi();
		//	_genCharge[_nGenMuons] = gmi->pdgId();
			
		//	_nGenMuons++;
		
  	}
  }
  */
  ///////////////////////////
  ////// Get GMT Muons ///////
  ////////////////////////////
  /*
  edm::Handle<L1MuGMTReadoutCollection> GMTRC;
  ev.getByToken(inputTokenGMT,GMTRC);
	
	
	_nGMTTracks = 0;
	
	std::vector<L1MuGMTReadoutRecord> gmt_records = GMTRC->getRecords();
   // std::cout<<"gmtrecords size = "<<gmt_records.size()<<"\n";//
	std::vector<L1MuGMTReadoutRecord>::const_iterator igmtrr;
  	for(igmtrr=gmt_records.begin(); igmtrr!=gmt_records.end(); igmtrr++) {
	
	
		std::vector<L1MuGMTExtendedCand>::const_iterator gmt_iter;
 	 	std::vector<L1MuGMTExtendedCand> exc = igmtrr->getGMTCands();
  		for(gmt_iter=exc.begin(); gmt_iter!=exc.end(); gmt_iter++) {
    		if ( _nGMTTracks < 10 && !(*gmt_iter).empty() ) {
			
			
				float phi = (*gmt_iter).phiValue();
				if(phi > 3.14159)
					phi -= 6.28318;
			
				_GMTPt[_nGMTTracks] = (*gmt_iter).ptValue();
				//std::cout<<"gmt pt = "<<(*gmt_iter).ptValue()<<", eta = "<<(*gmt_iter).etaValue()<<" and phi = "<<phi<<"\n";
				_GMTEta[_nGMTTracks] = (*gmt_iter).etaValue();
				_GMTPhi[_nGMTTracks] = phi;
				_GMTQual[_nGMTTracks] = (*gmt_iter).quality();
				_GMTRank[_nGMTTracks] = (*gmt_iter).rank();
				_GMTBx[_nGMTTracks] = (*gmt_iter).bx();
				
				if((*gmt_iter).detector() == 4)
					_GMTCSC[_nGMTTracks] = 1;
				else
					_GMTCSC[_nGMTTracks] = 0;
				
				_nGMTTracks++;
			
			}
			
		}
	
	
	}
	
	*/
  
  //////////////////////////////////////////////
  ///////// Make Trigger Primitives ////////////
  //////////////////////////////////////////////
  
  edm::Handle<CSCCorrelatedLCTDigiCollection> MDC;
  ev.getByToken(inputTokenCSC, MDC);
  std::vector<TriggerPrimitive> out;
  
  auto chamber = MDC->begin();
  auto chend  = MDC->end();
  for( ; chamber != chend; ++chamber ) {
    auto digi = (*chamber).second.first;
    auto dend = (*chamber).second.second;
    for( ; digi != dend; ++digi ) {
      out.push_back(TriggerPrimitive((*chamber).first,*digi));
      l1t::EMTFHitExtra thisHit;
      thisHit.ImportCSCDetId( (*chamber).first );
      thisHit.ImportCSCCorrelatedLCTDigi( *digi );
      if (thisHit.Station() == 1 && thisHit.Ring() == 1 && thisHit.Strip() > 127) thisHit.set_ring(4);
      thisHit.set_neighbor(0);
      OutputHits->push_back( thisHit );
      if ( ((thisHit.Ring() != 1 || thisHit.Station() == 1) && (thisHit.Chamber() % 6 == 2)) ||
	   ((thisHit.Ring() == 1 && thisHit.Station()  > 1) && (thisHit.Chamber() % 3 == 1)) ) {
	l1t::EMTFHitExtra neighborHit = thisHit;
	neighborHit.set_neighbor(1);
	OutputHits->push_back( neighborHit );
      }
    }
  }
  

  //////////////////////////////////////////////
  ///////// Get Trigger Primitives /////////////  Retrieve TriggerPrimitives from the event record: Currently does nothing because we don't take RPC's
  //////////////////////////////////////////////

 // auto tpsrc = _tpinputs.cbegin();
  //auto tpend = _tpinputs.cend();
 // for( ; tpsrc != tpend; ++tpsrc ) {
   // edm::Handle<TriggerPrimitiveCollection> tps;
   // ev.getByLabel(*tpsrc,tps);
    auto tp = out.cbegin();
    auto tpend = out.cend();
	
	bool S1 =false;
	int stationBool = 0;

    for( ; tp != tpend; ++tp ) {
      if(tp->subsystem() == 1)
      {
		//TriggerPrimitiveRef tpref(out,tp - out.cbegin());

		//tester.push_back(*tp);

		//std::cout<<"\ntrigger prim found station:"<<tp->detId<CSCDetId>().station()<<std::endl;
      }

     }
	 
	 //bool good = false;
	 
	 for(unsigned int i1=0;i1<out.size();i1++){
	 
	 	tester.push_back(out[i1]);
		
		switch(out[i1].detId<CSCDetId>().station()){
		
			case 1: stationBool |= 8;break;
			case 2: stationBool |= 4;break;
			case 3: stationBool |= 2;break;
			case 4: stationBool |= 1;break;
			default: stationBool |= 0;
		
		}
		
		if(out[i1].detId<CSCDetId>().triggerSector() == 1 && out[i1].detId<CSCDetId>().endcap() == 1)////
		  S1 = true;
		  
		 bool NC = false;
		 
		 if(out[i1].detId<CSCDetId>().station() == 1 && (out[i1].Id() == 3 || out[i1].Id() == 6 || out[i1].Id() == 9))
		 	NC = true;
			
		 if(out[i1].detId<CSCDetId>().station() > 1 && (out[i1].Id() == 3 || out[i1].Id() == 9))
		 	NC = true;
		  
		  
		if(out[i1].detId<CSCDetId>().triggerSector() == 6 && out[i1].detId<CSCDetId>().endcap() == 1 && NC )////
			S1 = true;
	 
	 	for(unsigned int i2=i1+1;i2<out.size();i2++){
	 
	 		if(i1 == i2) continue;
			
			if(out[i1].detId<CSCDetId>().station() == out[i2].detId<CSCDetId>().station() &&
				out[i1].detId<CSCDetId>().endcap() == out[i2].detId<CSCDetId>().endcap() &&
				out[i1].detId<CSCDetId>().triggerSector() == out[i2].detId<CSCDetId>().triggerSector() &&
				out[i1].detId<CSCDetId>().ring() == out[i2].detId<CSCDetId>().ring() &&
				out[i1].detId<CSCDetId>().chamber() == out[i2].detId<CSCDetId>().chamber() &&
				out[i1].Id() == out[i2].Id() && out[i1].getBX() == out[i2].getBX()){ 
					/*
						bool ok = false;
						
						if( (out[i1].getCSCData().strip < 127 && out[i2].getCSCData().strip > 127) || (out[i2].getCSCData().strip < 127 && out[i1].getCSCData().strip > 127) )
							ok = true;
				
						if(out[i1].detId<CSCDetId>().station() == 1 && out[i1].detId<CSCDetId>().ring() == 1 && ok){
							std::cout<<"2 TPs in same chamber, station "<<out[i1].detId<CSCDetId>().station()<<", ring "<<out[i1].detId<CSCDetId>().ring()<<": Event "<<ev.id().event()<<"\n";
							good = true;
						}
						
						if(good)
							std::cout<<"2 TPs in same chamber, station "<<out[i1].detId<CSCDetId>().station()<<", ring "<<out[i1].detId<CSCDetId>().ring()<<": Event "<<ev.id().event()<<"\n";
						
						*/	
						TriggerPrimitive NewWire1(out[i1],out[i2]);
						TriggerPrimitive NewWire2(out[i2],out[i1]);
						tester.push_back(NewWire1);
						tester.push_back(NewWire2);
						
				}
	 
	 	}
	 }
	 
	 bool MoreThanOne = false;
	 
	 if(stationBool != 1 && stationBool != 2 && stationBool != 4 && stationBool != 8)
	 	MoreThanOne = true;
	 
	/* if(S1 && MoreThanOne){
	  std::cout<<"12345\n12345\n12345\n12345\n12345\n12345\n12345\n12345\n12345\n12345\n";
	  std::cout<<"Gen pT/eta/phi = "<<genPt<<" "<<genEta<<" "<<genPhi<<"\n";
	  
	  }*/
	 
	 
   //}
  std::vector<ConvertedHit> CHits[NUM_SECTORS];
  //MatchingOutput MO[NUM_SECTORS];

for(int SectIndex=0;SectIndex<NUM_SECTORS;SectIndex++){//perform TF on all 12 sectors

//if(SectIndex !=9) continue;

  //////////////////////////////////////////////////////  Input is raw hit information from
  ///////////////// TP Conversion //////////////////////  Output is vector of Converted Hits
  //////////////////////////////////////////////////////

 	std::vector<ConvertedHit> ConvHits = primConv_.convert(tester,SectIndex, ev.id().event());
	CHits[SectIndex] = ConvHits;
	
	
	/*if(S1 && MoreThanOne && ConvHits.size()){
	  std::cout<<"12345\n12345\n12345\n12345\n12345\n12345\n12345\n12345\n12345\n12345"<<ev.id().event()<<"\n";//
	  //std::cout<<"Gen pT/eta/phi = "<<genPt<<" "<<genEta<<" "<<genPhi<<"\n";
	  
	  fprintf(GenInfo,"Gen pT/eta/phi = %f	%f	%f event %llu\n",genPt,genEta,genPhi,ev.id().event());
	  
	  }*/
	
	///ConvHits.clear();
	// Fill OutputHits with ConvertedHit information
	for (uint iCHit = 0; iCHit < ConvHits.size(); iCHit++) {
	
		FullPhi->Fill(ConvHits[iCHit].Phi());
		FullTheta->Fill(ConvHits[iCHit].Theta());
		CLCT->Fill(ConvHits[iCHit].Pattern());
		FullZ->Fill(ConvHits[iCHit].Zhit());
	  // bool isMatched = false;
	  
	  if(ConvHits[iCHit].Phi() == -999){
	  	std::cout<<"weird event with -999 phi  -> "<<ev.id().event()<<"\n";}

	  for (uint iHit = 0; iHit < OutputHits->size(); iHit++) {
	    if ( ConvHits.at(iCHit).Station()   == OutputHits->at(iHit).Station() &&
		 ( ConvHits.at(iCHit).Id()      == OutputHits->at(iHit).CSC_ID()  ||
		   ConvHits.at(iCHit).Id()      == ( (OutputHits->at(iHit).Ring() != 4) // Account for either ME1/1a 
						   ? OutputHits->at(iHit).CSC_ID()    // CSC ID numbering convention
						   : OutputHits->at(iHit).CSC_ID() + 9 ) ) &&
		 ConvHits.at(iCHit).Wire()       == OutputHits->at(iHit).Wire()    &&
		 ConvHits.at(iCHit).Strip()      == OutputHits->at(iHit).Strip()   &&
		 ConvHits.at(iCHit).BX() - 6     == OutputHits->at(iHit).BX()      &&
		 ConvHits.at(iCHit).IsNeighbor() == OutputHits->at(iHit).Neighbor() ) {
	      // isMatched = true;
	      OutputHits->at(iHit).set_neighbor    ( ConvHits.at(iCHit).IsNeighbor());
	      OutputHits->at(iHit).set_sector_index( ConvHits.at(iCHit).SectorIndex() );
	      OutputHits->at(iHit).set_zone_hit    ( ConvHits.at(iCHit).Zhit()   );
	      OutputHits->at(iHit).set_phi_hit     ( ConvHits.at(iCHit).Ph_hit() );
	      OutputHits->at(iHit).set_phi_z_val   ( ConvHits.at(iCHit).Phzvl()  );
	      OutputHits->at(iHit).set_phi_loc_int ( ConvHits.at(iCHit).Phi()    );
	      OutputHits->at(iHit).set_theta_int   ( ConvHits.at(iCHit).Theta()  );

	      //OutputHits->at(iHit).SetZoneContribution ( ConvHits.at(iCHit).ZoneContribution() );
	      OutputHits->at(iHit).set_phi_loc_deg  ( l1t::calc_phi_loc_deg( OutputHits->at(iHit).Phi_loc_int() ) );
	      OutputHits->at(iHit).set_phi_loc_rad  ( l1t::calc_phi_loc_rad( OutputHits->at(iHit).Phi_loc_int() ) );
	      OutputHits->at(iHit).set_phi_glob_deg ( l1t::calc_phi_glob_deg_hit( OutputHits->at(iHit).Phi_loc_deg(), OutputHits->at(iHit).Sector_index() ) );
	      OutputHits->at(iHit).set_phi_glob_rad ( l1t::calc_phi_glob_rad_hit( OutputHits->at(iHit).Phi_loc_rad(), OutputHits->at(iHit).Sector_index() ) );
	      OutputHits->at(iHit).set_theta_deg    ( l1t::calc_theta_deg_from_int( OutputHits->at(iHit).Theta_int() ) );
	      OutputHits->at(iHit).set_theta_rad    ( l1t::calc_theta_rad_from_int( OutputHits->at(iHit).Theta_int() ) );
	      OutputHits->at(iHit).set_eta( l1t::calc_eta_from_theta_rad( OutputHits->at(iHit).Theta_rad() ) * OutputHits->at(iHit).Endcap() );

	      OutHits->push_back( OutputHits->at(iHit).CreateEMTFHit() );
	    }
	  } // End loop: for (uint iHit = 0; iHit < OutputHits->size(); iHit++)

	  // if (isMatched == false) {
	  //   std::cout << "***********************************************" << std::endl;
	  //   std::cout << "Unmatched ConvHit in event " << ev.id().event() << ", SectIndex " << SectIndex << std::endl;
	  //   std::cout << "ConvHit: station = " << ConvHits.at(iCHit).Station() << ", CSC ID = " << ConvHits.at(iCHit).Id()
	  // 	      << ", sector index = " << ConvHits.at(iCHit).SectorIndex() << ", subsector = " << ConvHits.at(iCHit).Sub()
	  // 	      << ", wire = " << ConvHits.at(iCHit).Wire() << ", strip = " << ConvHits.at(iCHit).Strip()
	  // 	      << ", BX = " << ConvHits.at(iCHit).BX() << ", neighbor = " << ConvHits.at(iCHit).IsNeighbor() << std::endl;
	    
	  //   for (uint iHit = 0; iHit < OutputHits->size(); iHit++) {
	  //     std::cout << "EMTFHitExtra: station = " << OutputHits->at(iHit).Station() << ", CSC ID = " << OutputHits->at(iHit).CSC_ID()
	  // 		<< ", sector index = " << OutputHits->at(iHit).Sector_index() << ", subsector = " << OutputHits->at(iHit).Subsector() 
	  // 		<< ", wire = " << OutputHits->at(iHit).Wire() << ", strip = " << OutputHits->at(iHit).Strip()
	  // 		<< ", BX = " << OutputHits->at(iHit).BX() << ", neighbor = " << OutputHits->at(iHit).Neighbor()
	  // 		<< ", chamber = " << OutputHits->at(iHit).Chamber() << ", ring = " << OutputHits->at(iHit).Ring() 
	  // 		<< ", endcap = " << OutputHits->at(iHit).Endcap() << ", sector = " << OutputHits->at(iHit).Sector() << std::endl;
	  //   }
	  // }

	} // End loop: for (uint iCHit = 0; iCHit < ConvHits.size(); iCHit++)
	

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////print values for input into Alex's emulator code/////////////////////////////////////////////////////
	//for(std::vector<ConvertedHit>::iterator h = ConvHits.begin();h != ConvHits.end();h++){

		//if((h->Id()) > 9){h->SetId(h->Id() - 9);h->SetStrip(h->Strip() + 128);}
		//fprintf (write,"0	1	1 	%d	%d\n",h->Sub(),h->Station());
		//fprintf (write,"1	%d	%d 	%d\n",h->Quality(),h->Pattern(),h->Wire());
		//fprintf (write,"%d	0	%d\n",h->Id(),h->Strip());
	//}
////////////////////////////////print values for input into Alex's emulator code/////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



 //////////////////////////////////////////////////////
 //////////////////////////////////////////////////////  Takes the vector of converted hits and groups into 3 groups of hits
 ////////////////////// BX Grouper ////////////////////  which are 3 BX's wide. Effectively looking 2 BX's into the future and
 //////////////////////////////////////////////////////  past from the central BX, this analyzes a total of 5 BX's.
 //////////////////////////////////////////////////////
  
 std::vector<std::vector<ConvertedHit>> GroupedHits = GroupBX(ConvHits);


////////////////////////////////////////////////////////  Creates a zone for each of the three groups created in the BX Grouper module.
////////// Creat Zones for pattern Recognition /////////  The output of this module not only contains the zones but also the
////////////////////////////////////////////////////////  reference back to the TriggerPrimitives that went into making them.
 
 std::vector<ZonesOutput> Zout = Zones(GroupedHits);
 

  ///////////////////////////////
  ///// Pattern Recognition /////  Applies pattern recognition logic on each of the 3 BX groups and assigns a quality to each keystrip in the zone.
  ///// & quality assinment /////  The delete duplicate patterns function looks at the 3 BX groups and deletes duplicate patterns found from the
  ///////////////////////////////  same hits. This is where the BX analysis ends; Only 1 list of found patterns is given to the next module.


  std::vector<PatternOutput> Pout = Patterns(Zout);
  std::vector<PatternOutput> Pout_Hold = Pout;

  //PatternOutput Test = DeleteDuplicatePatterns(Pout);

  //PrintQuality(Test.detected);


  ///////////////////////////////
  //////Sector Sorting/////////// Sorts through the patterns found in each zone and selects the best three per zone to send to the next module.
  ///////Finding 3 Best Pattern//
  ///////////////////////////////

  //SortingOutput Sout = SortSect(Test);
  
  std::vector<SortingOutput> Sout_Hold = SortSect_Hold(Pout_Hold);

  //////////////////////////////////
  ///////// Match ph patterns ////// Loops over each sorted pattern and then loops over all possible triggerprimitives which could have made the pattern
  ////// to segment inputs ///////// and matches the associated full precision triggerprimitives to the detected pattern.
  //////////////////////////////////
  
  //MatchingOutput Mout = PhiMatching(Sout);
  
  std::vector<MatchingOutput> Mout_Hold = PhiMatching_Hold(Sout_Hold);
 // MO[SectIndex] = Mout;

  /////////////////////////////////
  //////// Calculate delta //////// Once we have matched the hits we calculate the delta phi and theta between all
  ////////    ph and th    //////// stations present.
  /////////////////////////////////

 //std::vector<std::vector<DeltaOutput>> Dout = CalcDeltas(Mout);////
 
 std::vector<std::vector<std::vector<DeltaOutput>>> Dout_Hold = CalcDeltas_Hold(Mout_Hold);

  /////////////////////////////////
  /////// Sorts and gives /////////  Loops over all of the found tracks(looking across zones) and selects the best three per sector.
  ////// Best 3 tracks/sector /////  Here ghost busting is done to delete tracks which are comprised of the same associated stubs.
  /////////////////////////////////

 // std::vector<BTrack> Bout = BestTracks(Dout);
  
  std::vector<std::vector<BTrack>> Bout_Hold = BestTracks_Hold(Dout_Hold);
  
  // PTracks[SectIndex] = Bout;
   for(int bx=0;bx<3;bx++){
   		PTracks_BX[SectIndex][bx] = Bout_Hold[bx];
	}

   
 } // End loop: for(int SectIndex=0;SectIndex<NUM_SECTORS;SectIndex++) 

 ///////////////////////////////////////
 /// Collect Muons from all sectors //// and BX windows 
 /////////////////////////////////////// 
										
 std::vector<BTrack> PTemp[NUM_SECTORS];
 std::vector<BTrack> AllTracks, AllTracks_PreDuplicationCancellation;
 for (int i=0; i<NUM_SECTORS; i++) PTemp[i] = PTracks[i];
 

for(int bx=0;bx<3;bx++){
 	for(int j=0;j<36;j++){


			//if(PTemp[j/3][j%3].phi)//no track
			//	AllTracks.push_back(PTemp[j/3][j%3]);
			
			if(PTracks_BX[j/3][bx][j%3].phi)//no track
				AllTracks_PreDuplicationCancellation.push_back(PTracks_BX[j/3][bx][j%3]);
				
 	}
}

for(unsigned int i1=0;i1<AllTracks_PreDuplicationCancellation.size();i1++){

	bool dup = false;
	
	int ebx = 20, sindex = -1;
	for(std::vector<ConvertedHit>::iterator A = AllTracks_PreDuplicationCancellation[i1].AHits.begin();A != AllTracks_PreDuplicationCancellation[i1].AHits.end();A++){
		if(A->TP().getCSCData().bx < ebx){
			ebx = A->TP().getCSCData().bx;
		}
		sindex = A->SectorIndex();
	}

	for(unsigned int i2=i1+1;i2<AllTracks_PreDuplicationCancellation.size();i2++){
		
		int ebx2 = 20, sindex2 = -1;
		for(std::vector<ConvertedHit>::iterator A2 = AllTracks_PreDuplicationCancellation[i1].AHits.begin();A2 != AllTracks_PreDuplicationCancellation[i1].AHits.end();A2++){
			if(A2->TP().getCSCData().bx < ebx2){
				ebx2 = A2->TP().getCSCData().bx;
			}
			sindex2 = A2->SectorIndex();
		}
		
		if(ebx == ebx2 && AllTracks_PreDuplicationCancellation[i1].theta == AllTracks_PreDuplicationCancellation[i2].theta &&
		   AllTracks_PreDuplicationCancellation[i1].phi == AllTracks_PreDuplicationCancellation[i2].phi && AllTracks_PreDuplicationCancellation[i1].winner.Rank() == AllTracks_PreDuplicationCancellation[i2].winner.Rank() &&
		   sindex == sindex2 && sindex != -1 && sindex2 != -1){dup = true;}
		
	}
	
	if(!dup)
		AllTracks.push_back(AllTracks_PreDuplicationCancellation[i1]);
}

  ///////////////////////////////////
  /// Make Internal track if ////////
  /////// tracks are found //////////
  /////////////////////////////////// 
  
  std::vector<l1t::RegionalMuonCand> tester1;
  std::vector<std::pair<int,l1t::RegionalMuonCand>> holder, holder2;

  for(unsigned int fbest=0;fbest<AllTracks.size();fbest++){

  	if(AllTracks[fbest].phi){

		InternalTrack tempTrack;
  		tempTrack.setType(2);
		tempTrack.phi = AllTracks[fbest].phi;
		tempTrack.theta = AllTracks[fbest].theta;
		tempTrack.rank = AllTracks[fbest].winner.Rank();
		tempTrack.deltas = AllTracks[fbest].deltas;
		std::vector<int> ps, ts;

		l1t::EMTFTrackExtra thisTrack;
		thisTrack.set_phi_loc_int ( AllTracks[fbest].phi           );
		thisTrack.set_theta_int   ( AllTracks[fbest].theta         );
		thisTrack.set_rank        ( AllTracks[fbest].winner.Rank() );
		// thisTrack.set_deltas        ( AllTracks[fbest].deltas        );
		int tempStraightness = 0;
		int tempRank = thisTrack.Rank();
		if (tempRank & 64)
		  tempStraightness |= 4;
		if (tempRank & 16)
		  tempStraightness |= 2;
		if (tempRank & 4)
		  tempStraightness |= 1;
		thisTrack.set_straightness ( tempStraightness );


		int sector = -1;
		bool ME13 = false;
		int me1address = 0, me2address = 0, CombAddress = 0, mode_uncorr = 0;
		int ebx = 20, sebx = 20;
		int phis[4] = {-99,-99,-99,-99};


		for(std::vector<ConvertedHit>::iterator A = AllTracks[fbest].AHits.begin();A != AllTracks[fbest].AHits.end();A++){

			if(A->Phi() != -999){
			  
			        l1t::EMTFHitExtra thisHit;
			        // thisHit.ImportCSCDetId( A->TP().detId<CSCDetId>() );

				for (uint iHit = 0; iHit < OutputHits->size(); iHit++) {
				  if ( A->TP().detId<CSCDetId>().station() == OutputHits->at(iHit).Station() and
				       A->TP().getCSCData().cscID          == OutputHits->at(iHit).CSC_ID()  and
				       A->Wire()                           == OutputHits->at(iHit).Wire()    and
				       A->Strip()                          == OutputHits->at(iHit).Strip()   and
				       A->TP().getCSCData().bx - 6         == OutputHits->at(iHit).BX() ) {
				    thisHit = OutputHits->at(iHit);
				    thisTrack.push_HitExtraIndex(iHit);
				    thisTrack.push_HitExtra(thisHit); // Done before theta windows are applied ... how can we do it after? - AWB 29.04.16
				    break;
				  }
				}
				thisTrack.set_endcap       ( thisHit.Endcap() );
				thisTrack.set_sector_index ( thisHit.Sector_index() );
				thisTrack.set_sector       ( l1t::calc_sector_from_index( thisHit.Sector_index() ) );
				thisTrack.set_sector_GMT   ( l1t::calc_sector_GMT( thisHit.Sector() ) );

				int station = A->TP().detId<CSCDetId>().station();
				int id = A->TP().getCSCData().cscID;
				int trknm = A->TP().getCSCData().trknmb;
				
				phis[station-1] = A->Phi();
				
				
				if(A->TP().getCSCData().bx < ebx){
					sebx = ebx;
					ebx = A->TP().getCSCData().bx;
				}
				else if(A->TP().getCSCData().bx < sebx){
					sebx = A->TP().getCSCData().bx;
				}

				tempTrack.addStub(A->TP());
				ps.push_back(A->Phi());
				ts.push_back(A->Theta());
				
				sector = A->SectorIndex();//(A->TP().detId<CSCDetId>().endcap() -1)*6 + A->TP().detId<CSCDetId>().triggerSector() - 1;
				//std::cout<<"Q: "<<A->Quality()<<", keywire: "<<A->Wire()<<", strip: "<<A->Strip()<<std::endl;

				switch(station){
					case 1: mode_uncorr |= 8;break;
					case 2: mode_uncorr |= 4;break;
					case 3: mode_uncorr |= 2;break;
					case 4: mode_uncorr |= 1;break;
					default: mode_uncorr |= 0;
				}
				
				_UpgradedQual[_nUpgradedTracks][station-1] = A->Quality();
				_UpgradedPatt[_nUpgradedTracks][station-1] = A->Pattern();
				_UpgradedBX_station[_nUpgradedTracks][station-1] = A->TP().getCSCData().bx;


				if(A->TP().detId<CSCDetId>().station() == 1 && A->TP().detId<CSCDetId>().ring() == 3)
					ME13 = true;

				if(station == 1 && id > 3 && id < 7){

					int sub = 2;
					if(A->TP().detId<CSCDetId>().chamber()%6 > 2)
						sub = 1;

					me1address = id;
					me1address -= 3;
					me1address += 3*(sub - 1);
					me1address = id<<1;//
					me1address |= trknm-1;

				}

				if(station == 2 && id > 3){

					me2address = id;
					me2address -= 3;
					me2address = me2address<<1;
					me2address |= trknm-1;

				}


			}

		}
		
		int mode = 0;
		if(tempTrack.rank & 32)
			mode |= 8;
		if(tempTrack.rank & 8)
			mode |= 4;
		if(tempTrack.rank & 2)
			mode |= 2;
		if(tempTrack.rank & 1)
			mode |= 1;

		tempTrack.phis = ps;
		tempTrack.thetas = ts;
		tempTrack.deltas = AllTracks[fbest].deltas;

		// // Before Mulhearn cleanup, May 11
		// unsigned long xmlpt_address = 0;
		// float xmlpt = CalculatePt(tempTrack, es, mode, &xmlpt_address);
		
		// After Mulhearn cleanup, May 11
		unsigned long xmlpt_address = ptAssignment_.calculateAddress(tempTrack, es, mode);
		//std::cout<<"address = "<<xmlpt_address<<"\n";
		
		
		float xmlpt = ptAssignment_.calculatePt(xmlpt_address);
		
		
		

		tempTrack.pt = xmlpt*1.4;
		
		//FoundTracks->push_back(tempTrack);

		CombAddress = (me2address<<4) | me1address;

		int charge = getCharge(phis[0],phis[1],phis[2],phis[3],mode);

		
								 
		bool readFromLUT = true;
		unsigned long FW_address = 0;
		if(readFromLUT){
			
			FW_address = ptAssignment_.calculateAddress_FW(tempTrack,es,mode);
			tempTrack.pt = dPtLUT[FW_address];
			//std::cout<<"pt = "<<dPtLUT[FW_address]<<"\n";
			
		}
		
		//l1t::RegionalMuonCand outCand = MakeRegionalCand(xmlpt*1.4,AllTracks[fbest].phi,AllTracks[fbest].theta,
		//						 charge,mode,CombAddress,sector);
								 
		l1t::RegionalMuonCand outCand = MakeRegionalCand(tempTrack.pt,AllTracks[fbest].phi,AllTracks[fbest].theta,
								 charge,mode,CombAddress,sector);						 

		float theta_angle = l1t::calc_theta_rad_from_int( AllTracks[fbest].theta ); 
		float eta = l1t::calc_eta_from_theta_rad( theta_angle );
		if(sector > 5)
			eta *= -1;
		

		thisTrack.set_phi_loc_deg  ( l1t::calc_phi_loc_deg( thisTrack.Phi_loc_int() ) );
		thisTrack.set_phi_loc_rad  ( l1t::calc_phi_loc_rad( thisTrack.Phi_loc_int() ) );
		thisTrack.set_phi_glob_deg ( l1t::calc_phi_glob_deg( thisTrack.Phi_loc_deg(), thisTrack.Sector() ) );
		thisTrack.set_phi_glob_rad ( l1t::calc_phi_glob_rad( thisTrack.Phi_loc_rad(), thisTrack.Sector() ) );
		thisTrack.set_quality    ( outCand.hwQual());
		thisTrack.set_mode       ( mode            ); 
		thisTrack.set_first_bx   ( ebx - 6         ); 
		thisTrack.set_second_bx  ( sebx - 6        ); 
		thisTrack.set_bx         ( thisTrack.First_BX() );
		thisTrack.set_phis       ( ps              );
		thisTrack.set_thetas     ( ts              );
		thisTrack.set_pt         ( xmlpt*1.4       );
		thisTrack.set_pt_XML     ( xmlpt           );
		thisTrack.set_pt_LUT_addr( xmlpt_address   );
		thisTrack.set_charge     ( (charge == 1) ? -1 : 1 ); // uGMT uses opposite of physical charge (to match pdgID)
		thisTrack.set_charge_GMT ( charge          );
		thisTrack.set_theta_rad  ( theta_angle     );
		thisTrack.set_theta_deg  ( theta_angle * 180/Geom::pi() );
		thisTrack.set_eta        ( eta  * thisTrack.Endcap() );
		thisTrack.set_pt_GMT     ( outCand.hwPt()  ); 
		thisTrack.set_phi_GMT    ( outCand.hwPhi() );
		thisTrack.set_eta_GMT    ( outCand.hwEta() );

		thisTrack.ImportPtLUT    ( thisTrack.Mode(), thisTrack.Pt_LUT_addr() );

		// thisTrack.phi_loc_rad(); // Need to implement - AWB 04.04.16
		// thisTrack.phi_glob_rad(); // Need to implement - AWB 04.04.16

 		// // Optimal emulator configuration - AWB 29.03.16
		// std::pair<int,l1t::RegionalMuonCand> outPair(sebx,outCand);

		// Actual setting in firmware - AWB 12.04.16
		std::pair<int,l1t::RegionalMuonCand> outPair(ebx,outCand);

		if(!ME13 && fabs(eta) > 1.1) {
		  // // Extra debugging output - AWB 29.03.16
		 //std::cout << "\nInput: eBX = " << ebx << ", seBX = " << sebx << ", pt = " << xmlpt*1.4 
		 // << ", phi = " << AllTracks[fbest].phi << ", eta = " << eta 
		 // << ", theta = " << AllTracks[fbest].theta << ", sign = " << 1 
		//  << ", quality = " << mode << ", trackaddress = " << 1 
		 // << ", sector = " << sector << std::endl;
		 //std::cout << "Output: BX = " << ebx << ", hwPt = " << tempTrack.pt << ", hwPhi = " << outCand.hwPhi() 
		 // << ", hwEta = " << outCand.hwEta() << ", hwSign = " << outCand.hwSign() 
		 // << ", hwQual = " << outCand.hwQual() << ", link = " << outCand.link()
		 // << ", processor = " << outCand.processor() 
		 // << ", trackFinderType = " << outCand.trackFinderType() << ", address = "<<FW_address<<"\n\n";
			holder.push_back(outPair);
			thisTrack.set_isGMT( 1 );
			
			float gpd = (AllTracks[fbest].phi*107.01/4096) + sector*60 + 13.0;
			float gpr = gpd*(3.14159265359/180.0);
			if(gpr > 3.14159)
				gpr -= 6.28318;
		
		
			_UpgradedPt[_nUpgradedTracks] = xmlpt*1.4;
			_UpgradedPt2[_nUpgradedTracks] = tempTrack.pt;
			//std::cout<<"Up pT = "<<tempTrack.pt<<", eta = "<<eta<<" and phi = "<<gpr<<"\n";
			_UpgradedEta[_nUpgradedTracks] = eta;
			_UpgradedPhi[_nUpgradedTracks] = gpr;
			_UpgradedMode[_nUpgradedTracks] = mode;
			_UpgradedMode2[_nUpgradedTracks] = mode;
			_UpgradedRank[_nUpgradedTracks] = tempTrack.rank;
			_nUpgradedTracks++;
			
			if(outCand.hwEta() == -240 || outCand.hwEta() == 239)
				std::cout<<"interesting event "<<ev.id().event()<<"\n";
		}
		OutputTracks->push_back( thisTrack );
		OutTracks->push_back( thisTrack.CreateEMTFTrack() );
	}
  }
  
OutputCands->setBXRange(-2,2);

for(int sect=0;sect<12;sect++){

	for(unsigned int h=0;h<holder.size();h++){
	
		int bx = holder[h].first - 6;
		int sector = holder[h].second.processor();
		if(holder[h].second.trackFinderType() == 3)
			sector += 6;
	
		if(sector == sect){
			OutputCands->push_back(bx,holder[h].second);
		}
		
	}
}


//ev.put( FoundTracks, "DataITC");
ev.put( OutputCands, "EMTF");
 ev.put( OutputHits, "EMTF"); 
 ev.put( OutputTracks, "EMTF");
  outputTree->Fill();
  //std::cout<<"End Upgraded Track Finder Prducer:::::::::::::::::::::::::::\n:::::::::::::::::::::::::::::::::::::::::::::::::\n\n";

}//analyzer

void L1TMuonEndCapTrackProducer::beginJob()
{


	GenInfo = fopen("FuckInfo.txt","w");
	LCTInfo = fopen("LCTInfo.txt","w");
	
	std::ifstream PtLUT("LUT_AndrewFix_25July16.dat");

  ULong64_t FullWord = 0;
  ULong64_t SubWord[4] = {0, 0, 0, 0};

  int Address;
  //int MaxAddress = 1<<30;
  int MaxAdd4 = (1<<30)/4 ; 


  // loop over LUT address space, 4 addresses at a time
  for (int add4=0; add4<MaxAdd4; add4++) {
    
    PtLUT.read(reinterpret_cast<char*>(&FullWord), sizeof(ULong64_t));

    SubWord [0] = FullWord & 0x1FF;
    SubWord [1] = (FullWord>>9) & 0x1FF;
    SubWord [2] = (FullWord>>32) & 0x1FF;
    SubWord [3] = (FullWord>>(32+9)) & 0x1FF;

    for (int i=0; i<4; i++) {
      Address = add4*4 + i;

      // Store LUT in memory
      dPtLUT[Address] = SubWord[i];

    }

  }

	FullPhi = fs->make<TH1F>("FullPhi","",6000,-500,5500);
	FullTheta = fs->make<TH1F>("FullTheta","",150,0,150);
	CLCT = fs->make<TH1F>("CLCT","",20,-10,10);
	FullZ = fs->make<TH1F>("FullZ","",200,0,200);
	
	outputTree = fs->make<TTree>("EmuTree","EmuTree");
	
	outputTree->Branch("_triggerSector", &_triggerSector, "_triggerSector[30]/I");
	
	outputTree->Branch("_nPU", &_nPU, "_nPU/I");
	
	outputTree->Branch("_nGenMuons", &_nGenMuons, "_nGenMuons/I");
	outputTree->Branch("_genPt", &_genPt, "_genPt[30]/D");
	outputTree->Branch("_genEta", &_genEta, "_genEta[30]/D");
	outputTree->Branch("_genPhi", &_genPhi, "_genPhi[30]/D");
	outputTree->Branch("_genCharge", &_genCharge, "_genCharge[30]/I");
	
	outputTree->Branch("_nUpgradedTracks", &_nUpgradedTracks, "_nUpgradedTracks/I");
	outputTree->Branch("_UpgradedPt", &_UpgradedPt, "_UpgradedPt[30]/D");
	outputTree->Branch("_UpgradedPt2", &_UpgradedPt2, "_UpgradedPt2[30]/D");
	outputTree->Branch("_UpgradedEta", &_UpgradedEta, "_UpgradedEta[30]/D");
	outputTree->Branch("_UpgradedPhi", &_UpgradedPhi, "_UpgradedPhi[30]/D");
	outputTree->Branch("_UpgradedMode", &_UpgradedMode, "_UpgradedMode[30]/I");
	outputTree->Branch("_UpgradedMode2", &_UpgradedMode2, "_UpgradedMode2[30]/I");
	outputTree->Branch("_UpgradedPatt", &_UpgradedPatt, "_UpgradedPatt[30][4]/I");
	outputTree->Branch("_UpgradedQual", &_UpgradedQual, "_UpgradedQual[30][4]/I");
	outputTree->Branch("_UpgradedRank", &_UpgradedRank, "_UpgradedRank[30]/I");
	outputTree->Branch("_UpgradedBX", &_UpgradedBX, "_UpgradedBX[30]/I");
	outputTree->Branch("_UpgradedBX_station", &_UpgradedBX_station, "_UpgradedBX_station[30][4]/I");
	outputTree->Branch("_dPhi12", &_dPhi12, "_dPhi12[30]/I");
	outputTree->Branch("_dPhi13", &_dPhi13, "_dPhi13[30]/I");
	outputTree->Branch("_dPhi14", &_dPhi14, "_dPhi14[30]/I");
	outputTree->Branch("_dPhi23", &_dPhi23, "_dPhi23[30]/I");
	outputTree->Branch("_dPhi24", &_dPhi24, "_dPhi24[30]/I");
	outputTree->Branch("_dPhi34", &_dPhi34, "_dPhi34[30]/I");
	
	outputTree->Branch("_nLegacyTracks", &_nLegacyTracks, "_nLegacyTracks/I");
	outputTree->Branch("_LegacyPt", &_LegacyPt, "_LegacyPt[30]/D");
	outputTree->Branch("_LegacyEta", &_LegacyEta, "_LegacyEta[30]/D");
	outputTree->Branch("_LegacyPhi", &_LegacyPhi, "_LegacyPhi[30]/D");
	outputTree->Branch("_LegacyMode", &_LegacyMode, "_LegacyMode[30]/I");
	outputTree->Branch("_LegacyCharge", &_LegacyCharge, "_LegacyCharge[30]/I");
	
	outputTree->Branch("_nGMTTracks", &_nGMTTracks, "_nGMTTracks/I");
	outputTree->Branch("_GMTPt", &_GMTPt, "_GMTPt[30]/D");
	outputTree->Branch("_GMTEta", &_GMTEta, "_GMTEta[30]/D");
	outputTree->Branch("_GMTPhi", &_GMTPhi, "_GMTPhi[30]/D");
	outputTree->Branch("_GMTQual", &_GMTQual, "_GMTQual[30]/I");
	outputTree->Branch("_GMTRank", &_GMTRank, "_GMTRank[30]/I");
	outputTree->Branch("_GMTBx", &_GMTBx, "_GMTBx[30]/I");
	outputTree->Branch("_GMTCSC", &_GMTCSC, "_GMTCSC[30]/I");

}
void L1TMuonEndCapTrackProducer::endJob()
{

}
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(L1TMuonEndCapTrackProducer);
