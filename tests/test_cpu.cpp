#include "../AnodeDSP.h"
#include <cstdio>
#include <chrono>
#include <vector>
using namespace anode;
int main(){ const double fs=192000; std::vector<double> in(192000); for(size_t i=0;i<in.size();++i) in[i]=0.4*sin(2*M_PI*110*i/fs)+0.2*sin(2*M_PI*331*i/fs);
 for(int pp=1;pp>=0;--pp)for(int pe=1;pe>=0;--pe){ PowerStage st; st.setup(1,pp,pe,fs); st.setControls(.5,.5);
  for(double drive: {1.0, 8.0}){ double acc=0; auto t0=std::chrono::steady_clock::now(); for(double x:in) acc+=st.process(x*drive);
  double ns=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/in.size(); printf("%s %s drive %.0f: %.0f ns (%g)\n",pp?"PP":"SE",pe?"P":"T",drive,ns,acc);} }}
