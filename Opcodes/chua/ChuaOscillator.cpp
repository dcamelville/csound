/*
  This file is part of Csound.

  The Csound Library is free software; you can redistribute it
  and/or modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  Csound is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Csound; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307 USA
*/
// function TimeSeries = chuacc(L,R0,C2,G,Ga,Gb,C1,E,x0,y0,z0,dataset_size,step_size)
// %0.00945,7.5,2e-007,0.00105,0,-0.00121,1.5e-008,1.76e-005,0,-0.1,0.1,2500,5e-6
// % Syntax: TimeSeries=chua(L,R0,C2,G,Ga,Gb,C1,E,x0,y0,z0,dataset_size,step_size)
// % ______________________________________
// %
// % Chua's oscillator is described by a  set  of  three
// % ordinary differential equations called Chua's equations:
// %
// %      dI3      R0      1
// %      --- =  - -- I3 - - V2
// %      dt       L       L
// %
// %      dV2    1       G
// %      --- = -- I3 - -- (V2 - V1)
// %      dt    C2      C2
// %
// %      dV1    G              1
// %      --- = -- (V2 - V1) - -- f(V1)
// %      dt    C1             C1
// %
// % where
// %
// %   f(V1) = Gb V1 + - (Ga - Gb)(|V1 + E| - |V1 - E|)
// %
// % A solution of these equations (I3,V2,V1)(t) starting from an
// % initial state (I3,V2,V1)(0) is called a trajectory of Chua's
// % oscillator.
// %
// % This function uses a RungeKutta integration method optimised for the chua
// % paradigm
// %
// % Reference:
// % ABC - Adventures in Bifurication & Chaos ... Prof M.P Kennedy 1993
// %
// % James McEvoy, Tom Murray
// % 
// % University e-mail: 99375940@student.ucc.ie
// % Lifetime e-mail: sacevoy@eircom.net
// % Homepage: http://www.sacevoy.com
// %
// % 2 Nov 2002
// % Models Initial Variables
// %-------------------------
// %Initial Conditions set by x0,y0,z0
// TimeSeries = [x0, y0, z0]'; % models initial conditions 
//                             %x0 = I3, y0 = V2, z0 = V1 from datafiles
// % Optimized Runge-Kutta Variables
// %--------------------------------
// h = step_size; %*(G/C2);
// h2 = (h)*(.5);
// h6 = (h)/(6);
// sys_variables(9:12) = [1.76e-005,0,-0.00121,0];
// k1 = [0 0 0]';
// k2 = [0 0 0]';
// k3 = [0 0 0]';
// k4 = [0 0 0]';
// M = [0 0 0]';
// % Calculate Time Series
// %----------------------
// % values have to be switched around 
// M(1) = TimeSeries(3);   %V1
// M(2) = TimeSeries(2);   %V2 
// M(3) = TimeSeries(1); %I3

// for i=1:dataset_size
//     % Runge Kutta
//     % Round One
//     k1(1) = (G*(M(2) - M(1)) - gnor(M(1),sys_variables))/C1;
//     k1(2) = (G*(M(1) - M(2)) + M(3))/C2;
//     k1(3) = (-(M(2) + R0*M(3)))/L;
//     % Round Two
//     k2(1) = (G*(M(2) + h2*k1(2) - (M(1) + h2*k1(1))) - gnor(M(1) + h2*k1(1),sys_variables))/C1;
//     k2(2) = (G*(M(1) + h2*k1(1) - (M(2) + h2*k1(2))) + M(3) + h2*k1(3))/C2;
//     k2(3) = (-(M(2) + h2*k1(2) + R0*(M(3) + h2*k1(3))))/L;
//     % Round Three
//     k3(1) = (G*(M(2) + h2*k2(2) - (M(1) + h2*k2(1))) - gnor(M(1) + h2*k2(1),sys_variables))/C1;
//     k3(2) = (G*(M(1) + h2*k2(1) - (M(2) + h2*k2(2))) + M(3) + h2*k2(3))/C2;
//     k3(3) = (-(M(2) + h2*k2(2) + R0*(M(3) + h2*k2(3))))/L;
//     % Round Four
//     k4(1) = (G*(M(2) + h*k3(2) - (M(1) + h*k3(1))) - gnor(M(1) + h*k3(1),sys_variables))/C1;
//     k4(2) = (G*(M(1) + h*k3(1) - (M(2) + h*k3(2))) + M(3) + h*k3(3))/C2;
//     k4(3) = (-(M(2) + h*k3(2) + R0*(M(3) + h*k3(3))))/L;    
//     M = M + (k1 + 2*k2 + 2*k3 + k4)*(h6); %Finishes integration and assigns values to M(1),
//                                           %M(2) and M(3)      
//     TimeSeries(3,i+1) = M(1);  %TimeSeries 3 is V1
//     TimeSeries(2,i+1) = M(2);  %TimeSeries 2 is V2
//     TimeSeries(1,i+1) = M(3); %TimeSeries 1 is I3    
//     i=i+1;
// end
// %gnor Calculates the cubic nonlinearity
// function gnor =  gnor(x,sys_variables)
//     a = sys_variables(9);
//     b = sys_variables(10);
//     c = sys_variables(11);
//     d = sys_variables(12);
//     gnor = a*(x.^3) + b*(x.^2) + c*x + d;

