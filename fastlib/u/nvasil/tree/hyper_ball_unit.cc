/*
 * =====================================================================================
 *
 *       Filename:  hyper_ball_unit.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/21/2007 06:12:12 PM EDT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Nikolaos Vasiloglou (NV), nvasil@ieee.org
 *        Company:  Georgia Tech Fastlab-ESP Lab
 *
 * =====================================================================================
 */

#include "loki/Typelist.h"
#include "fastlib/fastlib.h"
#include "mmanager/memory_manager.h"
#include "hyper_ball.h"

template<typename TYPELIST, bool diagnostic>
class HyperBallTest {
 public:
  typedef TypeAt<TYPELIST, 0>::value Precision_t;
	typedef TypeAt<TYPELIST, 1>::value Allocator_t;
	typedef TypeAt<TYPELIST, 2>::value Metric_t;
	typedef HyperBall<TYPELIST, diagnostic> HyperBall_t;
  HyperBallTest();
	~HyperBall() {
	  Destruct();
	}
	void Init() {
		dimension_=2;
		Allocator_t::allocator_ = new Allocator_t();
		Allocator_t::allocator_->Initialize();
	  hyper_ball_= new HyperBall_t();
		Allocator_t::ArrayPtr<Precision_t> center(dimension_);
		center[0]=1;
		center[1]=-1;
    Precision_t radious=2;		
    Allocator_t::ArrayPtr<Precision_t> pivot_left;
		Allocator_t::ArrayPtr<Precision_t> pivot_right;
   	hyper_ball_->Init(center, radious, pivot_left, pivot_right);
	}
	void Destruct() {
	  delete hyper_ball_;
		delete Allocator_t::allocator_;
	}
	
  void AliasTest() {
		printf("Alias Test\n");
	  Init();
    HyperBall_t other;
    other.Alias(*hyper_ball_);		
	  DEBUG_ASSERT_MSG(other.center_==hyper_ball_->center_, 
				             "Centers don't match\n");
		DEBUG_ASSERT_MSG(other.radious_==hyper_ball_->radious_,
				             "Radious don't match\n");
		DEBUG_ASSERT_MSG(other.pivot_left_==hyper_ball_->pivot_left_,
				             "Pivot left doesn't match\n");
		DEBUG_ASSERT_MSG(other.pivot_right_==hyper_ball_->pivot_right_,
				             "Pivot right don't match\n");
	  Destruct();
	}
  void CopyTest() {
		printf("Copy Test\n");
	  Init();
    HyperBall_t other;
		other.Init(dimension_);
    other.Copy(*hyper_ball_, dimension_);	
    DEBUG_ASSERT_MSG(other.radious_==hyper_ball_->radious_,
				             "Radious don't match\n");
    DEBUG_ASSERT_MSG(other.center_!=hyper_ball_->center_, 
				             "Centers are the same \n");
		for(index_t i=0; i<dimension_; i++) {
	    DEBUG_ASSERT_MSG(other.center_[i]==hyper_ball_->center_[i], 
				             "Centers don't match\n");
			DEBUG_ASSERT_MSG(other.pivot_left_[i]==hyper_ball_->pivot_left_[i],
				             "Pivot left doesn't match\n");
		  DEBUG_ASSERT_MSG(other.pivot_right_[i]==hyper_ball_->pivot_right_[i],
				             "Pivot right don't match\n");
		}
	  Destruct();
	}
	void IsWithinTest() {
	  Point<Precision_t, Allocator_t> point;
	  point[0]=1;
		point[1]=0.3;
		Precision_t range=0.03;
		DEBUG_ASSERT_MSG(hyper_ball_->IsWithin(point, dimension_, 
					                                 range, comp)==true, 
				             "IsWithin doesn't work\n");
		range=2;
    DEBUG_ASSERT_MSG(hyper_ball_->IsWithin(point, dimension_, 
					                                 range, comp)==false, 
				             "IsWithin doesn't work\n");

	  Destruct(); 
	}
	void CrossesBoundaryTest() {
	  Point<Precision_t, Allocator_t> point;
		point.Init(dimension_);
	  point[0]=2;
		point[1]=-4;
		Precision_t range=1;
		DEBUG_ASSERT_MSG(hyper_ball_->CrossesBoundary(point, dimension_, 
					                                 range, comp)==true, 
				             "CrossesBoundary doesn't work\n");
		range=0.25;
    DEBUG_ASSERT_MSG(hyper_ball_->CrossesBoundary(point, dimension_, 
					                                 range, comp)==false, 
				             "CrossesBoundary doesn't work\n");

    Destruct();
	}
	void DistanceTest(){
    Point<Precision_t, Allocator_t> point1;
	  Point<Precision_t, Allocator_t> point2;
	  point1.Init(dimension_);
		point1[0]=0;
		point1[1]=1;
		point2[0]=-1;
		point2[1]=-2;
		point2.Init(dimension_);
		ASSERT_DEBUG_MSG(HyperBall_t::Distance(point1, point2, dimension_)==10,
				             "Distance between points doesn't work\n");
		HyperBall_t other;
		other.Init(dimension_);
		other.center_[0]=1;
		other.center_[1]=5;
		other.radious_=3;
		ASSERT_DEBUG_MSG(HyperBall_t::Distance(*hyper_ball_, other,
				                                   dimension)==31,
				                                   "Dimension doesn't work\n");
		other.radious_=34;
		ASSERT_DEBUG_MSG(HyperBall_t::Distance(*hyper_ball_, other,
				                                   dimension)==0,
				                                   "Dimension doesn't work\n");
		Destruct();             
	}
	
 private: 
  HyperBall<TYPELIST, diagnostic> *hyper_ball_;
  int32 dimension_; 
  	
};


int main(int argc, char *argv[]) {
  typdef TYPELIST_3(float32, MemoryManager<true>, EuclideanMetric<float32>)
		 UserTypeParameters_t;
	HyperBallTest<UserTypeParameters_t, false> hyper_ball_test;
	hyper_ball_test.AliasTest();
	hyper_ball_test.CopyTest();
	hyper_ball_test.IsWithinTest();
	hyper_ball_test.CrossesBoundaryTest();
	hyper_ball_test.DistanceTest();
	hyper_ball_test.ClosestDistanceTest();
}
