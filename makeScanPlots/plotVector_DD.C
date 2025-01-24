#include "../CMS_lumi.h"

int mmed(double mh, int code){
    if (code == 800) return ((int)(mh-80000000000))/10000; 
    if (code == 801) return ((int)(mh-80100000000))/10000; 
    if (code == 805) return ((int)(mh-80500000000))/10000; 
    if (code == 806) return ((int)(mh-80600000000))/10000; 
    return -1;
}

int mdm(double mh, int code){
    if (code == 800) return (mh-80000000000)  - ( ((Int_t)(mh-80000000000))/10000 )*10000;
    if (code == 801) return (mh-80100000000)  - ( ((Int_t)(mh-80100000000))/10000 )*10000;
    if (code == 805) return (mh-80500000000)  - ( ((Int_t)(mh-80500000000))/10000 )*10000;
    if (code == 806) return (mh-80600000000)  - ( ((Int_t)(mh-80600000000))/10000 )*10000;
    return -1;
}

int code(double mh){
    return (int)(mh/100000000);
}

// To convert mMED-mDM in DM-nucleons vs mDM

static float gq  = 0.25;
static float gDM = 1.0;
static float mMED_min = 15;

double vecF(double mMED,double mDM){    
  double mR = (0.939*mDM)/(0.939+mDM);
  double c = 6.9e-41*1e12*pow((gq*gDM)/0.25,2);
  return c*(mR*mR)/(mMED*mMED*mMED*mMED);
}

TGraph * makeOBV(TGraph *Graph1){
  TGraph *gr = new TGraph();
  double X;
  double Y;
  int pp=0;
  Graph1->GetPoint(0,X,Y);
  for (double MDM = 1; MDM < Y; MDM += 0.1){
    gr->SetPoint(pp,MDM,vecF(X,MDM));
    pp++;
  }
  for (int p =0; p < Graph1->GetN(); p++){
    Graph1->GetPoint(p,X,Y);
    if(X <= mMED_min) continue;
    gr->SetPoint(pp,Y,vecF(X,Y));
    pp++;
  }
  gr->SetName(Form("%s_DD",Graph1->GetName()));
  gr->SetLineStyle(Graph1->GetLineStyle());
  gr->SetLineColor(Graph1->GetLineColor());
  gr->SetLineWidth(Graph1->GetLineWidth());
  
  return gr;
}

TGraph* produceContour (const int & reduction){

  TObjArray *lContoursE = (TObjArray*) gROOT->GetListOfSpecials()->FindObject("contours");
  std::vector<double> lXE;
  std::vector<double> lYE;
  int lTotalContsE = lContoursE->GetSize();
  for(int i0 = 0; i0 < lTotalContsE; i0++){
    TList * pContLevel = (TList*)lContoursE->At(i0);
    TGraph *pCurv = (TGraph*)pContLevel->First();
    for(int i1 = 0; i1 < pContLevel->GetSize(); i1++){
      for(int i2  = 0; i2 < pCurv->GetN(); i2++) {
        if(i2%reduction != 0) continue; // reduce number of points                                                                                                                                
        lXE.push_back(pCurv->GetX()[i2]);
        lYE.push_back(pCurv->GetY()[i2]);
      }
      pCurv->SetLineColor(kRed);
      pCurv = (TGraph*)pContLevel->After(pCurv);
    }
  }
  if(lXE.size() == 0) {
    lXE.push_back(0);
    lYE.push_back(0);
  }

  TGraph *lTotalE = new TGraph(lXE.size(),&lXE[0],&lYE[0]);
  return lTotalE;
}

/////                                                                                                                                                                                                 
static float nbinsX = 1000;
static float nbinsY = 600;
static float minX = 0;
static float minY = 1;
static float maxX = 2500;
static float maxY = 1200;
static float minZ = 0.01;
static float maxZ = 10;

static float minX_dd = 1;
static float maxX_dd = 1200;
static double minY_dd = 1e-47;
static double maxY_dd = 1e-25;
static int reductionForContour = 10;
static bool saveOutputFile = true;
static bool addPreliminary = false;

TGraph* superCDMS();
TGraph* lux();
TGraph* cdmslite();
TGraph* panda();
TGraph* xenon();
TGraph* cresst();
TGraph* neutrino_floor();

