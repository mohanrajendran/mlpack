/*
 * =====================================================================================
 *
 *       Filename:  node_unit.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/23/2007 10:19:28 AM EDT
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
#include "dataset/binary_dataset.h"
#include "hyper_rectangle.h"

template<typename TYPELIST, bool diagnostic>
class NodeTest {
  public:
  typedef Loki::TL::TypeAt<TYPELIST, 0>::value Precision_t;
	typedef Loki::TL::TypeAt<TYPELIST, 1>::value Allocator_t;
	typedef Loki::TL::TypeAt<TYPELIST, 2>::value Metric_t;
	typedef HyperRectangle<TYPELIST, diagnostic> HyperRectangle_t;
	typedef Loki::TL::Append<TYPELIST, 
	    TYPELIST_2(HyperRectangle_t, NullStatistics)> UserTypeParameters_t;

	typedef Node<UserTypeParameters_t, diagnostic> Node_t;
	typedef Allocator_t:: template ArrayPtr<Precision_t> Array_t;
	typedef Point<Precision_t, Loki::NullType> Point_t;
  NodeTest() {
	}
	~NodeTest() {
	  Destruct();
	}
	void Init() {
		dimension_=2;
		num_of_points_=30;
		Allocator_t::allocator_ = new Allocator_t();
		Allocator_t::allocator_->Initialize();
	  Array_t min(dimension);
		min[0]=-1;
		min[1]=-1;
	  Array_t max(dimension);
		max[0]=1;
		max[1]=1;
		data_file_="data"
    dataset_.Init(data_file_, num_of_points, dimension);
    for(index_t i=0; i<num_of_points_; i++) {
		  dataset_.At(i)[0]=rand()/14.333;
			dataset_.At(i)[1]=rand()/1.6778;
			dataset_.set_id(i,i);
			}	
		hyper_rectangle_->Init(min, max, 0, 0);
	  node_->Init(hyper_rectangle_,
			          NullStatistics(),
							  &dataset_,
			          0,
			          num_of_points_, 
			          dimension_); 
  }
	void Destruct() {
		hyper_rectangle_->Destruct();
		delete Allocator_t::allocator_;
		dataset_.Destruct();
		unlink(data_file_.c_str());
		unlink(data_file_.append(".ind").c_str());
	}

	void FindNearest() { 
	  PointIdentityDiscriminator discriminator;
    vector<pair<Precision_t, Point<Precision_t, Allocator_t> > nearest;
    ComputationsCounter<diagnostic> &comp;
		for(index_t i=0; i<num_of_points_; i++) {
			Point_t query_point;
			Precision_t node_distance;
      		  query_point.Alias(dataset_.At(i), dataset_.get_id(i));
			node_->FindNearest(query_point, 
			                   nearest, 
                         node_distance, 
							  				 1, 
												 dimension_,
                         discriminator,
									       comp);
			Precision_t min_distance=numerical_limits<Precision_t>::max();
			index_t min_id;
      for(index_t j=0; j<num_of_points_; j++) {
			  if (unlikely(dataset_.get_id(j)==dataset_.get_id(i))) {
				  continue;
				}
				Precision_t distance = HyperRectangle_t::Distance(dataset_.At(i),
						                                              dataset_.At(j),
																													dimension_);
				if (distance<min_dist) {
				  min_id=j;
					min_dist=distance;
				}
			}
			DEBUG_ASSERT_MSG(distance==nearest[0].first, 
					"Something wrong in the distance\n");
			DEBUG_ASSERT_MSG(min_id==nearest[0].second.get_id(i), 
					             "Something wrong in the distance\n");
	  }	
	}
  void FindAllNearest() {
    Node_t::NNResult result[num_of_points_];
	  node_->set_kneighbors(result, num_of_points_);	  
		Precision_t max_neighbor_distance=numeric_limits<Precision_t>::max();
    PointIdentityDiscriminator &discriminator;
    ComputationsCounter<diagnostic> comp;
	  node_->FindAllNearest(node,
                          max_neighbor_distance,
                          1,
                          dimension_,
                          discriminator,
                          comp);
		for(index_t i=0; i<num_of_poins_; i++) {
      for(index_t j=0; j<num_of_points_; j++) {
		    if (unlikely(dataset_.get_id(j)==dataset_.get_id(i))) {
			    continue;
	 	    }
		    Precision_t distance = HyperRectangle_t::Distance(dataset_.At(i),
		 	 	                                                  dataset_.At(j),
					  																						  dimension_);
		    if (distance<min_dist) {
		      min_id=j;
			    min_dist=distance;
		    }
	    }
			DEBUG_ASSERT_MSG(distance==result[i].nearest_.distance_, 
					"Something wrong in the distance\n");
			DEBUG_ASSERT_MSG(min_id==result[i].nearest_.get_id(), 
					             "Something wrong in the distance\n");
		}
	}
	
  void TestAll(){
   Init();
   FindNearest();
   Destruct();
   Init();
   FindAllNearest();
   Destruct;	 
	}
	
 private: 
	Allocator_t:: template Ptr<Node_t> node_;
	string data_file_;
	Allocator_t:: template<HyperRectangle_t> hyper_rectangle_;
  BinaryDataset<Precision_t> dataset_;
	index_t num_of_points_;
  int32 dimension_; 
  	
};


int main(int argc, char *argv[]) {
  typdef TYPELIST_3(float32, MemoryManager<true>, 
			              EuclideanMetric<float32>)	 UserTypeParameters1_t;
		
	NodeTest<UserTypeParameters1_t, false> node_test;
	node_test.TestAll();
}