#include <OpcodeBase.hpp>
#include <boost/numeric/ublas/vector.hpp>
using namespace boost::numeric;
#include <cmath>

struct ChuasOscillatorCubic : public OpcodeBase<ChuasOscillatorCubic>
{
  // OUTPUTS
  MYFLT *I3;
  MYFLT *V2;
  MYFLT *V1;
  // INPUTS
  // function TimeSeries = chuacc(L,R0,C2,G,Ga,Gb,C1,E,x0,y0,z0,dataset_size,step_size)
  // Circuit elements.
  MYFLT *L_;
  MYFLT *R0_;
  MYFLT *C2_;
  MYFLT *G_;
  // Omit Ga, not used with cubic nonlinearity.
  // Omit Gb, not used with cubic nonlinearity.
  MYFLT *C1_;
  // Omit E, not used with cubic nonlinearity.
  // Initial values...
  // %x0 = I3, y0 = V2, z0 = V1 from datafiles
  MYFLT *I3_;
  MYFLT *V2_;
  MYFLT *V1_;
  MYFLT *step_size_;
  // STATE
  // Runge-Kutta step sizes.
  MYFLT h;
  MYFLT h2;
  MYFLT h6;
  // Runge-Kutta slopes.
  ublas::vector<MYFLT> k1;
  ublas::vector<MYFLT> k2;
  ublas::vector<MYFLT> k3;
  ublas::vector<MYFLT> k4;
  // Temporary value.
  ublas::vector<MYFLT> M;
  // Polynomial for nonlinear element.
  MYFLT a;
  MYFLT b;
  MYFLT c;
  MYFLT d;
  size_t ksmps;
public:
  int init(CSOUND *csound)
  {
    // h = step_size; %*(G/C2);
    h = *step_size_;
    // h2 = (h)*(.5);
    h2 = h / 2.0;
    // h6 = (h)/(6);
    h6 = h / 6.0;
    // NOTE: The original MATLAB code uses 1-based indexing.
    //       Although the MATLAB vectors are columns, 
    //       these are rows; it doesn't matter here.
    // k1 = [0 0 0]';
    k1.resize(4);
    // k2 = [0 0 0]';
    k2.resize(4);
    // k3 = [0 0 0]';
    k3.resize(4);
    // k4 = [0 0 0]';
    k4.resize(4);
    // M = [0 0 0]';
    M.resize(4);
    // % values have to be switched around 
    // M(1) = TimeSeries(3); %V1
    M(1) = *V1_;
    // M(2) = TimeSeries(2); %V2 
    M(2) = *V2_;
    // M(3) = TimeSeries(1); %I3
    M(3) = *I3_;
    // sys_variables(9:12) = [1.76e-005,0,-0.00121,0];
    a = 1.76e-005;
    b = 0.0;
    c = -0.00121;
    d = 0.0;
    ksmps = csound->GetKsmps(csound);
    return OK;
  }
  MYFLT gnor(double x)
  {
    // No need to compute zero coefficients.
    return a * std::pow(x, 3.0) /* + b * std::pow(x, 2.0) */ + c * x /* + d */;
  }
  int kontrol(CSOUND *csound)
  {
    // NOTE: MATLAB code goes into ublas C++ code pretty straightforwardly,
    //       probaby by design. This is very handy and should prevent mistakes.
    // Start with aliases for the Csound inputs, in order
    // to preserve the clarity of the original code.
    MYFLT &L = *L_;
    MYFLT &R0 = *R0_;
    MYFLT &C2 = *C2_;
    MYFLT &G = *G_;
    MYFLT &C1 = *C1_;
    // Recompute Runge-Kutta step sizes if necessary.
    if (h != *step_size_) {
       // h = step_size; %*(G/C2);
      h = *step_size_;
      // h2 = (h)*(.5);
      h2 = h / 2.0;
      // h6 = (h)/(6);
      h6 = h / 6.0;
    }
    // Standard 4th-order Runge-Kutta integration.
    for (size_t i = 0; i < ksmps; i++) {
      // Stage 1.
      //     k1(1) = (G*(M(2) - M(1)) - gnor(M(1),sys_variables))/C1;
      k1(1) = (G * (M(2) - M(1)) - gnor(M(1))) / C1;
      //     k1(2) = (G*(M(1) - M(2)) + M(3))/C2;
      k1(2) = (G * (M(1) - M(2)) + M(3)) / C2;
      //     k1(3) = (-(M(2) + R0*M(3)))/L;
      k1(3) = (-(M(2) + R0 * M(3))) / L;
      // Stage 2.
      //     k2(1) = (G*(M(2) + h2*k1(2) - (M(1) + h2*k1(1))) - gnor(M(1) + h2*k1(1),sys_variables))/C1;
      k2(1) = (G * (M(2) + h2 * k1(2) - (M(1) + h2 * k1(1))) - gnor(M(1) + h2 * k1(1))) / C1;
      //     k2(2) = (G*(M(1) + h2*k1(1) - (M(2) + h2*k1(2))) + M(3) + h2*k1(3))/C2;
      k2(2) = (G * (M(1) + h2 * k1(1) - (M(2) + h2 * k1(2))) + M(3) + h2 * k1(3)) / C2;
      //     k2(3) = (-(M(2) + h2*k1(2) + R0*(M(3) + h2*k1(3))))/L;
      k2(3) = (-(M(2) + h2 * k1(2) + R0 * (M(3) + h2 * k1(3)))) / L;
      // Stage 3.
      //     k3(1) = (G*(M(2) + h2*k2(2) - (M(1) + h2*k2(1))) - gnor(M(1) + h2*k2(1),sys_variables))/C1;
      k3(1) = (G * (M(2) + h2 * k2(2) - (M(1) + h2 * k2(1))) - gnor(M(1) + h2 * k2(1))) / C1;
      //     k3(2) = (G*(M(1) + h2*k2(1) - (M(2) + h2*k2(2))) + M(3) + h2*k2(3))/C2;
      k3(2) = (G * (M(1) + h2 * k2(1) - (M(2) + h2 * k2(2))) + M(3) + h2 * k2(3)) / C2;
      //     k3(3) = (-(M(2) + h2*k2(2) + R0*(M(3) + h2*k2(3))))/L;
      k3(3) = (-(M(2) + h2 * k2(2) + R0 * (M(3) + h2 * k2(3)))) / L;
      // Stage 4.
      //     k4(1) = (G*(M(2) + h*k3(2) - (M(1) + h*k3(1))) - gnor(M(1) + h*k3(1),sys_variables))/C1;
      k4(1) = (G * (M(2) + h * k3(2) - (M(1) + h * k3(1))) - gnor(M(1) + h*k3(1))) / C1;
      //     k4(2) = (G*(M(1) + h*k3(1) - (M(2) + h*k3(2))) + M(3) + h*k3(3))/C2;
      k4(2) = (G * (M(1) + h * k3(1) - (M(2) + h * k3(2))) + M(3) + h * k3(3)) / C2;
      //     k4(3) = (-(M(2) + h*k3(2) + R0*(M(3) + h*k3(3))))/L;    
      k4(3) = (-(M(2) + h * k3(2) + R0 * (M(3) + h * k3(3)))) / L;    
      //     M = M + (k1 + 2*k2 + 2*k3 + k4)*(h6); %Finishes integration and assigns values to M(1),
      //                                           %M(2) and M(3)      
      M = M + (k1 + 2 * k2 + 2 * k3 + k4) * (h6);     
      //     TimeSeries(3,i+1) = M(1);  %TimeSeries 3 is V1
      //     TimeSeries(2,i+1) = M(2);  %TimeSeries 2 is V2
      //     TimeSeries(1,i+1) = M(3); %TimeSeries 1 is I3    
      V1[i] = M(1);
      V2[i] = M(2);
      I3[i] = M(3);
    }
    return OK;
  }
 };