//////
void plotVector_DD (string inputFileName, string outputDirectory, string coupling = "025", string energy = "13") {

  gROOT->SetBatch(kTRUE);
  system(("mkdir -p "+outputDirectory).c_str());
  setTDRStyle();

  TFile *file = TFile::Open(inputFileName.c_str(),"READ");
  TTree *tree = (TTree*)file->Get("limit");
  
  TGraph2D* grexp = new TGraph2D();
  TGraph2D* grobs = new TGraph2D();
  
  double mh;
  double limit;
  float quantile;
  
  tree->SetBranchAddress("mh",&mh);
  tree->SetBranchAddress("limit",&limit);
  tree->SetBranchAddress("quantileExpected",&quantile);

  // find bad limits
  int currentmedmass = -1;
  int currentdmmass  = -1;
  int npoints = 0;
  vector<pair<int,int> > goodMassPoint;
  for(int i = 0; i < tree->GetEntries(); i++){
    tree->GetEntry(i);

    int c       = code(mh);
    int medmass = mmed(mh, c);
    int dmmass  = mdm(mh, c);

    if(medmass != currentmedmass or dmmass != currentdmmass){
      if(npoints == 6)
        goodMassPoint.push_back(pair<int,int>(currentmedmass,currentdmmass));
      npoints = 0;
      currentmedmass = medmass;
      currentdmmass  = dmmass;
      npoints++;
    }
    else
      npoints++;
  }

  if(npoints == 6)
    goodMassPoint.push_back(pair<int,int>(currentmedmass,currentdmmass));

  
  int expcounter = 0;
  int obscounter = 0;
  double minmass = 100000;

  // main loop
  for (int i = 0; i < tree->GetEntries(); i++){
    
    tree->GetEntry(i);
    
    if (quantile != 0.5 && quantile != -1) continue;

    int c       = code(mh);
    int medmass = mmed(mh,c);
    int dmmass  = mdm(mh,c);

    bool isGoodMassPoint = false;
    for(auto mass : goodMassPoint){
      if(medmass == mass.first and dmmass == mass.second){
        isGoodMassPoint = true;
        break;
      }
    }
    if(not isGoodMassPoint){ // printout bad limits                                                                                                                                                 
      cout<<"Bad limit value: medmass "<<medmass<<" dmmass "<<dmmass<<endl;
      continue;
    }

    // remove some point by hand                                                                                                                                                                    
    if(medmass == 1800 and (dmmass == 200 or dmmass == 250 or dmmass == 350 or dmmass == 400 or dmmass == 800)) continue;
    if(medmass == 1725 and (dmmass == 200 or dmmass > 700)) continue;
    if(medmass == 1525 and dmmass > 600) continue;
    if(medmass == 1125 and dmmass > 600) continue;
    if(medmass == 600  and dmmass == 350) continue;
    if(medmass == 525  and dmmass == 275) continue;
    
    if (quantile == 0.5) {
      expcounter++;
      grexp->SetPoint(expcounter, double(medmass), double(dmmass), limit);
    }
    if (quantile == -1) {
      obscounter++;
      grobs->SetPoint(obscounter, double(medmass), double(dmmass), limit);
      if(medmass <= minmass) minmass = medmass;
    }
  }
  tree->ResetBranchAddresses();

  ///                                                                                                                                                                                                 
  TH2D* hexp = new TH2D("hexp", "", nbinsX, minX, maxX, nbinsY, minY, maxY);
  TH2D* hobs = new TH2D("hobs", "", nbinsX, minX, maxX, nbinsY, minY, maxY);

  // make granularity                                                                                                                                                                                 
  for (int i   = 1; i <= nbinsX; i++) {
    for (int j = 1; j <= nbinsY; j++) {
      hexp->SetBinContent(i,j,grexp->Interpolate(hexp->GetXaxis()->GetBinCenter(i),hexp->GetYaxis()->GetBinCenter(j)));
      hobs->SetBinContent(i,j,grobs->Interpolate(hobs->GetXaxis()->GetBinCenter(i),hobs->GetYaxis()->GetBinCenter(j)));
    }
  }

  // fix mass points below min med mass generated                                                                                                                                                    
  for (int i = 1; i   <= nbinsX; i++) {
    for (int j = 1; j <= nbinsY; j++) {
      if(hexp->GetXaxis()->GetBinCenter(i) < minmass){
        hexp->SetBinContent(i,j,hexp->GetBinContent(hexp->FindBin(minmass,hexp->GetYaxis()->GetBinCenter(j))));
        hobs->SetBinContent(i,j,hobs->GetBinContent(hobs->FindBin(minmass,hobs->GetYaxis()->GetBinCenter(j))));
      }
    }
  }

  for(int i = 0; i < nbinsX; i++){
    for(int j = 0; j < nbinsY; j++){

      if(hexp -> GetBinContent(i,j) <= 0) hexp->SetBinContent(i,j,maxZ);
      if(hobs -> GetBinContent(i,j) <= 0) hobs->SetBinContent(i,j,maxZ);

      if(hexp -> GetBinContent(i,j) > maxZ) hexp->SetBinContent(i,j,maxZ);
      if(hobs -> GetBinContent(i,j) > maxZ) hobs->SetBinContent(i,j,maxZ);

      if(hexp -> GetBinContent(i,j) < minZ) hexp->SetBinContent(i,j,minZ);
      if(hobs -> GetBinContent(i,j) < minZ) hobs->SetBinContent(i,j,minZ);
    }
  }

  hexp->Smooth();
  hobs->Smooth();
  
  TH2* hexp2 = (TH2*)hexp->Clone("hexp2");
  TH2* hobs2 = (TH2*)hobs->Clone("hobs2");

  //////////                                                                                                                                                                                         
  double contours[1]; contours[0]=1;
  hexp2->SetContour(1,contours);
  hobs2->SetContour(1,contours);

  hexp2->Draw("contz list");
  gPad->Update();

  
  TGraph* lTotalE = produceContour(reductionForContour);
  lTotalE->SetLineColor(kBlack);
  lTotalE->SetLineStyle(2);
  lTotalE->SetLineWidth(3);

  hobs2->Draw("contz list");
  gPad->Update();

  TGraph* lTotal = produceContour(reductionForContour);
  lTotal->SetLineColor(kRed);
  lTotal->SetLineWidth(3);
    
  TGraph *DDE_graph = makeOBV(lTotalE);
  TGraph *DD_graph  = makeOBV(lTotal);

  TCanvas* canvas = new TCanvas("canvas","canvas",600,625);
  canvas->SetLogx();
  canvas->SetLogy();

  TH1* frame = canvas->DrawFrame(minX_dd,minY_dd,maxX_dd,maxY_dd,"");
  frame->GetYaxis()->SetTitle("#sigma^{SI}_{DM-nucleon} [cm^{2}]");
  frame->GetXaxis()->SetTitle("m_{DM} [GeV]");
  frame->GetXaxis()->SetRangeUser(1,1000);
  frame->GetXaxis()->SetLabelSize(0.032);
  frame->GetYaxis()->SetLabelSize(0.032);
  frame->GetXaxis()->SetTitleSize(0.042);
  frame->GetYaxis()->SetTitleSize(0.042);
  frame->GetYaxis()->SetTitleOffset(1.75);
  frame->GetYaxis()->SetNdivisions(512);
  frame->GetXaxis()->SetTitleOffset(1.15);
  frame->Draw();

  TGraph *lM0 = lux();
  TGraph *lM1 = cdmslite();
  TGraph *lM2 = xenon();
  TGraph *lM3 = cresst();
  TGraph *lM4 = neutrino_floor();
  TGraph *lM5 = panda();

  lM0->SetLineColor(kBlue);
  lM1->SetLineColor(kBlue+2);
  lM2->SetLineColor(kAzure+1);
  lM3->SetLineColor(kAzure+8);
  lM4->SetLineColor(kGreen+2);
  lM5->SetLineColor(kBlue+3);

  lM0->Draw("L SAME");
  lM1->Draw("L SAME");
  lM2->Draw("L SAME");
  lM3->Draw("L SAME");
  //lM4->Draw("L SAME");
  lM5->Draw("L SAME");

  DDE_graph->Draw("L SAME");
  DD_graph->Draw("L SAME");

  //gPad->SetRightMargin(0.28);
  gPad->SetLeftMargin(0.15);
  gPad->RedrawAxis();
  gPad->Modified();
  gPad->Update();


  TLegend *leg = new TLegend(0.22,0.60,0.85,0.78,NULL,"brNDC");
  leg->SetFillStyle(0);
  leg->SetBorderSize(0);
  leg->SetFillColor(0);
  leg->SetNColumns(2);
  leg->AddEntry(DDE_graph,"CMS exp. 90% CL","L");
  leg->AddEntry(DD_graph ,"CMS obs. 90% CL","L");
  leg->AddEntry(lM0 ,"LUX","L");
  leg->AddEntry(lM1 ,"CDMSLite","L");
  leg->AddEntry(lM2 ,"Xenon-1T","L");
  leg->AddEntry(lM3 ,"CRESST-II","L");
  leg->AddEntry(lM5 ,"PandaX-II","L");
  //leg->AddEntry(lM4 ,"Neutrino floor","L");
  leg->Draw("SAME");

  if(addPreliminary)
    CMS_lumi(canvas,"35.9",false,false,false,0.05,0);
  else
    CMS_lumi(canvas,"35.9",false,true,false,0.05,0);

  canvas->RedrawAxis("samesaxis");

  TLatex * tex = new TLatex();
  tex->SetNDC();
  tex->SetTextFont(42);
  tex->SetLineWidth(2);
  tex->SetTextSize(0.030);
  tex->Draw();
  if (coupling == "1")
    tex->DrawLatex(0.225,0.81,"#bf{Vector med, Dirac DM, g_{q} = 1, g_{DM} = 1}");
  else
    tex->DrawLatex(0.225,0.81,"#bf{Vector med, Dirac DM, g_{q} = 0.25, g_{DM} = 1}");
    
  ///////                                                                                                                                                                                             
  canvas->SaveAs((outputDirectory+"/scanDD_vector_g"+coupling+"_"+energy+"TeV_v1.pdf").c_str(),"pdf");
  canvas->SaveAs((outputDirectory+"/scanDD_vector_g"+coupling+"_"+energy+"TeV_v1.png").c_str(),"png");
  
  if(saveOutputFile){

    TFile*outfile = new TFile((outputDirectory+"/scanDD_vector_g"+coupling+"_"+energy+"TeV_v1.root").c_str(),"RECREATE");
    //hobs2->Write("contour_obs");
    //hexp2->Write("contour_exp");
    //lTotalE->Write("contour_exp_graph");
    //lTotal->Write("contour_obs_graph");
    DDE_graph->Write("expected_dd");
    DD_graph->Write("observed_dd");
    outfile->Write();
    outfile->Close();
  }

}

