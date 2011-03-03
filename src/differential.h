#ifndef DIFFERENTIAL_H
#define DIFFERENTIAL_H
/*
 *  differential.h
 *  cufflinks
 *
 *  Created by Cole Trapnell on 3/15/10.
 *  Copyright 2009 Cole Trapnell. All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <set>
#include <map>
#include <utility>
#include <vector>
#include <string>

#include <boost/thread.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>

#include "abundances.h"
#include "jensen_shannon.h"

using namespace std;

enum TestStatus {
	NOTEST,  // successful calculation, test not performed
	OK,      // successful numerical calc, test performed
	FAIL     // numerical exception, test not performed
}; 

// Stores the differential expression of an isoform or set of isoforms in two
// different samples, along with a significance test statistic for the difference.
struct SampleDifference
{
	SampleDifference() :
	sample_1(-1), 
	sample_2(-1), 
	value_1(0.0),
	value_2(0.0),
	test_stat(0.0),
    p_value(1.0),
	corrected_p(1.0),
	tested_group_id(-1),
	test_status(NOTEST),
	significant(false){}
	
	size_t sample_1;
	size_t sample_2;
	
	double value_1;
	double value_2;
	double differential;
	double test_stat;
	double p_value;
	double corrected_p;
	
	size_t tested_group_id; // which scaffolds' FPKMs contribute
	
	string locus_desc;
	set<string> gene_names;
	set<string> protein_ids;
	string description; // isoforms or tss groups (e.g.) involved in this test
	
	TestStatus test_status;
	bool significant;
};

typedef map<string, SampleDifference > SampleDiffs;

struct Outfiles
{
	FILE* isoform_de_outfile;
	FILE* group_de_outfile;
	FILE* gene_de_outfile;
	FILE* cds_de_outfile;
	
	FILE* diff_splicing_outfile;
	FILE* diff_promoter_outfile;
	FILE* diff_cds_outfile;
	
	FILE* isoform_fpkm_tracking_out;
	FILE* tss_group_fpkm_tracking_out;
	FILE* gene_fpkm_tracking_out;
	FILE* cds_fpkm_tracking_out;
};

struct Tests
{
	vector<vector<SampleDiffs> > isoform_de_tests;
	vector<vector<SampleDiffs> > tss_group_de_tests;
	vector<vector<SampleDiffs> > gene_de_tests;
	vector<vector<SampleDiffs> > cds_de_tests;
	
	vector<vector<SampleDiffs> > diff_splicing_tests; // to be performed on the isoforms of a single tss group
	vector<vector<SampleDiffs> > diff_promoter_tests; // to be performed on the tss groups of a single gene
	vector<vector<SampleDiffs> > diff_cds_tests; // to be performed on the cds groups of a single gene
};

struct FPKMContext
{
	FPKMContext(double c, double r, double v, AbundanceStatus s)
		: counts(c), FPKM(r), FPKM_variance(v), status(s) {}
	double counts;
	double FPKM;
	double FPKM_variance;
    AbundanceStatus status;
};

struct FPKMTracking
{
	string locus_tag;
	char classcode;
	set<string> tss_ids; // for individual isoforms only
	set<string> gene_names;
	set<string> protein_ids;
	string description; // isoforms or tss groups (e.g.) involved in this test
	string ref_match;
	
	TestStatus test_status;
	
	vector<FPKMContext> fpkm_series;
};

typedef map<string,  FPKMTracking> FPKMTrackingTable;

struct Tracking
{
	FPKMTrackingTable isoform_fpkm_tracking;
	FPKMTrackingTable tss_group_fpkm_tracking;
	FPKMTrackingTable gene_fpkm_tracking;
	FPKMTrackingTable cds_fpkm_tracking;
};

// This factory merges bundles in a requested locus from several replicates
class ReplicatedBundleFactory
{
public:
	ReplicatedBundleFactory(const vector<shared_ptr<BundleFactory> >& factories)
    : _factories(factories) {}
	
	int num_bundles() { return _factories[0]->num_bundles(); }
	vector<shared_ptr<BundleFactory> > factories() { return _factories; }
	
	bool next_bundle(HitBundle& bundle_out)
    {
        vector<HitBundle*> bundles;
        
        bool non_empty_bundle = false;
        foreach (shared_ptr<BundleFactory> fac, _factories)
        {
            bundles.push_back(new HitBundle());
            if (fac->next_bundle(*(bundles.back())))
            {
                non_empty_bundle = true;
            }
        }
        
        if (non_empty_bundle == false)
        {
            foreach (HitBundle* in_bundle, bundles)
            {
                in_bundle->ref_scaffolds().clear();
                in_bundle->clear_hits();
                delete in_bundle;
            }
            return false;
        }
        
        for (size_t i = 1; i < bundles.size(); ++i)
        {
            const vector<shared_ptr<Scaffold> >& s1 = bundles[i]->ref_scaffolds();
            const vector<shared_ptr<Scaffold> >& s2 =  bundles[i-1]->ref_scaffolds();
            assert (s1.size() == s2.size());
            for (size_t j = 0; j < s1.size(); ++j)
            {
                assert (s1[j]->annotated_trans_id() == s2[j]->annotated_trans_id());
            }
        }
        
        // Merge the replicates into a combined bundle of hits.
        HitBundle::combine(bundles, bundle_out);
        
        foreach (HitBundle* in_bundle, bundles)
        {
            in_bundle->ref_scaffolds().clear();
            in_bundle->clear_hits();
            delete in_bundle;
        }
        return true;
    }
	
	void reset() 
    {
        foreach (shared_ptr<BundleFactory> fac, _factories)
        {
            fac->reset();
        }
    }
    
    
    /*
     Model variance with a loess-smoothed function:
     Public Function LOESS(X As Variant, Y As Variant, xDomain As Variant, nPts As Long) As Double()
     Dim i As Long
     Dim iMin As Long
     Dim iMax As Long
     Dim iPoint As Long
     Dim iMx As Long
     Dim mx As Variant
     Dim maxDist As Double
     Dim SumWts As Double, SumWtX As Double, SumWtX2 As Double, SumWtY As Double, SumWtXY As Double
     Dim Denom As Double, WLRSlope As Double, WLRIntercept As Double
     Dim xNow As Double
     Dim distance() As Double
     Dim weight() As Double
     Dim yLoess() As Double
     
     If TypeName(X) = "Range" Then
        X = X.Value
     End If
     
     If TypeName(Y) = "Range" Then
        Y = Y.Value
     End If
     
     If TypeName(xDomain) = "Range" Then
        xDomain = xDomain.Value
     End If
     
     ReDim yLoess(LBound(xDomain, 1) To UBound(xDomain, 1), 1 To 1)
     
     For iPoint = LBound(xDomain, 1) To UBound(xDomain, 1)
     
         iMin = LBound(X, 1)
         iMax = UBound(X, 1)
         
         xNow = xDomain(iPoint, 1)
         
         ReDim distance(iMin To iMax)
         ReDim weight(iMin To iMax)
         
         For i = iMin To iMax
            ' populate x, y, distance
            distance(i) = Abs(X(i, 1) - xNow)
         Next
         
         Do
             ' find the nPts points closest to xNow
             If iMax + 1 - iMin <= nPts Then Exit Do
             If distance(iMin) > distance(iMax) Then
                ' remove first point
                iMin = iMin + 1
             ElseIf distance(iMin) < distance(iMax) Then
                ' remove last point
                iMax = iMax - 1
             Else
                ' remove both points?
                iMin = iMin + 1
                iMax = iMax - 1
             End If
         Loop
         
         ' Find max distance
         maxDist = -1
         For i = iMin To iMax
            If distance(i) > maxDist Then maxDist = distance(i)
         Next
         
         ' calculate weights using scaled distances
         For i = iMin To iMax
            weight(i) = (1 - (distance(i) / maxDist) ^ 3) ^ 3
         Next
         
         ' do the sums of squares
         SumWts = 0
         SumWtX = 0
         SumWtX2 = 0
         SumWtY = 0
         SumWtXY = 0
         For i = iMin To iMax
             SumWts = SumWts + weight(i)
             SumWtX = SumWtX + X(i, 1) * weight(i)
             SumWtX2 = SumWtX2 + (X(i, 1) ^ 2) * weight(i)
             SumWtY = SumWtY + Y(i, 1) * weight(i)
             SumWtXY = SumWtXY + X(i, 1) * Y(i, 1) * weight(i)
         Next
         Denom = SumWts * SumWtX2 - SumWtX ^ 2
         
         ' calculate the regression coefficients, and finally the loess value
         WLRSlope = (SumWts * SumWtXY - SumWtX * SumWtY) / Denom
         WLRIntercept = (SumWtX2 * SumWtY - SumWtX * SumWtXY) / Denom
         yLoess(iPoint, 1) = WLRSlope * xNow + WLRIntercept
     
     Next
     
     LOESS = yLoess
     
     
     Read more: LOESS Smoothing in Excel | Peltier Tech Blog | Excel Charts http://peltiertech.com/WordPress/loess-smoothing-in-excel/#ixzz1FTum6dIE

     
     */
    
    void inspect_replicate_maps(int& min_len, int& max_len)
    {
        vector<pair<string, vector<double> > > sample_count_table;
        vector<double> sample_masses;
        
        foreach (shared_ptr<BundleFactory> fac, _factories)
        {
            BadIntronTable bad_introns;
            
            vector<pair<string, double> > count_table;
            inspect_map(*fac, NULL, count_table, false);
            
            shared_ptr<ReadGroupProperties> rg_props = fac->read_group_properties();
            
            for (size_t i = 0; i < count_table.size(); ++i)
            {
                pair<string, double>& c = count_table[i];
                double raw_count = c.second;

                
                if (i >= sample_count_table.size())
                {
                    sample_count_table.push_back(make_pair(c.first, vector<double>()));
                    sample_count_table.back().second.push_back(raw_count);
                }
                else
                {
                    const string& label = sample_count_table[i].first;
                    assert (label == c.first);
                    sample_count_table[i].second.push_back(raw_count);
                }
            }
            sample_masses.push_back(rg_props->total_map_mass());
			min_len = min(min_len, rg_props->frag_len_dist()->min());
			max_len = max(max_len, rg_props->frag_len_dist()->max());
        }
        
        vector<double> geom_means(sample_count_table.size(), 0.0);
        
//        for (size_t i = 0; i < sample_count_table.size(); ++i)
//        {
//            scale_factors.push_back(1/sample_masses[i]);
//        }
        
        for (size_t i = 0; i < sample_count_table.size(); ++i)
        {
            pair<string, vector<double> >& p = sample_count_table[i];
            
            for (size_t j = 0; j < p.second.size(); ++j)
            {
                assert (geom_means.size() > j);
                if (geom_means[i] > 0  && p.second[j] > 0)
                {
                    geom_means[i] *= p.second[j];
                }
                else if (p.second[j] > 0)
                {
                    geom_means[i] = p.second[j];
                }
            }
            geom_means[i] = pow(geom_means[i], 1.0/p.second.size());
        }
        

        
        vector<double> scale_factors(_factories.size(), 0.0);
        for (size_t j = 0; j < scale_factors.size(); ++j)
        {
            vector<double> tmp_counts;
            for (size_t i = 0; i < sample_count_table.size(); ++i)
            {
                if (geom_means[i])
                    tmp_counts.push_back(sample_count_table[i].second[j] / geom_means[i]);
            }
            sort(tmp_counts.begin(), tmp_counts.end());
            if (!tmp_counts.empty())
                scale_factors[j] = tmp_counts[tmp_counts.size()/2];
            else
                scale_factors[j] = 0.0;
        }
        
        // Transform raw counts to the common scale
        for (size_t i = 0; i < sample_count_table.size(); ++i)
        {
            pair<string, vector<double> >& p = sample_count_table[i];
            for (size_t j = 0; j < p.second.size(); ++j)
            {
                assert (scale_factors.size() > j);
                p.second[j] *= (1.0 / scale_factors[j]);
            }
        }
        
        vector<pair<double, double> > raw_means_and_vars;
        
        for (size_t i = 0; i < sample_count_table.size(); ++i)
        {
            pair<string, vector<double> >& p = sample_count_table[i];
            double mean = accumulate(p.second.begin(), p.second.end(), 0.0);
            if (mean > 0.0)
                mean /= p.second.size();
            
            double var = 0.0;
            foreach (double d, p.second)
            {
                var += (d - mean) * (d - mean);
            }
            if (var > 0.0)
                var /= p.second.size();
        
            raw_means_and_vars.push_back(make_pair(mean, var));
        }
        
        sort(raw_means_and_vars.begin(), raw_means_and_vars.end());
        char sample_name_buf[256];
        int sample_id = rand();
        sprintf(sample_name_buf, "%d_counts.txt", sample_id);
        FILE* sample_count_file = fopen(sample_name_buf, "w");
        
        if (sample_count_file)
        {
            fprintf(sample_count_file, "count_mean\tcount_var\n");
            for (size_t i = 0; i < raw_means_and_vars.size(); ++i)
            {
                if (raw_means_and_vars[i].first > 0)
                {
                    fprintf(sample_count_file, "%lg\t%lg\n", 
                            raw_means_and_vars[i].first, 
                            raw_means_and_vars[i].second);
                }
            }
            fclose(sample_count_file);
        }
    }
	
    
    // This function NEEDS to deep copy the ref_mRNAs, otherwise cuffdiff'd
    // samples will clobber each other
    void set_ref_rnas(const vector<shared_ptr<Scaffold> >& mRNAs)
    {
        foreach(shared_ptr<BundleFactory> fac, _factories)
        {
            fac->set_ref_rnas(mRNAs);
        }
    }
    
    void set_mask_rnas(const vector<shared_ptr<Scaffold> >& mRNAs)
    {
        foreach(shared_ptr<BundleFactory> fac, _factories)
        {
            fac->set_mask_rnas(mRNAs);
        }
    }
    
private:
	vector<shared_ptr<BundleFactory> > _factories;
};


struct SampleAbundances
{
    string locus_tag;
	AbundanceGroup transcripts;
	vector<AbundanceGroup> primary_transcripts;
	vector<AbundanceGroup> gene_primary_transcripts;
	vector<AbundanceGroup> cds;
	vector<AbundanceGroup> gene_cds;
	vector<AbundanceGroup> genes;
	double cluster_mass;
};

extern double min_read_count;

void sample_worker(const RefSequenceTable& rt,
                   ReplicatedBundleFactory& sample_factory,
                   shared_ptr<SampleAbundances> abundance,
                   shared_ptr<bool> non_empty);

void test_differential(const string& locus_tag,
					   const vector<shared_ptr<SampleAbundances> >& samples,
					   Tests& tests,
					   Tracking& tracking,
                       bool samples_are_time_series);

#if ENABLE_THREADS
void decr_pool_count();
#endif

#endif