// %%%%%%%%%%%%%%%%%%%%%%
// % SubFunction (James)%
// %%%%%%%%%%%%%%%%%%%%%%
// function TimeSeries = chua(sys_variables,integ_variables)
// % Syntax: TimeSeries=chua(sys_variables,integ_variables)
// % sys_variables = [L,R0,C2,G,Ga,Gb,E,C1];
// % integ_variables = [x0,y0,z0,dataset_size,step_size];
// % Models Initial Variables
// %-------------------------
// L  =           sys_variables(1);
// R0 =           sys_variables(2);
// C2 =           sys_variables(3);
// G  =           sys_variables(4);
// Ga =           sys_variables(5);
// Gb =           sys_variables(6);
// E  =           sys_variables(7);
// C1 =           sys_variables(8);
// x0 =           integ_variables(1);
// y0 =           integ_variables(2);
// z0 =           integ_variables(3);
// dataset_size = integ_variables(4);
// step_size =    integ_variables(5);
// TimeSeries = [x0, y0, z0]'; % models initial conditions
// % Optimized Runge-Kutta Variables
// %--------------------------------
// h = step_size*G/C2;
// h2 = (h)*(.5);
// h6 = (h)/(6);
// anor = Ga/G;
// bnor = Gb/G;
// bnorplus1 = bnor + 1;
// alpha = C2/C1;
// beta = C2/(L*G*G);
// gammaloc = (R0*C2)/(L*G);
// bh = beta*h;
// bh2 = beta*h2;
// ch = gammaloc*h;
// ch2 = gammaloc*h2;
// omch2 = 1 - ch2;
// k1 = [0 0 0]';
// k2 = [0 0 0]';
// k3 = [0 0 0]';
// k4 = [0 0 0]';
// M = [0 0 0]';
// % Calculate Time Series
// %----------------------
// M(1) = TimeSeries(3)/E;
// M(2) = TimeSeries(2)/E;
// M(3) = TimeSeries(1)/(E*G);
// for i=1:dataset_size
//     % Runge Kutta
//     % Round One
//     k1(1) = alpha*(M(2) - bnorplus1*M(1) - (.5)*(anor - bnor)*(abs(M(1) + 1) - abs(M(1) - 1)));
//     k1(2) = M(1) - M(2) + M(3);
//     k1(3) = -beta*M(2) - gammaloc*M(3);
//     % Round Two
//     temp = M(1) + h2*k1(1);
//     k2(1) = alpha*(M(2) + h2*k1(2) - bnorplus1*temp - (.5)*(anor - bnor)*(abs(temp + 1) - abs(temp - 1)));
//     k2(2) = k1(2) + h2*(k1(1) - k1(2) + k1(3));
//     k2(3) = omch2*k1(3) - bh2*k1(2);
//     % Round Three
//     temp = M(1) + h2*k2(1);
//     k3(1) = alpha*(M(2) + h2*k2(2) - bnorplus1*temp - (.5)*(anor - bnor)*(abs(temp + 1) - abs(temp - 1)));
//     k3(2) = k1(2) + h2*(k2(1) - k2(2) + k2(3));
//     k3(3) = k1(3) - bh2*k2(2) - ch2*k2(3);
//     % Round Four
//     temp = M(1) + h*k3(1);
//     k4(1) = alpha*(M(2) + h*k3(2) - bnorplus1*temp - (.5)*(anor - bnor)*(abs(temp + 1) - abs(temp - 1)));
//     k4(2) = k1(2) + h*(k3(1) - k3(2) + k3(3));
//     k4(3) = k1(3) - bh*k3(2) - ch*k3(3);
//     M = M + (k1 + 2*k2 + 2*k3 + k4)*(h6);
//     TimeSeries(3,i+1) = E*M(1);
//     TimeSeries(2,i+1) = E*M(2); 
//     TimeSeries(1,i+1) = (E*G)*M(3);
//     i=i+1;
// end

