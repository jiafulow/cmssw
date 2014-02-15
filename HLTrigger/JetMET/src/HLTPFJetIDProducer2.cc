/** \class HLTPFJetIDProducer2
 *
 * See header file for documentation
 *
 *  \author a Jet/MET person
 *
 */

#include "HLTrigger/JetMET/interface/HLTPFJetIDProducer2.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/Common/interface/Handle.h"


// Constructor
HLTPFJetIDProducer2::HLTPFJetIDProducer2(const edm::ParameterSet& iConfig) :
  minPt_    (iConfig.getParameter<double>("minPt")),
  CHF_      (iConfig.getParameter<double>("CHF")),
  NHF_      (iConfig.getParameter<double>("NHF")),
  CEF_      (iConfig.getParameter<double>("CEF")),
  NEF_      (iConfig.getParameter<double>("NEF")),
  NCH_      (iConfig.getParameter<int>("NCH")),
  NTOT_     (iConfig.getParameter<int>("NTOT")),
  inputTag_ (iConfig.getParameter<edm::InputTag>("inputTag")) {
    m_thePFJetToken = consumes<reco::PFJetCollection>(inputTag_);

    // Register the products
    produces<reco::PFJetCollection>();
}

// Destructor
HLTPFJetIDProducer2::~HLTPFJetIDProducer2() {}

// Fill descriptions
void HLTPFJetIDProducer2::fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<double>("minPt", 20.);
    desc.add<double>("CHF", -99.);
    desc.add<double>("NHF", 99.);
    desc.add<double>("CEF", 99.);
    desc.add<double>("NEF", 99.);
    desc.add<int>("NCH", 0);
    desc.add<int>("NTOT", 0);
    desc.add<edm::InputTag>("inputTag", edm::InputTag("hltAntiKT4PFJets"));
    descriptions.add("hltPFJetIDProducer2", desc);
}

// Produce the products
void HLTPFJetIDProducer2::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

    // Create a pointer to the products
    std::auto_ptr<reco::PFJetCollection> result (new reco::PFJetCollection());

    edm::Handle<reco::PFJetCollection> pfjets;
    iEvent.getByToken(m_thePFJetToken, pfjets);

    for (reco::PFJetCollection::const_iterator j = pfjets->begin(); j != pfjets->end(); ++j) {
        bool pass = false;
        double pt = j->pt();
        double eta = j->eta();

        if (!(pt > 0.))  continue;  // skip jets with zero or negative pt

        if (pt < minPt_) {
            pass = true;

        //} else if (std::abs(eta) > 2.4) {
        //    pass = true;

        } else {
            double chf  = j->chargedHadronEnergyFraction();
            double nhf  = j->neutralHadronEnergyFraction() + j->HFHadronEnergyFraction();
            //double nhf  = j->neutralHadronEnergyFraction();
            double cef  = j->chargedEmEnergyFraction();
            double nef  = j->neutralEmEnergyFraction();
            int    nch  = j->chargedMultiplicity();
            int    ntot = j->numberOfDaughters();

            pass = true;
            pass = pass && (ntot > NTOT_);
            pass = pass && (nef < NEF_);
            pass = pass && (nhf < NHF_);
            pass = pass && (cef < CEF_ || std::abs(eta) > 2.4);
            pass = pass && (chf > CHF_ || std::abs(eta) > 2.4);
            pass = pass && (nch > NCH_ || std::abs(eta) > 2.4);
        }

        if (pass)  result->push_back(*j);
    }

    // Put the products into the Event
    iEvent.put(result);
}
