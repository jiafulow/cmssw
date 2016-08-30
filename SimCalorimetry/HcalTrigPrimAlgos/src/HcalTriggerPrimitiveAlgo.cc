#include "SimCalorimetry/HcalTrigPrimAlgos/interface/HcalTriggerPrimitiveAlgo.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"


#include "DataFormats/HcalDetId/interface/HcalDetId.h"
#include "Geometry/HcalTowerAlgo/interface/HcalTrigTowerGeometry.h"
#include "DataFormats/HcalDetId/interface/HcalTrigTowerDetId.h"
#include "DataFormats/FEDRawData/interface/FEDNumbering.h"
#include "DataFormats/HcalDetId/interface/HcalElectronicsId.h"
#include "EventFilter/HcalRawToDigi/interface/HcalDCCHeader.h"
#include "EventFilter/HcalRawToDigi/interface/HcalHTRData.h"
#include "SimCalorimetry/HcalTrigPrimAlgos/interface/HcalFeatureHFEMBit.h"//cuts based on short and long energy deposited.
#include <iostream>
using namespace std;

HcalTriggerPrimitiveAlgo::HcalTriggerPrimitiveAlgo( bool pf, const std::vector<double>& w, int latency,
                                                    uint32_t FG_threshold, uint32_t FG_HF_threshold, uint32_t ZS_threshold,
                                                    int numberOfSamples, int numberOfPresamples,
                                                    int numberOfSamplesHF, int numberOfPresamplesHF,
                                                    uint32_t minSignalThreshold, uint32_t PMT_NoiseThreshold
                                                    )
                                                   : incoder_(0), outcoder_(0),
                                                   theThreshold(0), peakfind_(pf), weights_(w), latency_(latency),
                                                   FG_threshold_(FG_threshold), FG_HF_threshold_(FG_HF_threshold), ZS_threshold_(ZS_threshold),
                                                   numberOfSamples_(numberOfSamples),
                                                   numberOfPresamples_(numberOfPresamples),
                                                   numberOfSamplesHF_(numberOfSamplesHF),
                                                   numberOfPresamplesHF_(numberOfPresamplesHF),
                                                   minSignalThreshold_(minSignalThreshold),
                                                   PMT_NoiseThreshold_(PMT_NoiseThreshold),
                                                   NCTScaleShift(0), RCTScaleShift(0),
                                                   peak_finder_algorithm_(2)
{
   //No peak finding setting (for Fastsim)
   if (!peakfind_){
      numberOfSamples_ = 1; 
      numberOfPresamples_ = 0;
      numberOfSamplesHF_ = 1; 
      numberOfPresamplesHF_ = 0;
   }
   // Switch to integer for comparisons - remove compiler warning
   ZS_threshold_I_ = ZS_threshold_;
}


HcalTriggerPrimitiveAlgo::~HcalTriggerPrimitiveAlgo() {
}


void
HcalTriggerPrimitiveAlgo::setUpgradeFlags(bool hb, bool he, bool hf)
{
   upgrade_hb_ = hb;
   upgrade_he_ = he;
   upgrade_hf_ = hf;
}


void
HcalTriggerPrimitiveAlgo::overrideParameters(unsigned int hf_tdc_mask,
                                             unsigned int hf_adc_threshold,
                                             unsigned int hf_fg_threshold)
{
   auto parameters = new TPParameters();
   parameters->hf_tdc_mask = hf_tdc_mask;
   parameters->hf_adc_threshold = hf_adc_threshold;
   parameters->hf_fg_threshold = hf_fg_threshold;

   override_parameters_ = std::unique_ptr<const TPParameters>(parameters);
}


