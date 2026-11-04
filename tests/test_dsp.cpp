#include "../AnodeDSP.h"
#include <cstdio>
#include <vector>
#include <complex>
using namespace anode;
static double harm(const std::vector<double>& y,double f,double fs,int k){ std::complex<double> s=0; for(size_t i=0;i<y.size();++i) s+=y[i]*std::polar(1.0,-2*M_PI*f*k*i/fs); return std::abs(s)*2/y.size(); }
int main(){
  const double fs=192000;
  for(int pp=1;pp>=0;--pp) for(int pe=1;pe>=0;--pe) for(int t=0;t<4;++t){
    PowerStage st; st.setup(t,pp,pe,fs); st.setControls(0.5,0.5);
    auto& c=st.config();
    printf("%s %s %-4s bias=%6.1f rK=%5.0f Iq=%5.1fmA inScale=%.2f |",pp?"PP":"SE",pe?"Pent":"Tri ",tubeName(t),c.vBias,c.rK,st.quiescentCurrent()*1e3,c.inScale);
    for(double a: {0.1,0.3,1.0,3.0,10.0}){
      st.reset(); for(int i=0;i<fs*0.3;++i) st.process(0);
      std::vector<double> y; double f=200;
      int N=(int)(fs*0.5);
      for(int i=0;i<N;++i){ double o=st.process(a*sin(2*M_PI*f*i/fs)); if(i>=N/2) y.push_back(o);} 
      double h1=harm(y,f,fs,1),h2=harm(y,f,fs,2),h3=harm(y,f,fs,3);
      printf(" a=%.2f g=%5.2f H2=%5.1f H3=%5.1f |",a,h1/a,20*log10(h2/h1+1e-12),20*log10(h3/h1+1e-12));
    }
    printf("\n");
  }
}
