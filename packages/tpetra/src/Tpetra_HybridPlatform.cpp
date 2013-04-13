// @HEADER
// ***********************************************************************
// 
//          Tpetra: Templated Linear Algebra Services Package
//                 Copyright (2008) Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ************************************************************************
// @HEADER

#include <Tpetra_HybridPlatform.hpp>
#include <cstdio> // for std::sscanf

// This macro is only for use by Tpetra developers.
// It should only be invoked in the Tpetra namespace.
#define TPETRA_HYBRIDPLATFORM_ADD_NODE_SUPPORT_DEF(N) \
  template <> bool HybridPlatform::isNodeSupported<N>() {return true;}

namespace Tpetra {
  TPETRA_HYBRIDPLATFORM_ADD_NODE_SUPPORT_DEF(Kokkos::SerialNode)
#ifdef HAVE_KOKKOSCLASSIC_TBB
  TPETRA_HYBRIDPLATFORM_ADD_NODE_SUPPORT_DEF(Kokkos::TBBNode)
#endif        
#ifdef HAVE_KOKKOSCLASSIC_OPENMP
  TPETRA_HYBRIDPLATFORM_ADD_NODE_SUPPORT_DEF(Kokkos::OpenMPNode)
#endif        
#ifdef HAVE_KOKKOSCLASSIC_THREADPOOL
  TPETRA_HYBRIDPLATFORM_ADD_NODE_SUPPORT_DEF(Kokkos::TPINode)
#endif        
#ifdef HAVE_KOKKOSCLASSIC_THRUST
  TPETRA_HYBRIDPLATFORM_ADD_NODE_SUPPORT_DEF(Kokkos::ThrustGPUNode)
#endif

  HybridPlatform::
  HybridPlatform (const Teuchos::RCP<const Teuchos::Comm<int> >& comm, 
		  Teuchos::ParameterList& pl)
    : comm_ (comm)
    , nodeCreated_ (false)
    , nodeType_ (SERIALNODE)
  {
    // ParameterList format:
    // 
    // Node designation sublists have a name beginning with one of the
    // following characters: % = [ and satisfying the following
    // format:

    //   %M=N    is satisfied if mod(myrank,M) == N
    //   =N      is satisfied if myrank == N
    //   [M,N]   is satisfied if myrank \in [M,N]
    // 
    // A node designation sublist must have a parameter entry of type
    // std::string named "NodeType". The value indicates the type of
    // the Node.  The activated node designation sublist will be
    // passed to the Node constructor.
    // 
    // For example:
    // "%2=0"  ->  
    //    NodeType     = "Kokkos::ThrustGPUNode"
    //    DeviceNumber = 0
    //    Verbose      = 1
    // "%2=1"  ->
    //    NodeType     = "Kokkos::TPINode"
    //    NumThreads   = 8
    // 
    // In this scenario, nodes that are equivalent to zero module 2,
    // that is, even nodes, will be selected to use ThrustGPUNode
    // objects and initialized with the parameter list containing
    //    NodeType   = "Kokkos::ThrustGPUNode"
    //    DeviceNumber = 0
    //    Verbose      = 1
    //
    // Nodes that are equivalent to one modulo 2, i.e., odd nodes,
    // will be selected to use TPINode objects and initialized with
    // the parameter list containing
    //    NodeType   = "Kokkos::TPINode"
    //    NumThreads = 8
    // 
    // If multiple node designation sublists match the process rank,
    // then the first encountered node designation will be used.  I
    // don't know if ParameterList respects any ordering, therefore,
    // multiple matching designations are to be avoided.

    const int myrank = comm_->getRank();
    std::string desigNode("");
    bool matchFound = false;
    for (Teuchos::ParameterList::ConstIterator it = pl.begin(); it != pl.end(); ++it) {
      if (it->second.isList()) {
        int parsedLen, M, N;
        const std::string &name = it->first;
        const Teuchos::ParameterList &sublist = Teuchos::getValue<Teuchos::ParameterList>(it->second);
        // select and assign instList_;
        parsedLen = 0;
        if (std::sscanf(name.c_str(),"%%%d=%d%n",&M,&N,&parsedLen) == 2 && (size_t)parsedLen == name.length()) {
          if ((myrank % M) == N) {
            matchFound = true;
          }
        }
        parsedLen = 0;
        if (std::sscanf(name.c_str(),"=%d%n",&N,&parsedLen) == 1 && (size_t)parsedLen == name.length()) {
          if (myrank == N) {
            matchFound = true;
          }
        }
        parsedLen = 0;
        if (std::sscanf(name.c_str(),"[%d,%d]%n",&M,&N,&parsedLen) == 2 && (size_t)parsedLen == name.length()) {
          if (M <= myrank && myrank <= N) {
            matchFound = true;
          }
        }
        if (name == "default") {
          matchFound = true;
        }
        if (matchFound) {
          try {
            desigNode = sublist.get<std::string>("NodeType");
          }
          catch (Teuchos::Exceptions::InvalidParameterName &e) {
            TEUCHOS_TEST_FOR_EXCEPTION_PURE_MSG(true, std::runtime_error, 
              std::endl << Teuchos::typeName(*this) << ": Invalid machine file." << std::endl 
              << "Missing parameter \"NodeType\" on Node " << myrank << " for Node designator " << "\"" << name << "\":" << std::endl 
              << sublist << std::endl);
          }
          if (desigNode == "Kokkos::SerialNode") {
            nodeType_ = SERIALNODE;
          }
#ifdef HAVE_KOKKOSCLASSIC_THREADPOOL
          else if (desigNode == "Kokkos::TPINode") {
            nodeType_ = TPINODE;
          }
#endif
#ifdef HAVE_KOKKOSCLASSIC_TBB
          else if (desigNode == "Kokkos::TBBNode") {
            nodeType_ = TBBNODE;
          }
#endif
#ifdef HAVE_KOKKOSCLASSIC_OPENMP
          else if (desigNode == "Kokkos::OpenMPNode") {
            nodeType_ = OMPNODE;
          }
#endif
#ifdef HAVE_KOKKOSCLASSIC_THRUST
          else if (desigNode == "Kokkos::ThrustGPUNode") {
            nodeType_ = THRUSTGPUNODE;
          }
#endif
          else {
            matchFound = false;
          }
          if (matchFound) {
            instList_ = sublist;
            break;
          }
        }
      }
    }
    if (!matchFound) {
      TEUCHOS_TEST_FOR_EXCEPTION_PURE_MSG(true, std::runtime_error, 
          Teuchos::typeName(*this) << ": No matching node type on rank " << myrank);
    }
  } 

