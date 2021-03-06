// basic C++ headers
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iostream>

//boost lybraries
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

// FWCore headers
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

// additional
#include "CommonTools/UtilAlgos/interface/TFileService.h" 
#include "SimDataFormats/GeneratorProducts/interface/LHEEventProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/LHERunInfoProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"
#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"

// ROOT headers
#include "TTree.h"


class LHEWeightsTreeMaker : public edm::one::EDAnalyzer<edm::one::SharedResources,edm::one::WatchRuns> {

public:
  explicit LHEWeightsTreeMaker(const edm::ParameterSet&);
  ~LHEWeightsTreeMaker();
  
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);
  
  
private:
  virtual void beginJob() override;
  virtual void analyze(const edm::Event&, const edm::EventSetup&) override;
  virtual void endJob() override;
  
  virtual void beginRun(edm::Run const&, edm::EventSetup const&) override;
  virtual void endRun(edm::Run const&, edm::EventSetup const&) override;
  
  // InputTags
  const edm::InputTag lheInfoTag;
  const edm::InputTag lheRunInfoTag;
  const edm::InputTag genInfoTag;
  const edm::InputTag gensInfoTag;
  const edm::InputTag pileupInfoTag;
  
  // Tokens
  edm::EDGetTokenT<LHEEventProduct> lheInfoToken;
  edm::EDGetTokenT<LHERunInfoProduct> lheRunInfoToken;
  edm::EDGetTokenT<GenEventInfoProduct> genInfoToken;
  edm::EDGetTokenT<std::vector<PileupSummaryInfo> > pileupInfoToken;
  edm::EDGetTokenT<edm::View<reco::GenParticle> >    gensToken;
  
  const bool uselheweights, addqcdpdfweights, isSignalSample;

  TTree* tree;
  
  uint32_t event, run, lumi;
  int      puobs, putrue;
  float    wgt;
  float    wgtoriginal;
  float    lheXSEC;
  float    samplemedM, sampledmM;
  bool     readDMFromGenParticles;
  std::vector<float>  wgtpdf; //have id larger than 2000 for madgraph                                                                                                                                
  std::vector<float>  wgtqcd; //have id [1000-2000] for madgraph                                                                                                                                        
};

LHEWeightsTreeMaker::LHEWeightsTreeMaker(const edm::ParameterSet& iConfig): 
  lheInfoTag(iConfig.getParameter<edm::InputTag>("lheinfo")),
  lheRunInfoTag(iConfig.getParameter<edm::InputTag>("lheRuninfo")),
  genInfoTag(iConfig.getParameter<edm::InputTag>("geninfo")),
  gensInfoTag(iConfig.getParameter<edm::InputTag>("genParticles")),
  pileupInfoTag(iConfig.getParameter<edm::InputTag>("pileupinfo")),
  uselheweights(iConfig.getParameter<bool>("uselheweights")),
  addqcdpdfweights(iConfig.getParameter<bool>("addqcdpdfweights")),
  isSignalSample(iConfig.getParameter<bool>("isSignalSample"))
{
  // Token consumes instructions
  lheInfoToken = consumes<LHEEventProduct>(lheInfoTag);
  lheRunInfoToken = consumes<LHERunInfoProduct,edm::InRun>(lheRunInfoTag);
  genInfoToken = consumes<GenEventInfoProduct>(genInfoTag);
  pileupInfoToken = consumes<std::vector<PileupSummaryInfo> >(pileupInfoTag);

  gensToken = consumes<edm::View<reco::GenParticle> > (gensInfoTag);

  // state that TFileService is used
  usesResource();
  usesResource("TFileService");

  readDMFromGenParticles = false;
}


LHEWeightsTreeMaker::~LHEWeightsTreeMaker() {}

void LHEWeightsTreeMaker::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  
  using namespace edm;
  using namespace std;
  using namespace reco;

  // Get handles to all the requisite collections
  Handle<LHEEventProduct> lheInfoH;
  if (uselheweights) 
    iEvent.getByToken(lheInfoToken, lheInfoH);
    
  Handle<GenEventInfoProduct> genInfoH;
  if (uselheweights) 
    iEvent.getByToken(genInfoToken, genInfoH);

  Handle<std::vector<PileupSummaryInfo> > pileupInfoH;
  iEvent.getByToken(pileupInfoToken,pileupInfoH);

  Handle<View<GenParticle> > gensH;
  iEvent.getByToken(gensToken, gensH);

  // Event, lumi, run info
  event = iEvent.id().event();
  run   = iEvent.id().run();
  lumi  = iEvent.luminosityBlock();

  puobs = 0;
  putrue = 0;

  if(pileupInfoH.isValid()) {
    for (auto pileupInfo_iter = pileupInfoH->begin(); pileupInfo_iter != pileupInfoH->end(); ++pileupInfo_iter) {
      if (pileupInfo_iter->getBunchCrossing() == 0) {
	puobs  = pileupInfo_iter->getPU_NumInteractions();
	putrue = pileupInfo_iter->getTrueNumInteractions();
      }
    }
  }
  

  // Weights info
  wgt = 1.0;
  wgtoriginal = 1.0;
  if (uselheweights) {
    wgt = genInfoH->weight();
    wgtoriginal = lheInfoH->originalXWGTUP();
  }
  
  if (addqcdpdfweights) {
    wgtpdf.clear();
    wgtqcd.clear();

    vector<gen::WeightsInfo> weights = lheInfoH->weights();
    for (size_t i = 0; i < weights.size(); i++) {
      if(std::stoi(weights[i].id) >= 1001 and std::stoi(weights[i].id) <= 1009) // scale varions                                                                                                   
	wgtqcd.push_back(weights[i].wgt);
      else if(std::stoi(weights[i].id) >= 2001 and std::stoi(weights[i].id) < 3000) // variations of NNPDF LO or NLO                                                                               
	wgtpdf.push_back(weights[i].wgt);
      //else if(std::stoi(weights[i].id) >= 3001 and std::stoi(weights[i].id) < 4000) // variations of CT10                                                                                         
      //      wgtpdf.push_back(weights[i].wgt);                                                                                                                                                    
      //else if(std::stoi(weights[i].id) >= 4001 and std::stoi(weights[i].id) < 5000) // variations of MMHT                                                                                        
      //      wgtpdf.push_back(weights[i].wgt);                                                                                                                                                    
      //else if(std::stoi(weights[i].id) >= 5001 and std::stoi(weights[i].id) < 6000) // variations of HERAPDF                                                                                    
      //      wgtpdf.push_back(weights[i].wgt);                                                                                                                                                    
      else
	continue;
    }
  }

  if(readDMFromGenParticles and gensH.isValid()){

    bool foundfirst = false;
    for (auto gens_iter = gensH->begin(); gens_iter != gensH->end(); ++gens_iter) {
      bool goodParticle = false;
      if (abs(gens_iter->pdgId()) >= 1000001 and abs(gens_iter->pdgId()) <= 1000039)
	goodParticle = true;
      else if(abs(gens_iter->pdgId()) >= 2000001 and abs(gens_iter->pdgId()) <= 2000015)
	goodParticle = true;
      else if(abs(gens_iter->pdgId()) == 9100012)
	goodParticle = true;
      
      if(not goodParticle)
	continue;

      if(!foundfirst) // first DM particle                                                                                                                                   
	sampledmM = gens_iter->mass();
    }
  }
  
  tree->Fill();

}


