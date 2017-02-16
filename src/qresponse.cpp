/** @file qresponse.cpp
  @brief Implements the QResponse class, for the soma response of neural populations.

  Each neural population is associated with a QResponse object which produces
  the soma response governed by a specified equation, for example Sigmoid:
  \f[
    Insert equation 9 from draft neurofield paper here.
  \f]

  @author Peter Drysdale, Felix Fung,
*/

// C++ standard library headers
#include <vector>   // std::vector;
using std::vector;
#include <string>   // std::string;
using std::string;

// Neurofield headers
#include "array.h"      // Array;
#include "configf.h"    // Configf;
#include "output.h"     // Output;
#include "fmath.h"      // fmath::expd;
#include "coupling.h"   // Coupling;
#include "dendrite.h"   // Dendrite;
#include "propagator.h" // Propagator;
#include "dendriteramp.h"      // DendriteRamp;
#include "dendrite_integral.h" // DendriteIntegral;
#include "qresponse.h"  // QResponse;

void QResponse::init( Configf& configf ) {
  vector<string> temp = configf.arb("-");
  mode = temp[0];
  if( mode == "Sigmoid" ) {
    configf.param("Theta",theta);
    configf.param("Sigma",sigma);
    configf.param("Qmax",Q_max);
  } else if( mode == "Linear") { // Equation is q = av + b;
    configf.param("a",a);
    configf.param("b",b);
  } else if( mode == "Quadratic") { // Equation is q = av^2 + b^v + c;
    configf.param("a",a);
    configf.param("b",b);
    configf.param("c",c);
  } else if( mode == "Cubic") { // Equation is q = av^2 + b^v + c;
    configf.param("a",a);
    configf.param("b",b);
    configf.param("c",c);
    configf.param("d",d);
  }

  for( Array<Dendrite>::size_type i=0; i<dendrites.size(); i++ ) {
    configf>>*dendrites[i];
  }

  glu_m.init(configf);
}

void QResponse::Glu::init( Configf& configf ) {
  Lambda = 0;
  tGlu = 0;
  configf.optional("Lambda",Lambda);
  configf.optional("tGlu",tGlu);
  variables[0].resize(nodes,1e-6);
  variables[1].resize(nodes);
}

void QResponse::Glu::rhs( const vector<double>& y, vector<double>& dydt ) {
  // y = { glu, excitatory phi }
  // glu
  dydt[0] = Lambda*y[1] -y[0]/tGlu;
  if( y[0]+dydt[0]*deltat <0 ) {
    dydt[0] = -y[0];
  }
  // excitatory phi, leave alone
  dydt[1] = 0;
}

QResponse::QResponse( size_type nodes, double deltat, size_type index )
  : NF(nodes,deltat,index), v(nodes), glu_m(nodes,deltat), glu_rk4(glu_m) {
}

QResponse::~QResponse() = default;

void QResponse::step() {
  // step through dendrites, then sum up soma potential
  dendrites.step();
  for( size_type i=0; i<nodes; i++ ) {
    v[i] = 0;
  }
  for( Array<Dendrite>::size_type  i=0; i<dendrites.size(); i++ ) {
    for( size_type j=0; j<nodes; j++ ) {
      v[j] += dendrites[i]->V()[j];
    }
  }

  // glutamate dynamics
  if( glu_m.Lambda != 0 ) {
    for( size_type j=0; j<nodes; j++ ) {
      glu_m[1][j] = 0; // reset excitatory phi
    }
    for( Array<Dendrite>::size_type  i=0; i<dendrites.size(); i++ ) {
      if( dendrites[i]->precouple.excite() ) {
        for( size_type j=0; j<nodes; j++ ) {
          glu_m[1][j] += dendrites[i]->prepropag[j]; // put in excitatory phi
        }
      }
    }
    glu_rk4.step();
  }
}

void QResponse::add2Dendrite( size_type index, const Propagator& prepropag,
                              const Coupling& precouple, Configf& configf ) {
  string temp(configf.find( label("Dendrite ",index+1)+":" ));
  dendrite_index.push_back(index);
  // PUT YOUR DENDRITE HERE
  if( temp == "Ramp" ) {
    dendrites.add( new DendriteRamp(nodes,deltat,index,prepropag,precouple) );
  } else if ( temp == "Integral" ) {
    dendrites.add( new DendriteIntegral(nodes,deltat,index,prepropag,precouple) );
    // END PUT YOUR DENDRITE HERE
  } else {
    dendrites.add( new Dendrite(nodes,deltat,index,prepropag,precouple) );
  }
}

const vector<double>& QResponse::glu() const {
  return glu_m[0];
}

void QResponse::fire( vector<double>& Q ) const {
  if(mode == "Sigmoid") {
    for( size_type i=0; i<nodes; i++ ) {
      Q[i] = Q_max/( 1.0F+ fmath::expd( -(v[i]-theta)/sigma ) );
    }
  } else if (mode == "Linear") {
    for( size_type i=0; i<nodes; i++ ) {
      Q[i] = v[i]*a +b;
    }
  } else if (mode == "Quadratic") {
    for( size_type i=0; i<nodes; i++ ) {
      Q[i] = v[i]*v[i]*a + v[i]*b +c;
    }
  } else if (mode == "Cubic") {
    for( size_type i=0; i<nodes; i++ ) {
      Q[i] = v[i]*v[i]*v[i]*a + v[i]*v[i]*b + v[i]*c +d;
    }
  }
}

void QResponse::output( Output& output ) const {
  output("Pop",index+1,"V",v);
}

void QResponse::outputDendrite( size_type index, Output& output ) const {
  for( Array<Dendrite>::size_type  i=0; i<dendrites.size(); i++ ) {
    if( dendrite_index[i] == index ) {
      dendrites[i]->output(output);
    }
  }
}