void HcalTriggerPrimitiveAlgo::addSignal(const HBHEDataFrame & frame) {
   // TODO: Need to add support for seperate 28, 29 in HE
   //Hack for 300_pre10, should be removed.
   if (frame.id().depth()==5) return;

   std::vector<HcalTrigTowerDetId> ids = theTrigTowerGeometry->towerIds(frame.id());
   assert(ids.size() == 1 || ids.size() == 2);
   IntegerCaloSamples samples1(ids[0], int(frame.size()));

   samples1.setPresamples(frame.presamples());
   incoder_->adc2Linear(frame, samples1);

   std::vector<bool> msb;
   incoder_->lookupMSB(frame, msb);

   if (abs(ids[0].ieta()) < first_he_tower and upgrade_hb_) {
      edm::LogError("HCALTPAlgo") << "Upgrade hb but received " << ids[0] << " (out of " << ids.size() << ", from " << frame.id() << ")";
   } else if (abs(ids[0].ieta()) >= first_he_tower and upgrade_he_) {
      edm::LogError("HCALTPAlgo") << "Upgrade he but received " << ids[0] << " (out of " << ids.size() << ", from " << frame.id() << ")";
   }

   if(ids.size() == 2) {
      // make a second trigprim for the other one, and split the energy
      IntegerCaloSamples samples2(ids[1], samples1.size());
      for(int i = 0; i < samples1.size(); ++i) {
         samples1[i] = uint32_t(samples1[i]*0.5);
         samples2[i] = samples1[i];
      }
      samples2.setPresamples(frame.presamples());
      addSignal(samples2);
      addFG(ids[1], msb);
   }
   addSignal(samples1);
   addFG(ids[0], msb);
}


void HcalTriggerPrimitiveAlgo::addSignal(const HFDataFrame & frame) {
   if(frame.id().depth() == 1 || frame.id().depth() == 2) {
      std::vector<HcalTrigTowerDetId> ids = theTrigTowerGeometry->towerIds(frame.id());
      std::vector<HcalTrigTowerDetId>::const_iterator it;
      for (it = ids.begin(); it != ids.end(); ++it) {
         HcalTrigTowerDetId trig_tower_id = *it;
         IntegerCaloSamples samples(trig_tower_id, frame.size());
         samples.setPresamples(frame.presamples());
         incoder_->adc2Linear(frame, samples);

         // Don't add to final collection yet
         // HF PMT veto sum is calculated in analyzerHF()
         IntegerCaloSamples zero_samples(trig_tower_id, frame.size());
         zero_samples.setPresamples(frame.presamples());
         addSignal(zero_samples);

         // Pre-LS1 Configuration
         if (trig_tower_id.version() == 0) {
            // Mask off depths: fgid is the same for both depths
            uint32_t fgid = (frame.id().maskDepth());

            if ( theTowerMapFGSum.find(trig_tower_id) == theTowerMapFGSum.end() ) {
               SumFGContainer sumFG;
               theTowerMapFGSum.insert(std::pair<HcalTrigTowerDetId, SumFGContainer>(trig_tower_id, sumFG));
            }

            SumFGContainer& sumFG = theTowerMapFGSum[trig_tower_id];
            SumFGContainer::iterator sumFGItr;
            for ( sumFGItr = sumFG.begin(); sumFGItr != sumFG.end(); ++sumFGItr) {
               if (sumFGItr->id() == fgid) { break; }
            }
            // If find
            if (sumFGItr != sumFG.end()) {
               for (int i=0; i<samples.size(); ++i) {
                  (*sumFGItr)[i] += samples[i];
               }
            }
            else {
               //Copy samples (change to fgid)
               IntegerCaloSamples sumFGSamples(DetId(fgid), samples.size());
               sumFGSamples.setPresamples(samples.presamples());
               for (int i=0; i<samples.size(); ++i) {
                  sumFGSamples[i] = samples[i];
               }
               sumFG.push_back(sumFGSamples);
            }

            // set veto to true if Long or Short less than threshold
            if (HF_Veto.find(fgid) == HF_Veto.end()) {
               vector<bool> vetoBits(samples.size(), false);
               HF_Veto[fgid] = vetoBits;
            }
            for (int i=0; i<samples.size(); ++i) {
               if (samples[i] < minSignalThreshold_) {
                  HF_Veto[fgid][i] = true;
               }
            }
         }
         // HF 1x1
         else if (trig_tower_id.version() == 1) {
            uint32_t fgid = (frame.id().maskDepth());
            HFDetails& details = theHFDetailMap[trig_tower_id][fgid];
            // Check the frame type to determine long vs short
            if (frame.id().depth() == 1) { // Long
               details.long_fiber = samples;
               details.LongDigi = frame;
            } else if (frame.id().depth() == 2) { // Short
               details.short_fiber = samples;
               details.ShortDigi = frame;
            } else {
                // Neither long nor short... So we have no idea what to do
                edm::LogWarning("HcalTPAlgo") << "Unable to figure out what to do with data frame for " << frame.id();
                return;
            }
         }
         // Uh oh, we are in a bad/unknown state! Things will start crashing.
         else {
             return;
         }
      }
   }
}

