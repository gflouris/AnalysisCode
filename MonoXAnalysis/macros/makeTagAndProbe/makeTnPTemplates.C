#include "../CMS_lumi.h"

/// make the templates
void maketemplate(const string & inputDIR, 
		  const bool   & isMuon, 
		  const string & outputDIR, 
		  const string & idName,
		  const float  & ptMin,
		  const float  & ptMax,
		  const float  & etaMin,
		  const float  & etaMax,
		  bool  smooth = true,
		  int   nbins = 60, 
		  float xmin  = 65, 
		  float xmax  = 115) {

  TChain* inputMC = NULL;
  if(isMuon)
    inputMC = new TChain("muontnptree/fitter_tree");
  else
    inputMC = new TChain("electrontnptree/fitter_tree");

  inputMC->Add((inputDIR+"/*root").c_str());
  
  // pileup-re-weight from external file
  TFile* pufile = new TFile("$CMSSW_BASE/src/AnalysisCode/MonoXAnalysis/data/npvWeight/purwt.root");
  TH1* puhist = (TH1*)pufile->Get("puhist");

  TTreeReader reader(inputMC);
  TTreeReaderValue<float> mass (reader, "mass");
  TTreeReaderValue<float> eta  (reader, "eta");
  TTreeReaderValue<float> phi  (reader, "phi");
  TTreeReaderValue<float> pt   (reader, "pt");
  TTreeReaderValue<float> nvtx (reader, "nvtx");
  TTreeReaderValue<float> wgt  (reader, "wgt");
  TTreeReaderValue<int> id   (reader, idName.c_str());
  TTreeReaderValue<int> mcTrue  (reader, "mcTrue");
  TTreeReaderValue<float>    mcMass  (reader, "mcMass");

  TH1F hpass("hpass", "", nbins, xmin, xmax);
  TH1F hfail("hfail", "", nbins, xmin, xmax);
  TH1F hp("hp" , "", 1, xmin, xmax);
  TH1F ha("ha" , "", 1, xmin, xmax);
  TH1F hr("hr" , "", 1, xmin, xmax);
  hpass.Sumw2();
  hfail.Sumw2();
  hp.Sumw2();
  ha.Sumw2();
  hr.Sumw2();
  
  // weight sum
  double wgtsum = 0;
  while(reader.Next()){
    wgtsum += *wgt;
  }

  reader.SetEntry(0);
  // loop on the event
  while(reader.Next()){
    
    Float_t puwgt = 0.;
    if (*nvtx <= 40) puwgt = puhist->GetBinContent(puhist->FindBin(*nvtx));

    // check the probe lepton information --> if it falls inside the bins
    if(*pt < ptMin or *pt > ptMax) continue;
    if(fabs(*eta) < etaMin or fabs(*eta) > etaMax) continue;
    // if not matched to a genLepton skip --> we want to extract the true templateds
    if(not *mcTrue) continue;
    if (*id == 0) hfail.Fill(*mass, puwgt*(*wgt)/wgtsum);
    if (*id >  0) hpass.Fill(*mass, puwgt*(*wgt)/wgtsum);
    if (*id >  0) hp.Fill(*mass, puwgt*(*wgt)/wgtsum);
    ha.Fill(*mass, puwgt*(*wgt)/wgtsum);
  }

  for (int j = 1; j <= nbins; j++) {
    if (hpass.GetBinContent(j) < 0.) hpass.SetBinContent(j, 0.0);
    if (hfail.GetBinContent(j) < 0.) hfail.SetBinContent(j, 0.0);
  }
  
  // efficiency value with Binomial uncertainty
  hr.Divide(&hp, &ha, 1.0, 1.0, "B");
  // smooth templates two times
  if(smooth){// amcnlo as lower stat
    if(ptMin >= 50){
      hpass.Rebin(2);
      hfail.Rebin(2);
    }
    //    hpass.Smooth();
    //    hfail.Smooth();  
  }

  // Build the RooHistPdf  
  RooRealVar m("mass", "", xmin, xmax);
  RooDataHist dhpass("dhpass", "", RooArgList(m), RooFit::Import(hpass), 0);
  RooDataHist dhfail("dhfail", "", RooArgList(m), RooFit::Import(hfail), 0);
  RooHistPdf signalPassMC("signalPassMC", "", RooArgSet(m), dhpass);
  RooHistPdf signalFailMC("signalFailMC", "", RooArgSet(m), dhfail);

  RooWorkspace w("w", "");
  w.import(signalPassMC);
  w.import(signalFailMC);

  string fileName = "";
  if(isMuon)
    fileName = "template_TaP_muon_"+idName+"_pt_"+to_string(int(ptMin))+"_"+to_string(int(ptMax))+"_eta_"+to_string(int(etaMin))+"_"+to_string(int(etaMax));
  else
    fileName = "template_TaP_electron_"+idName+"_pt_"+to_string(int(ptMin))+"_"+to_string(int(ptMax))+"_eta_"+to_string(int(etaMin))+"_"+to_string(int(etaMax));


  TFile outfile((outputDIR+"/"+fileName+".root").c_str(), "RECREATE");
  w.Write();
  hpass.Write();
  hfail.Write();
  
  cout << "Efficiency -- pT [" << ptMin << ", " << ptMax << "], eta [" << etaMin << ", " << etaMax << "]   :   " << hr.GetBinContent(1) << " +/- " << hr.GetBinError(1) << endl;

  TCanvas* c1 = new TCanvas("c1","",600,650);
  c1->cd();
  hpass.Scale(1./hpass.Integral());
  hfail.Scale(1./hfail.Integral());
  hfail.SetLineColor(kRed);
  hpass.SetLineColor(kBlue);
  hfail.SetLineWidth(2);
  hpass.SetLineWidth(2);  
  hpass.Draw("hist");
  hfail.Draw("hist same");

  TLegend* leg = new TLegend(0.2,0.7,0.4,0.9);
  leg->SetFillColor(0);
  leg->SetBorderSize(0);
  leg->AddEntry(&hpass,"Passing probes","L");
  leg->AddEntry(&hfail,"Failing probes","L");
  leg->Draw("same");

  c1->SaveAs((outputDIR+"/"+fileName+".png").c_str(),"png");
  c1->SaveAs((outputDIR+"/"+fileName+".pdf").c_str(),"pdf");
}

