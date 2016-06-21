import FWCore.ParameterSet.Config as cms
import sys, os
## CMSSW command line parameter parser                                                                                                                                          
from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing ('python')

## data or MC options                                                                                                                                                          
options.register (
    'isMC',False,VarParsing.multiplicity.singleton,VarParsing.varType.bool,
    'flag to indicate data or MC');

options.register (
    'inputDIR',"",VarParsing.multiplicity.singleton,VarParsing.varType.string,
    'Directory where the tag and probes root files are located');

options.register (
    'isEOSDIR',False,VarParsing.multiplicity.singleton,VarParsing.varType.bool,
    'True if the input directory is located on eos');

options.register (
    'outputDIR',"",VarParsing.multiplicity.singleton,VarParsing.varType.string,
    'where output files are created');

options.register(
    'typeID',"tight",VarParsing.multiplicity.singleton,VarParsing.varType.string,
    'string to indicate the lepton type to be considered');

options.register(
    'leptonPID',11,VarParsing.multiplicity.singleton,VarParsing.varType.int,
    '11 means electrons, 13 means muons, 22 photon');

#### binning definition
options.register(
    'ptMin',0,VarParsing.multiplicity.singleton,VarParsing.varType.float,
    'min pt for leptons');

options.register(
    'ptMax',0,VarParsing.multiplicity.singleton,VarParsing.varType.float,
    'max pt for leptons');

options.register(
    'etaMin',0,VarParsing.multiplicity.singleton,VarParsing.varType.float,
    'min eta for leptons');

options.register(
    'etaMax',0,VarParsing.multiplicity.singleton,VarParsing.varType.float,
    'max eta for leptons');

options.register(
    'templateFile',
    "",VarParsing.multiplicity.singleton,VarParsing.varType.string,
    'input file with templates RooHistPdf');

### additional parameters
options.register(
    'floatShapeParameters',True,VarParsing.multiplicity.singleton,VarParsing.varType.bool,
    'float shape parameters in the fit');

options.register(
    'binsForMassPlots',25,VarParsing.multiplicity.singleton,VarParsing.varType.int,
    'number of bins for the output plot');

options.register(
    'backgroundType','RooCMSShape',VarParsing.multiplicity.singleton,VarParsing.varType.string,
    'Available options at the moment: RooCMSShape, Exponential');

options.register(
    'signalType','',VarParsing.multiplicity.singleton,VarParsing.varType.string,
    'too add an additional string to the output root file .. useful for alternative signal templates');

## parsing command line arguments                                                                                                                                            
options.parseArguments()

#### check the lepton type
if options.leptonPID == 13:
    dirName = "muontnptree";
elif options.leptonPID == 11:
    dirName = "electrontnptree";
elif options.leptonPID == 22:
    dirName = "photontnptree";    
else:
    sys.exit('Lepton type not recognized --> return');


#### build the input file list
if options.inputDIR != "":
    if not options.isEOSDIR:
        os.system("ls "+options.inputDIR+" > file_list_"+str(options.leptonPID)+"_"+options.typeID);
        file = open("file_list_"+str(options.leptonPID)+"_"+options.typeID,"r");
        inputFileList = [];
        for line in file:
            if line == "" or line == "\n": continue;
            if ".root" in line:
                inputFileList.append(options.inputDIR+"/"+line.replace("\n",""));
        os.system("rm file_list_"+str(options.leptonPID)+"_"+options.typeID);
    else:
        os.system("/afs/cern.ch/project/eos/installation/cms/bin/eos.select ls "+options.inputDIR+" > file_list_"+str(options.leptonPID)+"_"+options.typeID);
        file = open("file_list_"+str(options.leptonPID)+"_"+options.typeID,"r");
        inputFileList = [];
        for line in file:
            if line == "" or line == "\n": continue;
            if ".root" in line:
                inputFileList.append("root://eoscms.cern.ch//eos/cms/"+options.inputDIR+"/"+line.replace("\n",""));
        os.system("rm file_list_"+str(options.leptonPID)+"_"+options.typeID);        
        ifile = 0;
        for file in inputFileList:
            os.system("xrdcp -f "+file+" ./");
            inputFileList[ifile] = file.replace("root://eoscms.cern.ch//eos/cms/"+options.inputDIR+"/","");
