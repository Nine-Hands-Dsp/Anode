#include "../AnodeDSP.h"
#include <cstdio>
#include <vector>
#include <chrono>
using namespace anode;
int main(){
  const double fs=192000;
  double fr[]={40,80,120,250,500,1000,2000,4000,8000,12000};
  for(int pp=1;pp>=0;--pp) for(int pe=1;pe>=0;--pe){ int t=1;
   for(double dc : {0.0,0.5,1.0}){
    PowerStage st; st.setup(t,pp,pe,fs); st.setControls(dc,dc);
    printf("%s %s d=c=%.1f:",pp?"PP":"SE",pe?"P":"T",dc);
    for(double f:fr){ st.reset(); double a=0.05,num=0,den=0; int N=(int)(fs*0.25);
      for(int i=0;i<N;++i){double s=sin(2*M_PI*f*i/fs),c=cos(2*M_PI*f*i/fs); double o=st.process(a*s); if(i>N/2){num+=o*s;den+=o*c;}}
      double mag=2*sqrt(num*num+den*den)/(N-N/2-1)/a; printf(" %5.1f",20*log10(mag)); }
    printf("\n"); }}
  // stabilite + CPU
  int bad=0; auto t0=std::chrono::steady_clock::now(); long n=0;
  for(int t=0;t<4;++t)for(int pp=0;pp<2;++pp)for(int pe=0;pe<2;++pe){ PowerStage st; st.setup(t,pp,pe,fs); st.setControls(1,1);
    for(int i=0;i<fs*0.5;++i){ double x=(i%40000<20000? 20.0:0.0)*sin(2*M_PI*82*i/fs)+((i%7)==0?2.0:-0.3); double o=st.process(x); ++n; if(!std::isfinite(o)||fabs(o)>50) {bad++; break;} } }
  double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
  printf("bad=%d  ns/sample=%.1f\n",bad,sec*1e9/n);
}
