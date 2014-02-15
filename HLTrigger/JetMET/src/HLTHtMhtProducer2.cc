/** \class HLTHtMhtProducer2
 *
 * See header file for documentation
 *
 *  \author Steven Lowette
 *
 */

#include "HLTrigger/JetMET/interface/HLTHtMhtProducer2.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/Common/interface/Handle.h"


// Constructor
HLTHtMhtProducer2::HLTHtMhtProducer2(const edm::ParameterSet & iConfig) :
  usePt_                  ( iConfig.getParameter<bool>("usePt") ),
  useTracks_              ( iConfig.getParameter<bool>("useTracks") ),
  usePFRecTracks_         ( iConfig.getParameter<bool>("usePFRecTracks") ),
  usePFCandidatesCharged_ ( iConfig.getParameter<bool>("usePFCandidatesCharged") ),
  usePFCandidates_        ( iConfig.getParameter<bool>("usePFCandidates") ),
  excludePFMuons_         ( iConfig.getParameter<bool>("excludePFMuons") ),
  minNJetHt_              ( iConfig.getParameter<int>("minNJetHt") ),
  minNJetMht_             ( iConfig.getParameter<int>("minNJetMht") ),
  minPtJetHt_             ( iConfig.getParameter<double>("minPtJetHt") ),
  minPtJetMht_            ( iConfig.getParameter<double>("minPtJetMht") ),
  maxEtaJetHt_            ( iConfig.getParameter<double>("maxEtaJetHt") ),
  maxEtaJetMht_           ( iConfig.getParameter<double>("maxEtaJetMht") ),
  jetsLabel_              ( iConfig.getParameter<edm::InputTag>("jetsLabel") ),
  tracksLabel_            ( iConfig.getParameter<edm::InputTag>("tracksLabel") ),
  pfRecTracksLabel_       ( iConfig.getParameter<edm::InputTag>("pfRecTracksLabel") ),
  pfCandidatesLabel_      ( iConfig.getParameter<edm::InputTag>("pfCandidatesLabel") ) {
    m_theJetToken = consumes<edm::View<reco::Jet>>(jetsLabel_);
    m_theTrackToken = consumes<reco::TrackCollection>(tracksLabel_);
    m_theRecTrackToken = consumes<reco::PFRecTrackCollection>(pfRecTracksLabel_);
    m_thePFCandidateToken = consumes<reco::PFCandidateCollection>(pfCandidatesLabel_);

    // Register the products
    produces<reco::METCollection>();
}

// Destructor
HLTHtMhtProducer2::~HLTHtMhtProducer2() {}

// Fill descriptions
void HLTHtMhtProducer2::fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
    // Current default is for hltHtMht
    edm::ParameterSetDescription desc;
    desc.add<bool>("usePt", false);
    desc.add<bool>("useTracks", false);
    desc.add<bool>("usePFRecTracks", false);
    desc.add<bool>("usePFCandidatesCharged", false);
    desc.add<bool>("usePFCandidates", false);
    desc.add<bool>("excludePFMuons", false);
    desc.add<int>("minNJetHt", 0);
    desc.add<int>("minNJetMht", 0);
    desc.add<double>("minPtJetHt", 40.);
    desc.add<double>("minPtJetMht", 30.);
    desc.add<double>("maxEtaJetHt", 3.);
    desc.add<double>("maxEtaJetMht", 5.);
    desc.add<edm::InputTag>("jetsLabel", edm::InputTag("hltCaloJetCorrected"));
    desc.add<edm::InputTag>("tracksLabel",  edm::InputTag(""));  // set to hltL3Muons?
    desc.add<edm::InputTag>("pfRecTracksLabel",  edm::InputTag(""));  // set to hltLightPFTracks?
    desc.add<edm::InputTag>("pfCandidatesLabel",  edm::InputTag(""));  // set to hltParticleFlow?
    descriptions.add("hltHtMhtProducer2", desc);
}