else:
    sys.exit('No input dir found --> return');

## check binning
if options.ptMin >= options.ptMax:
    sys.exit('Wrong pT binning --> return');
if options.etaMin >= options.etaMax:
    sys.exit('Wrong pT binning --> return');

## check the id type:
if options.leptonPID == 11:
    if options.typeID != "vetoid" and options.typeID != "tightid":
        sys.exit('For electrons only vetoid or tightid are accepted --> you are parsing a wrong a id type --> return');
elif options.leptonPID ==13:
    if options.typeID != "looseid" and options.typeID != "tightid":
        sys.exit('For muons only looseid or tightid are accepted --> you are parsing a wrong a id type --> return');
elif options.leptonPID ==22:
    if options.typeID != "looseid" and options.typeID != "tightid" and options.typeID != "mediumid":
        sys.exit('For photons only looseid or mediumid or tightid are accepted --> you are parsing a wrong a id type --> return');
 
if not os.path.isfile(options.templateFile):
    sys.exit('Template file not found --> please check --> return');

#### check if data or MC
postfix = "";
if options.leptonPID == 11:
    postfix = "electron";
elif options.leptonPID == 13:
    postfix = "muon";
elif options.leptonPID == 22:
    postfix = "photon";

postfix += "_"+options.typeID+"_pt_"+str(options.ptMin)+"_"+str(options.ptMax)+"_eta_"+str(options.etaMin)+"_"+str(options.etaMax);

if options.isMC : 
    OutputFilePrefix = "efficiency-mc_"+postfix;
else :
    OutputFilePrefix = "efficiency-data"+postfix;

## create the pricess 
process = cms.Process("TNP")
process.source = cms.Source("EmptySource")
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(1) )

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.destinations = ['cout', 'cerr']
process.MessageLogger.cerr.FwkReport.reportEvery = 1000

PDFName = "pdfSignalPlusBackground"

### Binnning in eta and pt for the efficiency fit
EfficiencyBins = cms.PSet(
    pt     = cms.vdouble(options.ptMin,options.ptMax),
    eta    = cms.vdouble(options.etaMin,options.etaMax)
    )
### Defining the observable as well as the PDF names for each bin (total PDF for fitting pass and fail samples)-> One per pt
EfficiencyBinningSpecification = cms.PSet(
    UnbinnedVariables = cms.vstring("mass"),
    BinnedVariables = cms.PSet(
        EfficiencyBins,
    ),
    BinToPDFmap = cms.vstring([
        "pdfSignalPlusBackground", 
        "*pt_bin0*", "pdfSignalPlusBackgroundb0"
        ])  
    )

### define the selection --> pass the id + eta and pt binning full stop
IDModule = cms.PSet(
    Id = cms.PSet(
        EfficiencyBinningSpecification,
        EfficiencyCategoryAndState = cms.vstring(options.typeID,"pass"),
    )
)

if options.backgroundType == "RooCMSShape":
    OutputFilePrefix+="_RooCMSShape";
elif options.backgroundType == "Exponential":
    OutputFilePrefix+="_Exponential";

if options.signalType != "":
    OutputFilePrefix += "_Alternative";