TGraph *panda(){

  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];

  i0++; lX[i0] = 8.047; lY[i0]=  9.798e-45;
  i0++; lX[i0] = 8.461; lY[i0]=  6.708e-45;
  i0++; lX[i0] = 8.928; lY[i0]=  4.787e-45;
  i0++; lX[i0] = 9.387; lY[i0]=  3.399e-45;
  i0++; lX[i0] = 9.421; lY[i0]=  3.399e-45;
  i0++; lX[i0] = 9.764; lY[i0]=  2.805e-45;
  i0++; lX[i0] = 12.856; lY[i0]=  7.239e-46;
  i0++; lX[i0] = 16.737; lY[i0]=  2.890e-46;
  i0++; lX[i0] = 18.758; lY[i0]=  2.084e-46;
  i0++; lX[i0] = 21.322; lY[i0]=  1.543e-46;
  i0++; lX[i0] = 25.468; lY[i0]=  1.190e-46;
  i0++; lX[i0] = 29.148; lY[i0]=  1.035e-46;
  i0++; lX[i0] = 32.424; lY[i0]=  9.577e-47;
  i0++; lX[i0] = 35.559; lY[i0]=  8.907e-47;
  i0++; lX[i0] = 38.174; lY[i0]=  8.680e-47;
  i0++; lX[i0] = 40.691; lY[i0]=  8.614e-47;
  i0++; lX[i0] = 44.938; lY[i0]=  8.706e-47;
  i0++; lX[i0] = 49.981; lY[i0]=  8.868e-47;
  i0++; lX[i0] = 54.806; lY[i0]=  9.103e-47;
  i0++; lX[i0] = 60.955; lY[i0]=  9.296e-47;
  i0++; lX[i0] = 68.274; lY[i0]=  9.845e-47;
  i0++; lX[i0] = 77.561; lY[i0]=  1.065e-46;
  i0++; lX[i0] = 88.110; lY[i0]=  1.169e-46;
  i0++; lX[i0] = 103.701; lY[i0]=  1.325e-46;
  i0++; lX[i0] = 129.162; lY[i0]=  1.606e-46;
  i0++; lX[i0] = 193.418; lY[i0]=  2.173e-46;
  i0++; lX[i0] = 246.077; lY[i0]=  2.717e-46;
  i0++; lX[i0] = 313.057; lY[i0]=  3.507e-46;
  i0++; lX[i0] = 401.093; lY[i0]=  4.596e-46;
  i0++; lX[i0] = 483.863; lY[i0]=  5.703e-46;
  i0++; lX[i0] = 626.596; lY[i0]=  7.227e-46;
  i0++; lX[i0] = 855.682; lY[i0]=  9.723e-46;
  i0++; lX[i0] = 1236.581; lY[i0]=  1.422e-45;
  i0++; lX[i0] = 1724.982; lY[i0]=  1.923e-45;
  i0++; lX[i0] = 2306.110; lY[i0]=  2.546e-45;
  i0++; lX[i0] = 3428.606; lY[i0]=  3.685e-45;
  i0++; lX[i0] = 4748.818; lY[i0]=  5.088e-45;
  i0++; lX[i0] = 6484.752; lY[i0]=  7.024e-45;
  i0++; lX[i0] = 8792.397; lY[i0]=  9.902e-45;

  /*
  i0++; lX[i0] =4.970 ; lY[i0]= 9.731e-43;
  i0++; lX[i0] =6.285 ; lY[i0]= 9.376e-44;
  i0++; lX[i0] =7.717 ; lY[i0]= 1.870e-44;
  i0++; lX[i0] =10.173 ; lY[i0]= 3.748e-45;
  i0++; lX[i0] =14.394 ; lY[i0]= 9.809e-46;
  i0++; lX[i0] =16.305 ; lY[i0]= 6.979e-46;
  i0++; lX[i0] =19.347 ; lY[i0]= 4.712e-46;
  i0++; lX[i0] =23.526 ; lY[i0]= 3.405e-46;
  i0++; lX[i0] =34.701 ; lY[i0]= 2.511e-46;
  i0++; lX[i0] =42.506 ; lY[i0]= 2.420e-46;
  i0++; lX[i0] =61.783 ; lY[i0]= 2.713e-46;
  i0++; lX[i0] =90.243 ; lY[i0]= 3.430e-46;
  i0++; lX[i0] =126.139 ; lY[i0]= 4.406e-46;
  i0++; lX[i0] =185.600 ; lY[i0]= 5.932e-46;
  i0++; lX[i0] =321.685 ; lY[i0]= 9.791e-46;
  i0++; lX[i0] =457.401 ; lY[i0]= 1.368e-45;
  i0++; lX[i0] =701.572 ; lY[i0]= 2.188e-45;
  i0++; lX[i0] =990.270 ; lY[i0]= 3.323e-45;
  */

  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;
}

