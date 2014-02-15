/** \class HLTMhtFilter2
 *
 * See header file for documentation
 *
 *  \author Steven Lowette
 *
 */

#include "HLTrigger/JetMET/interface/HLTMhtFilter2.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"


// Constructor
HLTMhtFilter2::HLTMhtFilter2(const edm::ParameterSet & iConfig) : HLTFilter(iConfig),
  minMht_    ( iConfig.getParameter<std::vector<double> >("minMht") ),
  mhtLabels_ ( iConfig.getParameter<std::vector<edm::InputTag> >("mhtLabels") ),
  nOrs_      ( mhtLabels_.size() ) {  // number of settings to .OR.
    if (!(mhtLabels_.size() == minMht_.size()) ||
        mhtLabels_.size() == 0 ) {
        nOrs_ = (minMht_.size()    < nOrs_ ? minMht_.size()    : nOrs_);
        edm::LogError("HLTMhtFilter2") << "inconsistent module configuration!";
    }

    moduleLabel_ = iConfig.getParameter<std::string>("@module_label");

    for(unsigned int i=0; i<nOrs_; ++i) {
        m_theMhtToken.push_back(consumes<reco::METCollection>(mhtLabels_[i]));
    }

    // Register the products
    produces<reco::METCollection>();
}

// Destructor
HLTMhtFilter2::~HLTMhtFilter2() {}

// Fill descriptions
void HLTMhtFilter2::fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
    std::vector<edm::InputTag> tmp1(1, edm::InputTag("hltMhtProducer"));
    std::vector<double>        tmp2(1, 0.);
    edm::ParameterSetDescription desc;
    makeHLTFilterDescription(desc);
    desc.add<std::vector<edm::InputTag> >("mhtLabels", tmp1);
    tmp2[0] =  70; desc.add<std::vector<double> >("minMht", tmp2);
    descriptions.add("hltMhtFilter2", desc);
}

// Make filter decision
bool HLTMhtFilter2::hltFilter(edm::Event & iEvent, const edm::EventSetup & iSetup, trigger::TriggerFilterObjectWithRefs & filterproduct) const {

    // Create a pointer to the output filter objects
    std::auto_ptr<reco::METCollection> result(new reco::METCollection());

    // Create the reference to the output filter objects
    if (saveTags())  filterproduct.addCollectionTag(moduleLabel_);

    bool accept = false;

    // Take the .OR. of all sets of requirements
    for (unsigned int i = 0; i < nOrs_; ++i) {
        edm::Handle<reco::METCollection> hmht;
        iEvent.getByToken(m_theMhtToken[i], hmht);
        double mht = 0;
        if (hmht->size() > 0)  mht = hmht->front().pt();

        // Check if the event passes this cut set
        accept = accept || (mht > minMht_[i]);
        // In principle we could break if accepted, but in order to save
        // for offline analysis all possible decisions we keep looping here
        // in term of timing this will not matter much; typically 1 or 2 cut-sets
        // will be checked only

        // Store the object that was cut on and the ref to it
        if (hmht->size() > 0)  result->push_back(hmht->front());

        edm::Ref<reco::METCollection> mhtref(iEvent.getRefBeforePut<reco::METCollection>(), i);  // reference to i-th object
        filterproduct.addObject(trigger::TriggerMHT, mhtref);  // save as TriggerMHT object
    }

    iEvent.put(result);

    return accept;
}