process.leptonIdTnP = cms.EDAnalyzer("TagProbeFitTreeAnalyzer",
                                     InputFileNames = cms.vstring(inputFileList),
                                     InputDirectoryName = cms.string(dirName),
                                     InputTreeName = cms.string("fitter_tree"),
                                     OutputFileName = cms.string(options.outputDIR+"/"+OutputFilePrefix+".root"),
                                     NumCPU = cms.uint32(1),
                                     SaveWorkspace = cms.bool(True),
                                     floatShapeParameters = cms.bool(options.floatShapeParameters),
                                     binsForMassPlots = cms.uint32(options.binsForMassPlots),
                                     Variables = cms.PSet(
        mass   = cms.vstring("Tag-Probe Mass", "65.0", "115.0", "GeV/c^{2}"),
        pt     = cms.vstring("Probe p_{T}", str(options.ptMin),  str(options.ptMax), "GeV/c"),
        eta    = cms.vstring("Probe #eta",  str(options.etaMin), str(options.etaMax),""),                
        ),
                                     Efficiencies = cms.PSet(IDModule),
                                     Categories = cms.PSet(
        mcTrue         = cms.vstring("MC Truth", "dummy[true=1,false=0]"),
        ),
                                     PDFs = cms.PSet(
        pdfSignalPlusBackground = cms.vstring(
            "BreitWigner::signalPass(mass, mP[91, 89, 93], sP[3.0, 0.0, 7.0])", ## simple Breit Wigner for pass sample
            "BreitWigner::signalFail(mass, mF[91, 89, 93], sF[3.0, 0.0, 7.0])", ## simple Breit Wigner for fail sample
            ),
        pdfSignalPlusBackgroundb0 = cms.vstring(
            "#import "+options.templateFile+":w:signalPassMC", ## need a file with a workspace with signalPass RooDataHist
            "#import "+options.templateFile+":w:signalFailMC", ## need a file with a workspace with signalFail RooDataHist
            "Gaussian::signalPassSmear(mass, mP[0., -1,  1.], sP[0.5, 0., 2.0])", ## Gaussian Convolution
            "Gaussian::signalFailSmear(mass, mF[0., -2,  2.], sF[0.5, 0., 2.0])",
            "FCONV::signalPass(mass, signalPassMC, signalPassSmear)",
            "FCONV::signalFail(mass, signalFailMC, signalFailSmear)",
            ),        
        )
              
                                     )

if options.typeID == "looseid":
    process.leptonIdTnP.Categories.looseid = cms.vstring("Loose ID", "dummy[pass=1,fail=0]");
elif options.typeID == "vetoid":
    process.leptonIdTnP.Categories.vetoid = cms.vstring("Veto ID", "dummy[pass=1,fail=0]");
elif options.typeID == "tightid":
    process.leptonIdTnP.Categories.tightid = cms.vstring("Tight ID", "dummy[pass=1,fail=0]");
elif options.typeID == "mediumid":
    process.leptonIdTnP.Categories.mediumid = cms.vstring("Medium ID", "dummy[pass=1,fail=0]");

if options.backgroundType == "RooCMSShape":
    process.leptonIdTnP.PDFs.pdfSignalPlusBackground.append("RooCMSShape::backgroundPass(mass, alphaPass[80.,60.,100.], betaPass[0.105, 0.,1.0], gammaPass[0.033, -0.1, 0.1], peakPass[91.2, 90, 92])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackground.append("RooCMSShape::backgroundFail(mass, alphaFail[80.,60.,100.], betaFail[0.105, 0.,1.0], gammaFail[0.033, -0.1, 0.5], peakFail[91.2, 90, 92])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackground.append("efficiency[0.8,0,1]");

    process.leptonIdTnP.PDFs.pdfSignalPlusBackgroundb0.append("RooCMSShape::backgroundPass(mass, alphaPass[105.,80.,150.], betaPass[0.01,-1,1], gammaPass[0.05, -0.1, 0.1], peakPass[91.2,90, 92])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackgroundb0.append("RooCMSShape::backgroundFail(mass, alphaFail[125.,80.,150.], betaFail[0.01,-1,1], gammaFail[0.05, -0.1, 0.5], peakFail[91.2,90, 92])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackgroundb0.append("efficiency[0.8,0,1]");

elif options.backgroundType == "Exponential":
    process.leptonIdTnP.PDFs.pdfSignalPlusBackground.append("Exponential::backgroundPass(mass, cP[-0.05, -2.0, 2.0])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackground.append("Exponential::backgroundFail(mass, cF[-0.05, -2.0, 2.0])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackground.append("efficiency[0.8,0,1]");

    process.leptonIdTnP.PDFs.pdfSignalPlusBackgroundb0.append("Exponential::backgroundPass(mass, cP[-0.05, -2.0, 2.0])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackgroundb0.append("Exponential::backgroundFail(mass, cF[-0.05, -2.0, 2.0])");
    process.leptonIdTnP.PDFs.pdfSignalPlusBackgroundb0.append("efficiency[0.8,0,1]");
else:
    sys.exit('Background model are: RooCMSShape or Expnential --> your model not recognized --> return');

process.fit = cms.Path(process.leptonIdTnP)