TGraph *cdmslite(){
  
  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];
  i0++; lX[i0] =1.429 ; lY[i0]= 9.880e-38;
  i0++; lX[i0] =1.473 ; lY[i0]= 3.162e-38;
  i0++; lX[i0] =1.574 ; lY[i0]= 1.075e-38;
  i0++; lX[i0] =1.654 ; lY[i0]= 4.334e-39;
  i0++; lX[i0] =1.731 ; lY[i0]= 1.924e-39;
  i0++; lX[i0] =1.838 ; lY[i0]= 8.338e-40;
  i0++; lX[i0] =1.986 ; lY[i0]= 3.279e-40;
  i0++; lX[i0] =2.169 ; lY[i0]= 1.566e-40;
  i0++; lX[i0] =2.384 ; lY[i0]= 8.338e-41;
  i0++; lX[i0] =2.632 ; lY[i0]= 5.012e-41;
  i0++; lX[i0] =2.857 ; lY[i0]= 3.527e-41;
  i0++; lX[i0] =3.093 ; lY[i0]= 2.701e-41;
  i0++; lX[i0] =3.415 ; lY[i0]= 2.120e-41;
  i0++; lX[i0] =3.827 ; lY[i0]= 1.789e-41;
  i0++; lX[i0] =4.207 ; lY[i0]= 1.566e-41;
  i0++; lX[i0] =4.625 ; lY[i0]= 1.492e-41;
  i0++; lX[i0] =5.106 ; lY[i0]= 1.438e-41;
  i0++; lX[i0] =5.613 ; lY[i0]= 1.321e-41;
  i0++; lX[i0] =6.157 ; lY[i0]= 1.143e-41;
  i0++; lX[i0] =6.768 ; lY[i0]= 9.527e-42;
  i0++; lX[i0] =7.504 ; lY[i0]= 8.040e-42;
  i0++; lX[i0] =8.161 ; lY[i0]= 7.122e-42;
  i0++; lX[i0] =9.009 ; lY[i0]= 6.387e-42;
  i0++; lX[i0] =9.968 ; lY[i0]= 5.796e-42;
  i0++; lX[i0] =11.100 ; lY[i0]= 5.261e-42;
  i0++; lX[i0] =12.307 ; lY[i0]= 5.073e-42;
  i0++; lX[i0] =13.645 ; lY[i0]= 4.775e-42;
  i0++; lX[i0] =14.871 ; lY[i0]= 4.892e-42;
  
  
  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;
  
}