// Produce the products
void HLTHtMhtProducer2::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {

    // Create a pointer to the products
    std::auto_ptr<reco::METCollection> result(new reco::METCollection());

    if (pfCandidatesLabel_.label() == "")
        excludePFMuons_ = false;

    bool useJets = !useTracks_ && !usePFRecTracks_ && !usePFCandidatesCharged_ && !usePFCandidates_;
    if (!useJets) {
        minNJetHt_ = 0;
        minNJetMht_ = 0;
    }

    edm::Handle<reco::JetView> jets;
    if (useJets) iEvent.getByToken(m_theJetToken, jets);

    edm::Handle<reco::TrackCollection> tracks;
    if (useTracks_) iEvent.getByToken(m_theTrackToken, tracks);

    edm::Handle<reco::PFRecTrackCollection> pfRecTracks;
    if (usePFRecTracks_) iEvent.getByToken(m_theRecTrackToken, pfRecTracks);

    edm::Handle<reco::PFCandidateCollection> pfCandidates;
    if (excludePFMuons_ || usePFCandidatesCharged_ || usePFCandidates_)
        iEvent.getByToken(m_thePFCandidateToken, pfCandidates);

    int nj_ht = 0, nj_mht = 0;
    double ht = 0., mhx = 0., mhy = 0.;

    if (useJets && jets->size() > 0) {
        for(reco::JetView::const_iterator j = jets->begin(); j != jets->end(); ++j) {
            double pt = usePt_ ? j->pt() : j->et();
            double eta = j->eta();
            double phi = j->phi();
            double px = usePt_ ? j->px() : j->et() * cos(phi);
            double py = usePt_ ? j->py() : j->et() * sin(phi);

            if (pt > minPtJetHt_ && std::abs(eta) < maxEtaJetHt_) {
                ht += pt;
                ++nj_ht;
            }

            if (pt > minPtJetMht_ && std::abs(eta) < maxEtaJetMht_) {
                mhx -= px;
                mhy -= py;
                ++nj_mht;
            }
        }

    } else if (useTracks_ && tracks->size() > 0) {
        for (reco::TrackCollection::const_iterator j = tracks->begin(); j != tracks->end(); ++j) {
            double pt = j->pt();
            double px = j->px();
            double py = j->py();
            double eta = j->eta();

            if (pt > minPtJetHt_ && std::abs(eta) < maxEtaJetHt_) {
                ht += pt;
                ++nj_ht;
            }

            if (pt > minPtJetMht_ && std::abs(eta) < maxEtaJetMht_) {
                mhx -= px;
                mhy -= py;
                ++nj_mht;
            }
        }

    } else if (usePFRecTracks_ && pfRecTracks->size() > 0) {
        for (reco::PFRecTrackCollection::const_iterator j = pfRecTracks->begin(); j != pfRecTracks->end(); ++j) {
            double pt = j->trackRef()->pt();
            double px = j->trackRef()->px();
            double py = j->trackRef()->py();
            double eta = j->trackRef()->eta();

            if (pt > minPtJetHt_ && std::abs(eta) < maxEtaJetHt_) {
                ht += pt;
                ++nj_ht;
            }

            if (pt > minPtJetMht_ && std::abs(eta) < maxEtaJetMht_) {
                mhx -= px;
                mhy -= py;
                ++nj_mht;
            }
        }

    } else if ((usePFCandidatesCharged_ || usePFCandidates_) && pfCandidates->size() > 0) {
        for (reco::PFCandidateCollection::const_iterator j = pfCandidates->begin(); j != pfCandidates->end(); ++j) {
            if (usePFCandidatesCharged_ && j->charge() == 0)  continue;
            double pt = j->pt();
            double px = j->px();
            double py = j->py();
            double eta = j->eta();

            if (pt > minPtJetHt_ && std::abs(eta) < maxEtaJetHt_) {
                ht += pt;
                ++nj_ht;
            }

            if (pt > minPtJetMht_ && std::abs(eta) < maxEtaJetMht_) {
                mhx -= px;
                mhy -= py;
                ++nj_mht;
            }
        }
    }

    if (excludePFMuons_) {
        for (reco::PFCandidateCollection::const_iterator j = pfCandidates->begin(); j != pfCandidates->end(); ++j) {
            if (std::abs(j->pdgId()) == 13) {
                mhx += j->px();
                mhy += j->py();
            }
        }
    }

    if (nj_ht  < minNJetHt_ ) { ht = 0; }
    if (nj_mht < minNJetMht_) { mhx = 0; mhy = 0; }

    reco::MET::LorentzVector p4(mhx, mhy, 0, sqrt(mhx*mhx + mhy*mhy));
    reco::MET::Point vtx(0, 0, 0);
    reco::MET htmht(ht, p4, vtx);
    result->push_back(htmht);

    // Put the products into the Event
    iEvent.put(result);
}