void
HcalTriggerPrimitiveAlgo::addSignal(const QIE10DataFrame& frame)
{
   auto ids = theTrigTowerGeometry->towerIds(frame.id());
   for (const auto& id: ids) {
      if (id.version() == 0) {
         edm::LogError("HcalTPAlgo") << "Encountered QIE10 data frame mapped to TP version 0:" << id;
         continue;
      }

      IntegerCaloSamples samples(id, frame.samples());
      samples.setPresamples(frame.presamples());
      incoder_->adc2Linear(frame, samples);

      // Don't add to final collection yet
      // HF PMT veto sum is calculated in analyzerHF()
      IntegerCaloSamples zero_samples(id, frame.samples());
      zero_samples.setPresamples(frame.presamples());
      addSignal(zero_samples);

      auto fid = HcalDetId(frame.id());
      auto& details = theHFUpgradeDetailMap[id][fid.maskDepth()];
      details[fid.depth() - 1].samples = samples;
      details[fid.depth() - 1].digi = frame;
   }
}

void
HcalTriggerPrimitiveAlgo::addSignal(const QIE11DataFrame& frame)
{
   HcalDetId detId(frame.id());
   std::vector<HcalTrigTowerDetId> ids = theTrigTowerGeometry->towerIds(detId);
   assert(ids.size() == 1 || ids.size() == 2);
   IntegerCaloSamples samples1(ids[0], int(frame.samples()));

   samples1.setPresamples(frame.presamples());
   incoder_->adc2Linear(frame, samples1);

   std::vector<std::bitset<2>> msb(frame.samples(), 0);
   incoder_->lookupMSB(frame, msb);

   if (abs(ids[0].ieta()) < first_he_tower and not upgrade_hb_) {
      edm::LogError("HCALTPAlgo") << "No upgrade hb but received " << ids[0] << " (" << ids.size() << ")";
   } else if (abs(ids[0].ieta()) >= first_he_tower and not upgrade_he_) {
      edm::LogError("HCALTPAlgo") << "No upgrade he but received " << ids[0] << " (" << ids.size() << ")";
   }

   if(ids.size() == 2) {
      // make a second trigprim for the other one, and split the energy
      IntegerCaloSamples samples2(ids[1], samples1.size());
      for(int i = 0; i < samples1.size(); ++i) {
         samples1[i] = uint32_t(samples1[i]*0.5);
         samples2[i] = samples1[i];
      }
      samples2.setPresamples(frame.presamples());
      addSignal(samples2);
      addUpgradeFG(ids[1], detId.depth(), msb);
   }
   addSignal(samples1);
   addUpgradeFG(ids[0], detId.depth(), msb);
}

void HcalTriggerPrimitiveAlgo::addSignal(const IntegerCaloSamples & samples) {
   HcalTrigTowerDetId id(samples.id());
   SumMap::iterator itr = theSumMap.find(id);
   if(itr == theSumMap.end()) {
      theSumMap.insert(std::make_pair(id, samples));
   }
   else {
      // wish CaloSamples had a +=
      for(int i = 0; i < samples.size(); ++i) {
         (itr->second)[i] += samples[i];
      }
   }
}