TGraph *superCDMS() {
  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];
  i0++; lX[i0] = 3.5946953342351033; lY[i0]= 9.848667679721256e-40;
  i0++; lX[i0] = 3.75095871431554;   lY[i0] = 4.7369269651270765e-40;
  i0++; lX[i0] = 4.055296786109914;  lY[i0] = 1.731452556791703e-40;
  i0++; lX[i0] = 4.859185478655973;  lY[i0] = 3.335637195211155e-41;
  i0++; lX[i0] = 5.443071002530319;  lY[i0] = 1.4979478319670555e-41;
  i0++; lX[i0] = 6.568479751568511;  lY[i0] = 4.257347230887382e-42;
  i0++; lX[i0] = 7.759712545805536;  lY[i0] = 1.4200965776277166e-42;
  i0++; lX[i0] = 10.195931085528889; lY[i0] = 5.073387251235342e-43;
  i0++; lX[i0] = 13.636666866311272; lY[i0] = 2.930133564061426e-43;
  i0++; lX[i0] = 18.04552501288121;  lY[i0] = 2.0321090962591862e-43;
  i0++; lX[i0] = 23.377104302070727; lY[i0] = 1.941241066769181e-43;
  i0++; lX[i0] = 29.857390063416926; lY[i0] = 2.0321090962591862e-43;
  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;
}


TGraph *cresst(){

  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];
    
  i0++; lX[i0] = 0.502; lY[i0] =  1.929e-36;
  i0++; lX[i0] = 0.535; lY[i0] =  1.164e-36;
  i0++; lX[i0] = 0.566; lY[i0] =  6.032e-37;
  i0++; lX[i0] = 0.603; lY[i0] =  2.755e-37;
  i0++; lX[i0] = 0.638; lY[i0] =  1.428e-37;
  i0++; lX[i0] = 0.675; lY[i0] =  7.402e-38;
  i0++; lX[i0] = 0.720; lY[i0] =  4.036e-38;
  i0++; lX[i0] = 0.767; lY[i0] =  2.257e-38;
  i0++; lX[i0] = 0.811; lY[i0] =  1.468e-38;
  i0++; lX[i0] = 0.879; lY[i0] =  9.317e-39;
  i0++; lX[i0] = 0.942; lY[i0] =  6.708e-39;
  i0++; lX[i0] = 1.004; lY[i0] =  4.953e-39;
  i0++; lX[i0] = 1.106; lY[i0] =  3.390e-39;
  i0++; lX[i0] = 1.242; lY[i0] =  2.206e-39;
  i0++; lX[i0] = 1.405; lY[i0] =  1.629e-39;
  i0++; lX[i0] = 1.517; lY[i0] =  1.331e-39;
  i0++; lX[i0] = 1.727; lY[i0] =  1.087e-39;
  i0++; lX[i0] = 2.000; lY[i0] =  8.442e-40;
  i0++; lX[i0] = 2.159; lY[i0] =  7.073e-40;
  i0++; lX[i0] = 2.339; lY[i0] =  5.223e-40;
  i0++; lX[i0] = 2.483; lY[i0] =  3.760e-40;
  i0++; lX[i0] = 2.992; lY[i0] =  1.237e-40;
  i0++; lX[i0] = 3.285; lY[i0] =  6.914e-41;
  i0++; lX[i0] = 3.523; lY[i0] =  4.978e-41;
  i0++; lX[i0] = 4.273; lY[i0] =  3.079e-41;
  i0++; lX[i0] = 5.166; lY[i0] =  2.274e-41;
  i0++; lX[i0] = 6.184; lY[i0] =  1.637e-41;
  i0++; lX[i0] = 7.703; lY[i0] =  1.209e-41;
  i0++; lX[i0] = 10.393; lY[i0] =  1.065e-41;
  i0++; lX[i0] = 13.654; lY[i0] =  1.149e-41;
  i0++; lX[i0] = 17.466; lY[i0] =  1.304e-41;
  i0++; lX[i0] = 23.330; lY[i0] =  1.637e-41;
  i0++; lX[i0] = 26.476; lY[i0] =  1.480e-41;
  i0++; lX[i0] = 29.748; lY[i0] =  9.628e-42;
     
  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;

}


