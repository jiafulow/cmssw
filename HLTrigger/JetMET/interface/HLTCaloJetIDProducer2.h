#ifndef HLTCaloJetIDProducer2_h_
#define HLTCaloJetIDProducer2_h_

/** \class HLTCaloJetIDProducer2
 *
 *  \brief  This applies Calo jet ID and produces a jet collection with jets that passed.
 *  \author a Jet/MET person
 *  \author Michele de Gruttola, Jia Fu Low (Nov 2013)
 *
 *  (Descriptions)
 *
 */

#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"

#include "RecoJets/JetProducers/interface/JetIDHelper.h"


namespace edm {
    class ConfigurationDescriptions;
}

namespace reco {
    namespace helper {
        class JetIDHelper;
    }
}

// Class declaration
class HLTCaloJetIDProducer2 : public edm::EDProducer {
  public:
    explicit HLTCaloJetIDProducer2(const edm::ParameterSet & iConfig);
    ~HLTCaloJetIDProducer2();
    static void fillDescriptions(edm::ConfigurationDescriptions & descriptions);
    virtual void produce(edm::Event & iEvent, const edm::EventSetup & iSetup);

  private:
    int min_N90_;                     ///< mininum N90
    int min_N90hits_;                 ///< mininum N90hits
    double min_EMF_;                  ///< minimum EMF
    double max_EMF_;                  ///< maximum EMF
    edm::InputTag inputTag_;          ///< input calo jet collection
    edm::ParameterSet jetIDParams_;   ///< calo jet ID parameters

    /// A helper to calculates calo jet ID variables.
    reco::helper::JetIDHelper jetIDHelper_;

    edm::EDGetTokenT<reco::CaloJetCollection> m_theCaloJetToken;
};

#endif  // HLTCaloJetIDProducer2_h_