void HcalTriggerPrimitiveAlgo::analyze(IntegerCaloSamples & samples, HcalTriggerPrimitiveDigi & result) {
   int shrink = weights_.size() - 1;
   std::vector<bool>& msb = fgMap_[samples.id()];
   IntegerCaloSamples sum(samples.id(), samples.size());

   //slide algo window
   for(int ibin = 0; ibin < int(samples.size())- shrink; ++ibin) {
      int algosumvalue = 0;
      for(unsigned int i = 0; i < weights_.size(); i++) {
         //add up value * scale factor
         algosumvalue += int(samples[ibin+i] * weights_[i]);
      }
      if (algosumvalue<0) sum[ibin]=0;            // low-side
                                                  //high-side
      //else if (algosumvalue>QIE8_LINEARIZATION_ET) sum[ibin]=QIE8_LINEARIZATION_ET;
      else sum[ibin] = algosumvalue;              //assign value to sum[]
   }

   // Align digis and TP
   int dgPresamples=samples.presamples(); 
   int tpPresamples=numberOfPresamples_;
   int shift = dgPresamples - tpPresamples;
   int dgSamples=samples.size();
   int tpSamples=numberOfSamples_;
   if(peakfind_){
       if((shift<shrink) || (shift + tpSamples + shrink > dgSamples - (peak_finder_algorithm_ - 1) )   ){
	    edm::LogInfo("HcalTriggerPrimitiveAlgo::analyze") << 
		"TP presample or size from the configuration file is out of the accessible range. Using digi values from data instead...";
	    shift=shrink;
	    tpPresamples=dgPresamples-shrink;
	    tpSamples=dgSamples-(peak_finder_algorithm_-1)-shrink-shift;
       }
   }

   std::vector<int> finegrain(tpSamples,false);

   IntegerCaloSamples output(samples.id(), tpSamples);
   output.setPresamples(tpPresamples);

   for (int ibin = 0; ibin < tpSamples; ++ibin) {
      // ibin - index for output TP
      // idx - index for samples + shift
      int idx = ibin + shift;

      //Peak finding
      if (peakfind_) {
         bool isPeak = false;
         switch (peak_finder_algorithm_) {
            case 1 :
               isPeak = (samples[idx] > samples[idx-1] && samples[idx] >= samples[idx+1] && samples[idx] > theThreshold);
               break;
            case 2:
               isPeak = (sum[idx] > sum[idx-1] && sum[idx] >= sum[idx+1] && sum[idx] > theThreshold);
               break;
            default:
               break;
         }

         if (isPeak){
            output[ibin] = std::min<unsigned int>(sum[idx],QIE8_LINEARIZATION_ET);
            finegrain[ibin] = msb[idx];
         }
         // Not a peak
         else output[ibin] = 0;
      }
      else { // No peak finding, just output running sum
         output[ibin] = std::min<unsigned int>(sum[idx],QIE8_LINEARIZATION_ET);
         finegrain[ibin] = msb[idx];
      }

      // Only Pegged for 1-TS algo.
      if (peak_finder_algorithm_ == 1) {
         if (samples[idx] >= QIE8_LINEARIZATION_ET)
            output[ibin] = QIE8_LINEARIZATION_ET;
      }
   }
   outcoder_->compress(output, finegrain, result);
}