TGraph *lux(){

  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];
  
  i0++; lX[i0] = 5.230202675339297; lY[i0] =    9.999999999999836e-43;
  i0++; lX[i0] = 5.320175096324759; lY[i0] =    7.906043210907605e-43;
  i0++; lX[i0] = 5.411695265464636; lY[i0] =      6.105402296585314e-43;
  i0++; lX[i0] = 5.50478980785497; lY[i0] =       4.4984326689693795e-43;
  i0++; lX[i0] = 5.599485806609297; lY[i0] =     3.393221771895274e-43;
  i0++; lX[i0] = 5.695810810737687; lY[i0] =     2.6203985288583326e-43;
  i0++; lX[i0] = 5.793792843161313; lY[i0] =     2.0716983998953036e-43;
  i0++; lX[i0] = 5.893460408864902; lY[i0] =     1.5264179671752116e-43;
  i0++; lX[i0] = 5.99484250318941; lY[i0] =      1.2067926406393165e-43;
  i0++; lX[i0] = 6.202868761603334; lY[i0] =     8.891593339164587e-44;
  i0++; lX[i0] = 6.309573444801932; lY[i0] =     6.707035611184258e-44;
  i0++; lX[i0] = 6.418113712446068; lY[i0] =     4.941713361323818e-44;
  i0++; lX[i0] = 6.6408278506348415; lY[i0] =    3.556480306223092e-44;
  i0++; lX[i0] = 6.989473207273485; lY[i0] =     2.500110382617941e-44;
  i0++; lX[i0] = 7.232014017781835; lY[i0] =     1.7575106248547966e-44;
  i0++; lX[i0] = 7.611696816847943; lY[i0] =     1.0481131341546917e-44;
  i0++; lX[i0] = 8.011313071180073; lY[i0] =     7.19685673001147e-45;
  i0++; lX[i0] = 8.576958985908941; lY[i0] =     4.941713361323818e-45;
  i0++; lX[i0] = 9.027251779484576; lY[i0] =     3.641031949310677e-45;
  i0++; lX[i0] = 9.501185073181436; lY[i0] =     2.7464741148160504e-45;
  i0++; lX[i0] = 10.347008713411984; lY[i0] =    1.8858632787726555e-45;
  i0++; lX[i0] = 11.268130079648545; lY[i0] =    1.2354828882567482e-45;
  i0++; lX[i0] = 12.697075549188032; lY[i0] =    7.722449945836333e-46;
  i0++; lX[i0] = 14.803703235666655; lY[i0] =    4.826957437677881e-46;
  i0++; lX[i0] = 16.398903672222477; lY[i0] =    3.4738921120831546e-46;
  i0++; lX[i0] = 19.448624389373624; lY[i0] =    2.4420530945486748e-46;
  i0++; lX[i0] = 22.67543125870801; lY[i0] =     1.930697728883277e-46;
  i0++; lX[i0] = 26.892404165083185; lY[i0] =    1.4563484775012623e-46;
  i0++; lX[i0] = 31.89361179185746; lY[i0] =     1.2354828882567684e-46;
  i0++; lX[i0] = 39.13745601980384; lY[i0] =     1.0481131341546917e-46;
  i0++; lX[i0] = 44.85923383437637; lY[i0] =     1.073030940526174e-46;
  i0++; lX[i0] = 57.937928431613145; lY[i0] =    1.2067926406393363e-46;
  i0++; lX[i0] = 78.75829328777596; lY[i0] =     1.422529313485372e-46;
  i0++; lX[i0] = 100; lY[i0] =                   1.6768329368110306e-46;
  i0++; lX[i0] = 129.1549665014884; lY[i0] =     2.0235896477251638e-46;
  i0++; lX[i0] = 161.21572750178856; lY[i0] =    2.4420530945486748e-46;
  i0++; lX[i0] = 204.696827180752; lY[i0] =      3.0888435964774975e-46;
  i0++; lX[i0] = 264.3761185749101; lY[i0] =     3.906939937054621e-46;
  i0++; lX[i0] = 335.68035509467273; lY[i0] =    5.059197488435864e-46;
  i0++; lX[i0] = 426.2158829015325; lY[i0] =     6.250551925274027e-46;
  i0++; lX[i0] = 532.0175096324763; lY[i0] =     7.906043210907734e-46;
  i0++; lX[i0] = 652.8521141127848; lY[i0] =     1e-45;
  i0++; lX[i0] = 787.5829328777596; lY[i0] =     1.2067926406393363e-45;
  i0++; lX[i0] = 983.0884473994828; lY[i0] =     1.4909716571840752e-45;
  i0++; lX[i0] = 1248.2348288165172; lY[i0] =    1.976598071701651e-45;
  i0++; lX[i0] = 1584.893192461114; lY[i0] =     2.500110382617941e-45;
  i0++; lX[i0] = 1978.3188827841623; lY[i0] =    3.088843596477498e-45;
  i0++; lX[i0] = 2386.5897868585835; lY[i0] =    3.727593720314923e-45;
  i0++; lX[i0] = 2879.116638022353; lY[i0] =     4.498432668969453e-45;
  i0++; lX[i0] = 3593.813663804626; lY[i0] =     5.557736586486892e-45;
  i0++; lX[i0] = 4485.923383437636; lY[i0] =     6.866488450043026e-45;
  i0++; lX[i0] = 5994.8425031894085; lY[i0] =    9.102981779915264e-45;
  i0++; lX[i0] = 7875.829328777596; lY[i0] =     1.2354828882567482e-44;
  i0++; lX[i0] = 11461.96978456578; lY[i0] =     1.7575106248547966e-44;
  i0++; lX[i0] = 16967.959918688968; lY[i0] =   2.6203985288583324e-44;
  i0++; lX[i0] = 26437.61185749096; lY[i0] =    4.0949150623803854e-44;
  i0++; lX[i0] = 35330.366949927295; lY[i0] =   5.557736586486892e-44;
  i0++; lX[i0] = 48026.56010546725; lY[i0] =    7.543120063354607e-44;
  i0++; lX[i0] = 68712.70363478754; lY[i0] =    1.0730309405261566e-43;
  i0++; lX[i0] = 98308.84473994808; lY[i0] =    1.4909716571840507e-43;
    
  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;
    
}