struct ChuasOscillatorPiecewise : public OpcodeBase<ChuasOscillatorPiecewise>
{
  // OUTPUTS
  MYFLT *I3;
  MYFLT *V2;
  MYFLT *V1;
  // INPUTS
  // function TimeSeries = chuacc(L,R0,C2,G,Ga,Gb,C1,E,x0,y0,z0,dataset_size,step_size)
  // Circuit elements.
  MYFLT *L_;
  MYFLT *R0_;
  MYFLT *C2_;
  MYFLT *G_;
  MYFLT *Ga_;
  MYFLT *Gb_;
  MYFLT *C1_;
  MYFLT *E_;
  // Initial values...
  MYFLT *I3_;
  MYFLT *V2_;
  MYFLT *V1_;
  MYFLT *step_size_;
  // STATE
  // Runge-Kutta step sizes.
  MYFLT h;
  MYFLT h2;
  MYFLT h6;
  // Runge-Kutta slopes.
  ublas::vector<MYFLT> k1;
  ublas::vector<MYFLT> k2;
  ublas::vector<MYFLT> k3;
  ublas::vector<MYFLT> k4;
  // Temporary value.
  ublas::vector<MYFLT> M;
  // Other variables.
  MYFLT step_size;
  MYFLT anor;
  MYFLT bnor;
  MYFLT bnorplus1;
  MYFLT alpha;
  MYFLT beta;
  MYFLT gammaloc;
  MYFLT bh;
  MYFLT bh2;
  MYFLT ch;
  MYFLT ch2;
  MYFLT omch2;
  MYFLT temp;
  size_t ksmps;
  void stepSizes()
  {
    step_size = *step_size_;
    // h = step_size*G/C2;
    h = step_size * *G_ / *C2_;
    // h2 = (h)*(.5);
    h2 = h / 2.0;
    // h6 = (h)/(6);
    h6 = h / 6.0;
    // anor = Ga/G;
    anor = *Ga_ / *G_;
    // bnor = Gb/G;
    bnor = *Gb_ / *G_;
    // bnorplus1 = bnor + 1;
    bnorplus1 = bnor + 1.0;
    // alpha = C2/C1;
    alpha = *C2_ / *C1_;
    // beta = C2/(L*G*G);
    beta = *C2_ / (*L_ * *G_ * *G_);
    // gammaloc = (R0*C2)/(L*G);
    gammaloc = (*R0_ * *C2_) / (*L_ * *G_);
    // bh = beta*h;
    bh = beta * h;
    // bh2 = beta*h2;
    bh2 = beta * h2;
    // ch = gammaloc*h;
    ch = gammaloc * h;
    // ch2 = gammaloc*h2;
    ch2 = gammaloc * h2;
    // omch2 = 1 - ch2;
    omch2 = 1.0 - ch2;
  }
  int init(CSOUND *csound)
  {
    stepSizes();
    // NOTE: The original MATLAB code uses 1-based indexing.
    //       Although the MATLAB vectors are columns, 
    //       these are rows; it doesn't matter here.
    // k1 = [0 0 0]';
    k1.resize(4);
    // k2 = [0 0 0]';
    k2.resize(4);
    // k3 = [0 0 0]';
    k3.resize(4);
    // k4 = [0 0 0]';
    k4.resize(4);
    // M = [0 0 0]';
    M.resize(4);
    M(1) = *V1_ * *E_;
    M(2) = *V2_ / *E_;
    M(3) = *I3_ / (*E_ * *G_);
    ksmps = csound->GetKsmps(csound);
    return OK;
  }
  int kontrol(CSOUND *csound)
  {
    // NOTE: MATLAB code goes into ublas C++ code pretty straightforwardly,
    //       probaby by design. This is very handy and should prevent mistakes.
    // Start with aliases for the Csound inputs, in order
    // to preserve the clarity of the original code.
    MYFLT &L = *L_;
    MYFLT &R0 = *R0_;
    MYFLT &C2 = *C2_;
    MYFLT &G = *G_;
    MYFLT &Ga = *Ga_;
    MYFLT &Gb = *Gb_;
    MYFLT &C1 = *C1_;
    MYFLT &E = *E_;
    // Recompute Runge-Kutta step sizes if necessary.
    if (step_size != *step_size_) {
      stepSizes();
    }
    // Standard 4th-order Runge-Kutta integration.
    for (size_t i = 0; i < ksmps; i++) {
      // Stage 1.
      k1(1) = alpha*(M(2) - bnorplus1*M(1) - (.5)*(anor - bnor)*(abs(M(1) + 1) - abs(M(1) - 1)));
      k1(2) = M(1) - M(2) + M(3);
      k1(3) = -beta*M(2) - gammaloc*M(3);
      // Stage 2.
      temp = M(1) + h2*k1(1);
      k2(1) = alpha*(M(2) + h2*k1(2) - bnorplus1*temp - (.5)*(anor - bnor)*(abs(temp + 1) - abs(temp - 1)));
      k2(2) = k1(2) + h2*(k1(1) - k1(2) + k1(3));
      k2(3) = omch2*k1(3) - bh2*k1(2);
      // Stage 3.
      temp = M(1) + h2*k2(1);
      k3(1) = alpha*(M(2) + h2*k2(2) - bnorplus1*temp - (.5)*(anor - bnor)*(abs(temp + 1) - abs(temp - 1)));
      k3(2) = k1(2) + h2*(k2(1) - k2(2) + k2(3));
      k3(3) = k1(3) - bh2*k2(2) - ch2*k2(3);
      // Stage 4.
      temp = M(1) + h*k3(1);
      k4(1) = alpha*(M(2) + h*k3(2) - bnorplus1*temp - (.5)*(anor - bnor)*(abs(temp + 1) - abs(temp - 1)));
      k4(2) = k1(2) + h*(k3(1) - k3(2) + k3(3));
      k4(3) = k1(3) - bh*k3(2) - ch*k3(3);
      M = M + (k1 + 2*k2 + 2*k3 + k4)*(h6);
      V1[i] = E * M(1);
      V2[i] = E * M(2); 
      I3[i] = (E * G) * M(3);
    }
    return OK;
  }
};