void makeTnPTemplates(string inputDIR, bool isMuon, string outputDIR) {

  gROOT->SetBatch(kTRUE);
  system(("mkdir -p "+outputDIR).c_str());

  vector<float> ptBin  = {10.,20.,30.,40.,50.,70.,200};
  vector<float> etaBin = {0.,1.2,2.4};

  setTDRStyle();
  
  for(size_t ipt = 0; ipt < ptBin.size()-1; ipt++){
    for(size_t ieta = 0; ieta < etaBin.size()-1; ieta++){
      if(isMuon){
	string idName = "looseid";
	maketemplate(inputDIR, isMuon, outputDIR, idName, ptBin.at(ipt), ptBin.at(ipt+1), etaBin.at(ieta), etaBin.at(ieta+1),true);    
	idName = "tightid";
	maketemplate(inputDIR, isMuon, outputDIR, idName, ptBin.at(ipt), ptBin.at(ipt+1), etaBin.at(ieta), etaBin.at(ieta+1),true);

      }
      else{
	string idName = "vetoid";
	maketemplate(inputDIR, isMuon, outputDIR, idName, ptBin.at(ipt), ptBin.at(ipt+1), etaBin.at(ieta), etaBin.at(ieta+1));    

	idName = "tightid";
	maketemplate(inputDIR, isMuon, outputDIR, idName, ptBin.at(ipt), ptBin.at(ipt+1), etaBin.at(ieta), etaBin.at(ieta+1));
      }
    }
  }
}