void
HcalTriggerPrimitiveAlgo::analyze2017(IntegerCaloSamples& samples, HcalTriggerPrimitiveDigi& result, const HcalFinegrainBit& fg_algo)
{
   int shrink = weights_.size() - 1;
   auto& msb = fgUpgradeMap_[samples.id()];
   IntegerCaloSamples sum(samples.id(), samples.size());

   //slide algo window
   for(int ibin = 0; ibin < int(samples.size())- shrink; ++ibin) {
      int algosumvalue = 0;
      for(unsigned int i = 0; i < weights_.size(); i++) {
         //add up value * scale factor
         algosumvalue += int(samples[ibin+i] * weights_[i]);
      }
      if (algosumvalue<0) sum[ibin]=0;            // low-side
                                                  //high-side
      //else if (algosumvalue>QIE11_LINEARIZATION_ET) sum[ibin]=QIE11_LINEARIZATION_ET;
      else sum[ibin] = algosumvalue;              //assign value to sum[]
   }

   // Align digis and TP
   int dgPresamples=samples.presamples(); 
   int tpPresamples=numberOfPresamples_;
   int shift = dgPresamples - tpPresamples;
   int dgSamples=samples.size();
   int tpSamples=numberOfSamples_;

   if((shift<shrink) || (shift + tpSamples + shrink > dgSamples - (peak_finder_algorithm_ - 1) )   ){
      edm::LogInfo("HcalTriggerPrimitiveAlgo::analyze") << 
         "TP presample or size from the configuration file is out of the accessible range. Using digi values from data instead...";
      shift=shrink;
      tpPresamples=dgPresamples-shrink;
      tpSamples=dgSamples-(peak_finder_algorithm_-1)-shrink-shift;
   }

   std::vector<int> finegrain(tpSamples,false);

   IntegerCaloSamples output(samples.id(), tpSamples);
   output.setPresamples(tpPresamples);

   for (int ibin = 0; ibin < tpSamples; ++ibin) {
      // ibin - index for output TP
      // idx - index for samples + shift
      int idx = ibin + shift;
      bool isPeak = (sum[idx] > sum[idx-1] && sum[idx] >= sum[idx+1] && sum[idx] > theThreshold);

      if (isPeak){
         output[ibin] = std::min<unsigned int>(sum[idx],QIE11_LINEARIZATION_ET);
         finegrain[ibin] = fg_algo.compute(msb[idx]).to_ulong();
      } else {
         // Not a peak
         output[ibin] = 0;
         finegrain[ibin] = 0;
      }
   }
   outcoder_->compress(output, finegrain, result);
}


void HcalTriggerPrimitiveAlgo::analyzeHF(IntegerCaloSamples & samples, HcalTriggerPrimitiveDigi & result, const int hf_lumi_shift) {
   HcalTrigTowerDetId detId(samples.id());

   // Align digis and TP
   int dgPresamples=samples.presamples(); 
   int tpPresamples=numberOfPresamplesHF_;
   int shift = dgPresamples - tpPresamples;
   int dgSamples=samples.size();
   int tpSamples=numberOfSamplesHF_;
   if(shift<0 || shift+tpSamples>dgSamples){
	edm::LogInfo("HcalTriggerPrimitiveAlgo::analyzeHF") << 
	    "TP presample or size from the configuration file is out of the accessible range. Using digi values from data instead...";
	tpPresamples=dgPresamples;
	shift=0;
	tpSamples=dgSamples;
   }

   std::vector<int> finegrain(tpSamples, false);

   TowerMapFGSum::const_iterator tower2fg = theTowerMapFGSum.find(detId);
   assert(tower2fg != theTowerMapFGSum.end());

   const SumFGContainer& sumFG = tower2fg->second;
   // Loop over all L+S pairs that mapped from samples.id()
   // Note: 1 samples.id() = 6 x (L+S) without noZS
   for (SumFGContainer::const_iterator sumFGItr = sumFG.begin(); sumFGItr != sumFG.end(); ++sumFGItr) {
      const std::vector<bool>& veto = HF_Veto[sumFGItr->id().rawId()];
      for (int ibin = 0; ibin < tpSamples; ++ibin) {
         int idx = ibin + shift;
         // if not vetod, add L+S to total sum and calculate FG
	 bool vetoed = idx<int(veto.size()) && veto[idx];
         if (!(vetoed && (*sumFGItr)[idx] > PMT_NoiseThreshold_)) {
            samples[idx] += (*sumFGItr)[idx];
            finegrain[ibin] = (finegrain[ibin] || (*sumFGItr)[idx] >= FG_threshold_);
         }
      }
   }

   IntegerCaloSamples output(samples.id(), tpSamples);
   output.setPresamples(tpPresamples);

   for (int ibin = 0; ibin < tpSamples; ++ibin) {
      int idx = ibin + shift;
      output[ibin] = samples[idx] >> hf_lumi_shift;
      static const int MAX_OUTPUT = QIE8_LINEARIZATION_ET;  // QIE8_LINEARIZATION_ET = 1023
      if (output[ibin] > MAX_OUTPUT) output[ibin] = MAX_OUTPUT;
   }
   outcoder_->compress(output, finegrain, result);
}