TGraph* neutrino_floor(){

  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];
  
  i0++; lX[i0] = 0.30482158919260427; lY[i0] = 6.299697963699192e-44;
  i0++; lX[i0] = 0.3603946561512068;  lY[i0] =  4.0949150623804635e-44;
  i0++; lX[i0] = 0.43984751685789686; lY[i0] = 5.779692884153241e-44;
  i0++; lX[i0] = 0.5200746924731524; lY[i0] =  4.4633389000179914e-44;
  i0++; lX[i0] = 0.6411993957422804; lY[i0] =  2.7789296403176915e-44;
  i0++; lX[i0] = 0.7988323339256767; lY[i0] =  1.5873776897913385e-44;
  i0++; lX[i0] = 1.0162579107120813; lY[i0] =  8.318940674880885e-45;
  i0++; lX[i0] = 1.2793860690880594; lY[i0] =  4.3596917354484964e-45;
  i0++; lX[i0] = 1.7516637252854315; lY[i0] =  2.958614867885203e-45;
  i0++; lX[i0] = 2.2289068055528998; lY[i0] =  2.5999559286685708e-45;
  i0++; lX[i0] = 2.8964966553873484; lY[i0] =  2.833876712454476e-45;
  i0++; lX[i0] = 4.268822489777; lY[i0] =      3.999823395608993e-45;
  i0++; lX[i0] = 5.724444577448171; lY[i0] =   4.1758827810524175e-45;
  i0++; lX[i0] = 6.839499775885055; lY[i0] =   2.833876712454476e-45;
  i0++; lX[i0] = 8.17073546776026; lY[i0] =    1.4225293134853725e-45;
  i0++; lX[i0] = 8.97476948447229; lY[i0] =    4.845896673409914e-46;
  i0++; lX[i0] = 9.25839860916086; lY[i0] =    2.2317187169684525e-46;
  i0++; lX[i0] = 9.550140289895431; lY[i0] =   8.286427728546935e-47;
  i0++; lX[i0] = 9.851777264155823; lY[i0] =   3.6553180422141624e-47;
  i0++; lX[i0] = 10.162941309378429; lY[i0] =  1.612437883662069e-47;
  i0++; lX[i0] = 10.484120160024952; lY[i0] =  7.425886368281569e-48;
  i0++; lX[i0] = 10.814485589904205; lY[i0] =  2.7572502862167695e-48;
  i0++; lX[i0] = 11.157050406504503; lY[i0] =  1.5085907086001826e-48;
  i0++; lX[i0] = 11.509440928960965; lY[i0] =  6.654711796333953e-49;
  i0++; lX[i0] = 12.251893114554521; lY[i0] =  2.811768697974256e-49;;
  i0++; lX[i0] = 14.637115109229152; lY[i0] =  1.5384196906973548e-49;
  i0++; lX[i0] = 18.046079725235675; lY[i0] =  9.578390020327465e-50;
  i0++; lX[i0] = 22.722595717571664; lY[i0] =  7.722449945836317e-50;
  i0++; lX[i0] = 31.44380379973561; lY[i0] =   8.062367401460673e-50;
  i0++; lX[i0] = 42.61450002802649; lY[i0] =   1.089971057280838e-49;
  i0++; lX[i0] = 64.13479538097116; lY[i0] =   1.5384196906973548e-49;
  i0++; lX[i0] = 96.52970659121127; lY[i0] =   2.5796728079999194e-49;
  i0++; lX[i0] = 132.19839802285773; lY[i0] =  3.340484983513337e-49;
  i0++; lX[i0] = 235.2858107825558; lY[i0] =   5.365273145287781e-49;
  i0++; lX[i0] = 401.5785603427742; lY[i0] =   8.996666725006114e-49;
  i0++; lX[i0] = 989.1533688824095; lY[i0] =   1.953513093877141e-48;
  i0++; lX[i0] =  1488.6751897112276; lY[i0] =  2.7572502862167695e-48;
  i0++; lX[i0] = 2312.1664768828095; lY[i0] =  4.6234480949599336e-48;
  i0++; lX[i0] = 3516.4629174691013; lY[i0] =  6.525681155193038e-48;
  i0++; lX[i0] = 4866.731114703792; lY[i0] =   9.210553176894823e-48;
  i0++; lX[i0] = 6459.013016462642; lY[i0] =   1.3000066630116841e-47;
  i0++; lX[i0] = 10031.23380395163; lY[i0] =   1.8348706005132336e-47;

  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;
  
 
}