  HybridPlatform::~HybridPlatform() 
  {}

  RCP<ParameterList> HybridPlatform::listSupportedNodes()
  {
    RCP<ParameterList> list = Teuchos::parameterList();
    {
      ParameterList subpl;
      subpl.set("NodeType","Kokkos::SerialNode");
      subpl.setParameters( Kokkos::SerialNode::getDefaultParameters() );
      list->set("=-1",subpl);
    }
#ifdef HAVE_KOKKOSCLASSIC_TBB
    {
      ParameterList subpl;
      subpl.set("NodeType","Kokkos::TBBNode");
      subpl.setParameters( Kokkos::TBBNode::getDefaultParameters() );
      list->set("=-2",subpl);
    }
#endif        
#ifdef HAVE_KOKKOSCLASSIC_OPENMP
    {
      ParameterList subpl;
      subpl.set("NodeType","Kokkos::OpenMPNode");
      subpl.setParameters( Kokkos::OpenMPNode::getDefaultParameters() );
      list->set("=-3",subpl);
    }
#endif        
#ifdef HAVE_KOKKOSCLASSIC_THREADPOOL
    {
      ParameterList subpl;
      subpl.set("NodeType","Kokkos::TPINode");
      subpl.setParameters( Kokkos::TPINode::getDefaultParameters() );
      list->set("=-4",subpl);
    }
#endif        
#ifdef HAVE_KOKKOSCLASSIC_THRUST
    {
      ParameterList subpl;
      subpl.set("NodeType","Kokkos::ThrustGPUNode");
      subpl.setParameters( Kokkos::ThrustGPUNode::getDefaultParameters() );
      list->set("=-5",subpl);
    }
#endif        
    return list;
  }

  Teuchos::RCP<const Teuchos::Comm<int> > 
  HybridPlatform::getComm () const {
    return comm_;
  }

  void HybridPlatform::createNode () {
    using Teuchos::rcp;

    if (nodeCreated_) {
      return;
    }
    switch (nodeType_) {
    case SERIALNODE:
      serialNode_ = rcp (new Kokkos::SerialNode (instList_));
      break;
#ifdef HAVE_KOKKOSCLASSIC_TBB
    case TBBNODE:
      tbbNode_ = rcp (new Kokkos::TBBNode (instList_));
      break;
#endif        
#ifdef HAVE_KOKKOSCLASSIC_OPENMP
    case OMPNODE:
      ompNode_ = rcp (new Kokkos::OpenMPNode (instList_));
      break;
#endif        
#ifdef HAVE_KOKKOSCLASSIC_THREADPOOL
    case TPINODE:
      tpiNode_  = rcp (new Kokkos::TPINode (instList_));
      break;
#endif        
#ifdef HAVE_KOKKOSCLASSIC_THRUST
    case THRUSTGPUNODE:
      thrustNode_ = rcp (new Kokkos::ThrustGPUNode (instList_));
      break;
#endif        
    default:
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error, 
        Teuchos::typeName(*this) << "::runUserCode(): Invalid Node type." 
	<< std::endl);
    } // end of switch
    nodeCreated_ = true;
  }
} // namespace Tpetra