void HcalTriggerPrimitiveAlgo::analyzeHF2016(
        const IntegerCaloSamples& SAMPLES,
        HcalTriggerPrimitiveDigi& result,
        const int HF_LUMI_SHIFT,
        const HcalFeatureBit* HCALFEM
        ) {
    // Align digis and TP
    const int SHIFT = SAMPLES.presamples() - numberOfPresamples_;
    assert(SHIFT >= 0);
    assert((SHIFT + numberOfSamples_) <= SAMPLES.size());

    // Try to find the HFDetails from the map corresponding to our samples
    const HcalTrigTowerDetId detId(SAMPLES.id());
    HFDetailMap::const_iterator it = theHFDetailMap.find(detId);
    // Missing values will give an empty digi
    if (it == theHFDetailMap.end()) {
        return;
    }

    std::vector<std::bitset<2>> finegrain(numberOfSamples_, false);

    // Set up out output of IntergerCaloSamples
    IntegerCaloSamples output(SAMPLES.id(), numberOfSamples_);
    output.setPresamples(numberOfPresamples_);

    for (const auto& item: it->second) {
        auto& details = item.second;
        for (int ibin = 0; ibin < numberOfSamples_; ++ibin) {
            const int IDX = ibin + SHIFT;
            int long_fiber_val = 0;
            if (IDX < details.long_fiber.size()) {
                long_fiber_val = details.long_fiber[IDX];
            }
            int short_fiber_val = 0;
            if (IDX < details.short_fiber.size()) {
                short_fiber_val = details.short_fiber[IDX];
            }
            output[ibin] += (long_fiber_val + short_fiber_val);

            uint32_t ADCLong = details.LongDigi[ibin].adc();
            uint32_t ADCShort = details.ShortDigi[ibin].adc();

            if (details.LongDigi.id().ietaAbs() != 29) {
               finegrain[ibin][1] = (ADCLong > FG_HF_threshold_ || ADCShort > FG_HF_threshold_);

               if (HCALFEM != 0) {
                  finegrain[ibin][0] = HCALFEM->fineGrainbit(
                        ADCShort, details.ShortDigi.id(),
                        details.ShortDigi[ibin].capid(),
                        ADCLong, details.LongDigi.id(),
                        details.LongDigi[ibin].capid()
                  );
               }
            }
        }
    }

    for (int bin = 0; bin < numberOfSamples_; ++bin) {
       static const unsigned int MAX_OUTPUT = QIE8_LINEARIZATION_ET;  // QIE8_LINEARIZATION_ET = 1023
       output[bin] = min({MAX_OUTPUT, output[bin] >> HF_LUMI_SHIFT});
    }

    std::vector<int> finegrain_converted;
    for (const auto& fg: finegrain)
       finegrain_converted.push_back(fg.to_ulong());
    outcoder_->compress(output, finegrain_converted, result);
    
}

