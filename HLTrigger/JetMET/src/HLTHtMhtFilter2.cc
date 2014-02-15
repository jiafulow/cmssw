/** \class HLTHtMhtFilter2
 *
 * See header file for documentation
 *
 *  \author Steven Lowette
 *
 */

#include "HLTrigger/JetMET/interface/HLTHtMhtFilter2.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"


// Constructor
HLTHtMhtFilter2::HLTHtMhtFilter2(const edm::ParameterSet & iConfig) : HLTFilter(iConfig),
  minHt_     ( iConfig.getParameter<std::vector<double> >("minHt") ),
  minMht_    ( iConfig.getParameter<std::vector<double> >("minMht") ),
  minMeff_   ( iConfig.getParameter<std::vector<double> >("minMeff") ),
  meffSlope_ ( iConfig.getParameter<std::vector<double> >("meffSlope") ),
  htLabels_  ( iConfig.getParameter<std::vector<edm::InputTag> >("htLabels") ),
  mhtLabels_ ( iConfig.getParameter<std::vector<edm::InputTag> >("mhtLabels") ),
  nOrs_      ( htLabels_.size() ) {  // number of settings to .OR.
    if (!( htLabels_.size() == minHt_.size() &&
           htLabels_.size() == minMht_.size() &&
           htLabels_.size() == minMeff_.size() &&
           htLabels_.size() == meffSlope_.size() &&
           htLabels_.size() == mhtLabels_.size() ) ||
        htLabels_.size() == 0 ) {
        nOrs_ = (minHt_.size()     < nOrs_ ? minHt_.size()     : nOrs_);
        nOrs_ = (minMht_.size()    < nOrs_ ? minMht_.size()    : nOrs_);
        nOrs_ = (minMeff_.size()   < nOrs_ ? minMeff_.size()   : nOrs_);
        nOrs_ = (meffSlope_.size() < nOrs_ ? meffSlope_.size() : nOrs_);
        nOrs_ = (mhtLabels_.size() < nOrs_ ? mhtLabels_.size() : nOrs_);
        edm::LogError("HLTHtMhtFilter2") << "inconsistent module configuration!";
    }

    moduleLabel_ = iConfig.getParameter<std::string>("@module_label");

    for(unsigned int i=0; i<nOrs_; ++i) {
        m_theHtToken.push_back(consumes<reco::METCollection>(htLabels_[i]));
        m_theMhtToken.push_back(consumes<reco::METCollection>(mhtLabels_[i]));
    }

    // Register the products
    produces<reco::METCollection>();
}

// Destructor
HLTHtMhtFilter2::~HLTHtMhtFilter2() {}

// Fill descriptions
void HLTHtMhtFilter2::fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
    std::vector<edm::InputTag> tmp1(1, edm::InputTag("hltHtMhtProducer"));
    std::vector<double>        tmp2(1, 0.);
    edm::ParameterSetDescription desc;
    makeHLTFilterDescription(desc);
    desc.add<std::vector<edm::InputTag> >("htLabels",  tmp1);
    desc.add<std::vector<edm::InputTag> >("mhtLabels", tmp1);
    tmp2[0] = 250; desc.add<std::vector<double> >("minHt",     tmp2);
    tmp2[0] =  70; desc.add<std::vector<double> >("minMht",    tmp2);
    tmp2[0] =   0; desc.add<std::vector<double> >("minMeff",   tmp2);
    tmp2[0] =   1; desc.add<std::vector<double> >("meffSlope", tmp2);
    descriptions.add("hltHtMhtFilter2", desc);
}

// Make filter decision
bool HLTHtMhtFilter2::hltFilter(edm::Event & iEvent, const edm::EventSetup & iSetup, trigger::TriggerFilterObjectWithRefs & filterproduct) const {

    // Create a pointer to the output filter objects
    std::auto_ptr<reco::METCollection> result(new reco::METCollection());

    // Create the reference to the output filter objects
    if (saveTags())  filterproduct.addCollectionTag(moduleLabel_);

    bool accept = false;

    // Take the .OR. of all sets of requirements
    for (unsigned int i = 0; i < nOrs_; ++i) {
        edm::Handle<reco::METCollection> hht;
        iEvent.getByToken(m_theHtToken[i], hht);
        double ht = 0;
        if (hht->size() > 0)  ht = hht->front().sumEt();

        edm::Handle<reco::METCollection> hmht;
        iEvent.getByToken(m_theMhtToken[i], hmht);
        double mht = 0;
        if (hmht->size() > 0)  mht = hmht->front().pt();

        // Check if the event passes this cut set
        accept = accept || (ht > minHt_[i] && mht > minMht_[i] && sqrt(mht + meffSlope_[i]*ht) > minMeff_[i]);
        // In principle we could break if accepted, but in order to save
        // for offline analysis all possible decisions we keep looping here
        // in term of timing this will not matter much; typically 1 or 2 cut-sets
        // will be checked only

        // Store the object that was cut on and the ref to it
        reco::MET htmht(ht, hmht->front().p4(), reco::MET::Point(0, 0, 0));
        result->push_back(htmht);

        edm::Ref<reco::METCollection> htmhtref(iEvent.getRefBeforePut<reco::METCollection>(), i);  // reference to i-th object
        filterproduct.addObject(trigger::TriggerMHT, htmhtref);  // save as TriggerMHT object
    }

    iEvent.put(result);

    return accept;
}
