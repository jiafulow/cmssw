#ifndef HLTPFJetIDProducer2_h_
#define HLTPFJetIDProducer2_h_

/** \class HLTPFJetIDProducer2
 *
 *  \brief  This applies PF jet ID and produces a jet collection with jets that passed.
 *  \author Michele de Gruttola, Jia Fu Low (Nov 2013)
 *
 *  (Descriptions)
 *
 */

#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/JetReco/interface/PFJet.h"
#include "DataFormats/JetReco/interface/PFJetCollection.h"


namespace edm {
   class ConfigurationDescriptions;
}

// Class declaration
class HLTPFJetIDProducer2 : public edm::EDProducer {
  public:
    explicit HLTPFJetIDProducer2(const edm::ParameterSet & iConfig);
    ~HLTPFJetIDProducer2();
    static void fillDescriptions(edm::ConfigurationDescriptions & descriptions);
    virtual void produce(edm::Event & iEvent, const edm::EventSetup & iSetup);

  private:
    double minPt_;
    double CHF_;              ///< charged hadron fraction
    double NHF_;              ///< neutral hadron fraction
    double CEF_;              ///< charged EM fraction
    double NEF_;              ///< neutral EM fraction
    int NCH_;                 ///< number of charged constituents
    int NTOT_;                ///< number of constituents
    edm::InputTag inputTag_;  ///< input PF jet collection

    edm::EDGetTokenT<reco::PFJetCollection> m_thePFJetToken;
};

#endif  // HLTPFJetIDProducer2_h_