TGraph *xenon(){
  
  int i0 = -1;
  double *lX = new double[1000];
  double *lY = new double[1000];
  
  i0++; lX[i0] = 6.2662857571501345; lY[i0] = 8.543266059341143e-44;
  i0++; lX[i0] = 6.546165237459862; lY[i0] = 6.138103229381494e-44;
  i0++; lX[i0] = 6.779666254444859; lY[i0] = 4.273357542109263e-44;
  i0++; lX[i0] = 7.083000795258592; lY[i0] = 2.837863178135969e-44;
  i0++; lX[i0] = 7.464615600011724; lY[i0] = 1.826158468270242e-44;
  i0++; lX[i0] = 7.9359351889724685; lY[i0] = 1.0861690808398076e-44;
  i0++; lX[i0] = 8.364000229168054; lY[i0] = 6.562873255542149e-45;
  i0++; lX[i0] = 9.047496326054578; lY[i0] = 4.028359352386536e-45;
  i0++; lX[i0] = 9.787282397354216; lY[i0] = 2.358574357958542e-45;
  i0++; lX[i0] = 10.773534028219355; lY[i0] = 1.2966432776590116e-45;
  i0++; lX[i0] = 11.653761493745419; lY[i0] = 8.085228676856251e-46;
  i0++; lX[i0] = 13.051491437918491; lY[i0] = 4.885273571519322e-46;
  i0++; lX[i0] = 14.744023607773366; lY[i0] = 2.9986313485755303e-46;
  i0++; lX[i0] = 16.94685704302921; lY[i0] = 1.929612418460103e-46;
  i0++; lX[i0] = 19.309090031421555; lY[i0] = 1.3433993325988877e-46;
  i0++; lX[i0] = 21.99798577079349; lY[i0] = 1.0608183551394396e-46;
  i0++; lX[i0] = 25.498135022600614; lY[i0] = 9.062853448588874e-47;
  i0++; lX[i0] = 29.81276216010585; lY[i0] = 7.742636826811214e-47;
  i0++; lX[i0] = 34.85334634119006; lY[i0] = 7.502632520822492e-47;
  i0++; lX[i0] = 42.366398703467055; lY[i0] = 7.681925401780325e-47;
  i0++; lX[i0] = 49.31031717163866; lY[i0] = 8.245923781774311e-47;
  i0++; lX[i0] = 58.65047988876358; lY[i0] = 8.921283702446966e-47;
  i0++; lX[i0] = 70.36461542082742; lY[i0] = 1.0118781198504426e-46;
  i0++; lX[i0] = 83.68907499954075; lY[i0] = 1.147701792233362e-46;
  i0++; lX[i0] = 102.15715500762107; lY[i0] = 1.3647174196395528e-46;
  i0++; lX[i0] = 127.98931328428989; lY[i0] = 1.622767907196007e-46;
  i0++; lX[i0] = 154.8790045783498; lY[i0] = 1.9913394573407126e-46;
  i0++; lX[i0] = 200.88081263549006; lY[i0] = 2.5618105424761687e-46;
  i0++; lX[i0] = 249.49149885765476; lY[i0] = 3.193548412908898e-46;
  i0++; lX[i0] = 315.2849237925243; lY[i0] = 4.044246393116117e-46;
  i0++; lX[i0] = 391.58580600176094; lY[i0] = 4.962796825169922e-46;
  i0++; lX[i0] = 490.57577025301464; lY[i0] = 6.284787504342158e-46;
  i0++; lX[i0] = 583.4467421081572; lY[i0] = 7.473159878245431e-46;
  i0++; lX[i0] = 687.904332649574; lY[i0] = 8.886238162743408e-46;
  i0++; lX[i0] = 892.2373394747145; lY[i0] = 1.1253355826007646e-45;
  i0++; lX[i0] = 1167.2979372125035; lY[i0] = 1.494028817264169e-45;
  i0++; lX[i0] = 1437.2660133277113; lY[i0] = 1.8333604707298815e-45;
  i0++; lX[i0] = 1754.3565126041958; lY[i0] = 2.2854638641349838e-45;
  i0++; lX[i0] = 2197.877322459486; lY[i0] = 2.849055140905999e-45;
  i0++; lX[i0] = 2826.141743698262; lY[i0] = 3.5516270124862015e-45;
  i0++; lX[i0] = 3665.501539159193; lY[i0] = 4.6415888336127535e-45;
  i0++; lX[i0] = 4552.440310089992; lY[i0] = 5.878016072274875e-45;
  i0++; lX[i0] = 5753.050071199337; lY[i0] = 7.327524259667632e-45;
  i0++; lX[i0] = 6961.778709595096; lY[i0] = 8.921283702446965e-45;
  i0++; lX[i0] = 8497.879610544662; lY[i0] = 1.0861690808398076e-44;
  i0++; lX[i0] = 9594.342745086236; lY[i0] = 1.2319647754934755e-44;

  TGraph *lLimit = new TGraph(i0,lX,lY);
  lLimit->SetLineWidth(3.);
  return lLimit;

}