extern "C"
{
  OENTRY oentries[] =
    {
      {
        "chuac",
        sizeof(ChuasOscillatorCubic),
        5,
	//                                                                           %x0 = I3, y0 = V2, z0 = V1 from datafiles
	// kL,       kR0,  kC2,     kG,       kC1,       iI3, iV2,   iV1,  kstep_size
	// 0.00945,  7.5,  2e-007,  0.00105,  1.5e-008,  0,   -0.1,  0.1,  5e-6
        "aaa",
        "kkkkkiiik",
        (SUBR) ChuasOscillatorCubic::init_,
        0,
        (SUBR) ChuasOscillatorCubic::kontrol_,
      },
      {
        "chuap",
        sizeof(ChuasOscillatorPiecewise),
        5,
	//                                                                           %x0 = I3, y0 = V2, z0 = V1 from datafiles
	// kL,       kR0,  kC2,     kG,       kGa, kGb,       kC1,       kE,         iI3, iV2,   iV1,  kstep_size
	// 0.00945,  7.5,  2e-007,  0.00105,  0,   -0.00121,  1.5e-008,  1.76e-005,  0,   -0.1,  0.1,  5e-6
        "aaa",
        "kkkkkkkkiiik",
        (SUBR) ChuasOscillatorPiecewise::init_,
        0,
        (SUBR) ChuasOscillatorPiecewise::kontrol_,
      },
      {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
      }
    };

  PUBLIC int csoundModuleCreate(CSOUND *csound)
  {
    return 0;
  }

  PUBLIC int csoundModuleInit(CSOUND *csound)
  {
    int status = 0;
    for(OENTRY *oentry = &oentries[0]; oentry->opname; oentry++)
      {
        status |= csound->AppendOpcode(csound, oentry->opname,
                                       oentry->dsblksiz, oentry->thread,
                                       oentry->outypes, oentry->intypes,
                                       (int (*)(CSOUND*,void*)) oentry->iopadr,
                                       (int (*)(CSOUND*,void*)) oentry->kopadr,
                                       (int (*)(CSOUND*,void*)) oentry->aopadr);
      }
    return status;
  }

  PUBLIC int csoundModuleDestroy(CSOUND *csound)
  {
    return 0;
  }
}