void LHEWeightsTreeMaker::beginJob() {

  edm::Service<TFileService> fs;
  tree = fs->make<TTree>("gentree"    , "gentree");
  // Run, Lumi, Event info
  tree->Branch("event"                , &event                , "event/i");
  tree->Branch("run"                  , &run                  , "run/i");
  tree->Branch("lumi"                 , &lumi                 , "lumi/i");
  // Event weights
  tree->Branch("wgt"                  , &wgt                  , "wgt/F");
  tree->Branch("wgtoriginal"          , &wgtoriginal          , "wgtoriginal/F");
  // pileup info
  tree->Branch("puobs"                , &puobs                , "puobs/I");
  tree->Branch("putrue"               , &putrue               , "putrue/I");

  if (addqcdpdfweights) {
    tree->Branch("wgtpdf"               , "std::vector<float>",  &wgtpdf);
    tree->Branch("wgtqcd"               , "std::vector<float>",  &wgtqcd);
  }

  // LHE xs
  tree->Branch("lheXSEC",    &lheXSEC,    "lheXSEC/F");
  // sample info: mediator and DM mass, useful for fast sim
  tree->Branch("samplemedM", &samplemedM, "samplemedM/F");
  tree->Branch("sampledmM",  &sampledmM,  "sampledmM/F");

}

void LHEWeightsTreeMaker::endJob() {}

void LHEWeightsTreeMaker::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup) {

  if(uselheweights){
    // in cae of MC store XS value                                                                                                                                              
    edm::Handle<LHERunInfoProduct> run;
    iRun.getByLabel(lheRunInfoTag,run);
    LHERunInfoProduct myLHERunInfoProduct = *(run.product());
    lheXSEC = myLHERunInfoProduct.heprup().XSECUP.at(0); 

    
    using namespace boost::algorithm;
    
    if(isSignalSample){
      for (auto iter = myLHERunInfoProduct.headers_begin(); iter != myLHERunInfoProduct.headers_end(); iter++){
	std::vector<std::string> lines = iter->lines();    
	for (unsigned int iLine = 0; iLine<lines.size(); iLine++) {
	  std::vector<std::string> tokens;
	  if(lines.at(iLine).find("DMmass") !=std::string::npos){ // powheg mono-jet
	    split(tokens, lines.at(iLine), is_any_of(" "));
	    tokens.erase(std::remove(tokens.begin(), tokens.end(),""), tokens.end());
	    sampledmM = std::stod(tokens.at(1));
	  }
	  else if(lines.at(iLine).find("DMVmass") !=std::string::npos){ // powheg mono-jet
	    split(tokens, lines.at(iLine), is_any_of(" "));
	    tokens.erase(std::remove(tokens.begin(), tokens.end(),""), tokens.end());
	    samplemedM = std::stod(tokens.at(1));
	  }
	  else if(lines.at(iLine).find("import model") !=std::string::npos){ // madgraph mono-V
	    split(tokens, lines.at(iLine), is_any_of(" "));
	    tokens.erase(std::remove(tokens.begin(), tokens.end(),""), tokens.end());
	    std::vector<std::string> subtokens;
	    split(subtokens,tokens.at(2),is_any_of("_"));	
	    samplemedM = std::stod(subtokens.at(3));
	    sampledmM = std::stod(subtokens.at(4));	
	  }      
	  else if(lines.at(iLine).find("Resonance:") != std::string::npos){ // JHUGen --> only resonance mass (mediator) .. dM fixed in the event loop
	    split(tokens, lines.at(iLine), is_any_of(" "));
	    tokens.erase(std::remove(tokens.begin(), tokens.end(),""), tokens.end());
	    samplemedM = std::stod(tokens.at(3));
	    sampledmM  = -1.; 
	  readDMFromGenParticles = true;
	  }
	}
      }   
    }
  }
}

void LHEWeightsTreeMaker::endRun(edm::Run const&, edm::EventSetup const&) {}

void LHEWeightsTreeMaker::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.setUnknown();
    descriptions.addDefault(desc);
}

DEFINE_FWK_MODULE(LHEWeightsTreeMaker);

