#ifndef ASSEMBLE_H
#define ASSEMBLE_H

/*
 *  assemble.h
 *  cufflinks
 *
 *  Created by Cole Trapnell on 3/23/09.
 *  Copyright 2009 Cole Trapnell. All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <vector>
#include <map>

//#include <lemon/list_graph.h>

//#include <lemon/concepts/bpugraph.h>
#include <boost/graph/adjacency_list.hpp>

#include "hits.h"
#include "bundles.h"
#include "scaffolds.h"
#include "biascorrection.h"


bool assemble_hits(BundleFactory& bundle_factory, BiasLearner* bl_p);

//bool intron_compatible(const MateHit& lhs, const MateHit& rhs);
bool read_hits_overlap(const ReadHit* lhs, const ReadHit* rhs);
bool read_hits_intron_agree(const ReadHit* h1, const ReadHit* h2);

int  match_length(const MateHit& m, int left, int right);


bool mate_graphs(const HitBundle& bundle, BundleStats* stats);



bool make_scaffolds(const EmpDist& frag_len_dist, 
                    int bundle_left,
					int bundle_length,
					vector<Scaffold>& hits,
					vector<Scaffold>& scaffolds);

#endif