void HcalTriggerPrimitiveAlgo::analyzeHF2017(
        const IntegerCaloSamples& samples, HcalTriggerPrimitiveDigi& result,
        const int hf_lumi_shift, const HcalFeatureBit* hcalfem)
{
    // Align digis and TP
    const int shift = samples.presamples() - numberOfPresamples_;
    assert(shift >= 0);
    assert((shift + numberOfSamples_) <= samples.size());

    // Try to find the HFDetails from the map corresponding to our samples
    const HcalTrigTowerDetId detId(samples.id());
    auto it = theHFUpgradeDetailMap.find(detId);
    // Missing values will give an empty digi
    if (it == theHFUpgradeDetailMap.end()) {
        return;
    }

    std::vector<int> finegrain(numberOfSamples_, false);

    // Set up out output of IntergerCaloSamples
    IntegerCaloSamples output(samples.id(), numberOfSamples_);
    output.setPresamples(numberOfPresamples_);

    for (const auto& item: it->second) {
        auto& details = item.second;
        for (int ibin = 0; ibin < numberOfSamples_; ++ibin) {
            const int idx = ibin + shift;

            int long_fiber_val = 0;
            int long_fiber_count = 0;
            int short_fiber_val = 0;
            int short_fiber_count = 0;

            bool saturated = false;

            for (auto i: {0, 2}) {
               if (idx < details[i].samples.size()) {
                  if ((unsigned int) details[i].digi[idx].adc() < override_parameters_->hf_adc_threshold
                        or (1ul << (details[i].digi[idx].le_tdc() - 1)) & override_parameters_->hf_tdc_mask) {
                     long_fiber_val += details[i].samples[idx];
                     saturated = saturated || (details[i].samples[idx] == QIE10_LINEARIZATION_ET);
                     ++long_fiber_count;
                  }
               }
            }
            for (auto i: {1, 3}) {
               if (idx < details[i].samples.size()) {
                  if ((unsigned int) details[i].digi[idx].adc() < override_parameters_->hf_adc_threshold
                        or (1ul << (details[i].digi[idx].le_tdc() - 1)) & override_parameters_->hf_tdc_mask) {
                     short_fiber_val += details[i].samples[idx];
                     saturated = saturated || (details[i].samples[idx] == QIE10_LINEARIZATION_ET);
                     ++short_fiber_count;
                  }
               }
            }

            if (saturated) {
               output[ibin] = QIE10_MAX_LINEARIZATION_ET;
            } else {
               // If one of the channels is invalid, double the value of
               // the other channel.
               if (long_fiber_count == 1)
                  long_fiber_val *= 2;
               if (short_fiber_count == 1)
                  short_fiber_val *= 2;

               // If one of the towers is invalid, double the value of the
               // other tower.
               if (long_fiber_count == 0)
                  short_fiber_val *= 2;
               if (short_fiber_count == 0)
                  long_fiber_val *= 2;

               // Ideally the sum of *4 channels* ⇒ LSB is also multiplied
               // by 4
               output[ibin] += long_fiber_val + short_fiber_val;

               if (long_fiber_count == 0 and short_fiber_count == 0)
                  output[ibin] = 0;
            }

            // int ADCLong = details.LongDigi[ibin].adc();
            // int ADCShort = details.ShortDigi[ibin].adc();
            // if(hcalfem != 0)
            // {
            //     finegrain[ibin] = (finegrain[ibin] || hcalfem->fineGrainbit(ADCShort, details.ShortDigi.id(), details.ShortDigi[ibin].capid(), ADCLong, details.LongDigi.id(), details.LongDigi[ibin].capid()));
            // }
        }
    }

    for (int bin = 0; bin < numberOfSamples_; ++bin) {
       output[bin] = min({(unsigned int) QIE10_MAX_LINEARIZATION_ET, output[bin] >> hf_lumi_shift});
    }
    outcoder_->compress(output, finegrain, result);
}

void HcalTriggerPrimitiveAlgo::runZS(HcalTrigPrimDigiCollection & result){
   for (HcalTrigPrimDigiCollection::iterator tp = result.begin(); tp != result.end(); ++tp){
      bool ZS = true;
      for (int i=0; i<tp->size(); ++i) {
         if (tp->sample(i).compressedEt()  > ZS_threshold_I_) {
            ZS=false;
            break;
         }
      }
      if (ZS) tp->setZSInfo(false,true);
      else tp->setZSInfo(true,false);
   }
}

void HcalTriggerPrimitiveAlgo::runFEFormatError(const FEDRawDataCollection* rawraw,
                                                const HcalElectronicsMap *emap,
                                                HcalTrigPrimDigiCollection & result
                                                ){
  std::set<uint32_t> FrontEndErrors;

  for(int i=FEDNumbering::MINHCALFEDID; i<=FEDNumbering::MAXHCALFEDID; ++i) {
    const FEDRawData& raw = rawraw->FEDData(i);
    if (raw.size()<12) continue;
    const HcalDCCHeader* dccHeader=(const HcalDCCHeader*)(raw.data());
    if(!dccHeader) continue;
    HcalHTRData htr;
    for (int spigot=0; spigot<HcalDCCHeader::SPIGOT_COUNT; spigot++) {
      if (!dccHeader->getSpigotPresent(spigot)) continue;
      dccHeader->getSpigotData(spigot,htr,raw.size());
      int dccid = dccHeader->getSourceId();
      int errWord = htr.getErrorsWord() & 0x1FFFF;
      bool HTRError = (!htr.check() || htr.isHistogramEvent() || (errWord & 0x800)!=0);

      if(HTRError) {
        bool valid =false;
        for(int fchan=0; fchan<3 && !valid; fchan++) {
          for(int fib=0; fib<9 && !valid; fib++) {
            HcalElectronicsId eid(fchan,fib,spigot,dccid-FEDNumbering::MINHCALFEDID);
            eid.setHTR(htr.readoutVMECrateId(),htr.htrSlot(),htr.htrTopBottom());
            DetId detId = emap->lookup(eid);
            if(detId.null()) continue;
            HcalSubdetector subdet=(HcalSubdetector(detId.subdetId()));
            if (detId.det()!=4||
              (subdet!=HcalBarrel && subdet!=HcalEndcap &&
              subdet!=HcalForward )) continue;
            std::vector<HcalTrigTowerDetId> ids = theTrigTowerGeometry->towerIds(detId);
            for (std::vector<HcalTrigTowerDetId>::const_iterator triggerId=ids.begin(); triggerId != ids.end(); ++triggerId) {
              FrontEndErrors.insert(triggerId->rawId());
            }
            //valid = true;
          }
        }
      }
    }
  }

  // Loop over TP collection
  // Set TP to zero if there is FE Format Error
  HcalTriggerPrimitiveSample zeroSample(0);
  for (HcalTrigPrimDigiCollection::iterator tp = result.begin(); tp != result.end(); ++tp){
    if (FrontEndErrors.find(tp->id().rawId()) != FrontEndErrors.end()) {
      for (int i=0; i<tp->size(); ++i) tp->setSample(i, zeroSample);
    }
  }
}

void HcalTriggerPrimitiveAlgo::addFG(const HcalTrigTowerDetId& id, std::vector<bool>& msb){
   FGbitMap::iterator itr = fgMap_.find(id);
   if (itr != fgMap_.end()){
      std::vector<bool>& _msb = itr->second;
      for (size_t i=0; i<msb.size(); ++i)
         _msb[i] = _msb[i] || msb[i];
   }
   else fgMap_[id] = msb;
}

void
HcalTriggerPrimitiveAlgo::addUpgradeFG(const HcalTrigTowerDetId& id, int depth, const std::vector<std::bitset<2>>& bits)
{
   auto it = fgUpgradeMap_.find(id);
   if (it == fgUpgradeMap_.end()) {
      FGUpgradeContainer element;
      element.resize(bits.size());
      it = fgUpgradeMap_.insert(std::make_pair(id, element)).first;
   }
   for (unsigned int i = 0; i < bits.size(); ++i) {
      it->second[i][0][depth] = bits[i][0];
      it->second[i][1][depth] = bits[i][1];
   }
}

void HcalTriggerPrimitiveAlgo::setPeakFinderAlgorithm(int algo){
   if (algo <=0 && algo>2)
      throw cms::Exception("ERROR: Only algo 1 & 2 are supported.") << std::endl;
   peak_finder_algorithm_ = algo;
}

void HcalTriggerPrimitiveAlgo::setNCTScaleShift(int shift){
   NCTScaleShift = shift;
}

void HcalTriggerPrimitiveAlgo::setRCTScaleShift(int shift){
   RCTScaleShift = shift;
}
