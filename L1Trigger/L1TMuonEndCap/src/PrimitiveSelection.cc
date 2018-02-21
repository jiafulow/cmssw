#include "L1Trigger/L1TMuonEndCap/interface/PrimitiveSelection.h"

#include "DataFormats/MuonDetId/interface/DTChamberId.h"
#include "DataFormats/MuonDetId/interface/CSCDetId.h"
#include "DataFormats/MuonDetId/interface/RPCDetId.h"
#include "DataFormats/MuonDetId/interface/GEMDetId.h"
#include "L1Trigger/L1TMuonEndCap/interface/EMTFGEMDetId.h"
#include "L1Trigger/L1TMuonEndCap/interface/EMTFGEMDetIdImpl.h"


#include "helper.h"  // merge_map_into_map, assert_no_abort

#define NUM_CSC_CHAMBERS 6*9   // 18 in ME1; 9x3 in ME2,3,4; 9 from neighbor sector.
                               // Arranged in FW as 6 stations, 9 chambers per station.
#define NUM_RPC_CHAMBERS 7*8   // 6x2 in RE1,2; 12x2 in RE3,4; 6 from neighbor sector.
                               // Arranged in FW as 7 stations, 6 chambers per station. (8 with iRPC)
#define NUM_GEM_CHAMBERS 6*9   // 6 in GE1/1; 3 in GE2/1; 2 from neighbor sector.
                               // Arranged in FW as 6 stations, 9 chambers per station, mimicking CSC. (unconfirmed!)


void PrimitiveSelection::configure(
      int verbose, int endcap, int sector, int bx,
      int bxShiftCSC, int bxShiftRPC, int bxShiftGEM,
      bool includeNeighbor, bool duplicateTheta,
      bool bugME11Dupes
) {
  verbose_ = verbose;
  endcap_  = endcap;
  sector_  = sector;
  bx_      = bx;

  bxShiftCSC_      = bxShiftCSC;
  bxShiftRPC_      = bxShiftRPC;
  bxShiftGEM_      = bxShiftGEM;

  includeNeighbor_ = includeNeighbor;
  duplicateTheta_  = duplicateTheta;
  bugME11Dupes_    = bugME11Dupes;
}


// _____________________________________________________________________________
// Specialized process() for CSC
template<>
void PrimitiveSelection::process(
    CSCTag tag,
    const TriggerPrimitiveCollection& muon_primitives,
    std::map<int, TriggerPrimitiveCollection>& selected_csc_map
) const {
  TriggerPrimitiveCollection::const_iterator tp_it  = muon_primitives.begin();
  TriggerPrimitiveCollection::const_iterator tp_end = muon_primitives.end();

  for (; tp_it != tp_end; ++tp_it) {
    TriggerPrimitive new_tp = *tp_it;  // make a copy and apply patches to this copy

    // Patch the CLCT pattern number
    // It should be 0-10, see: L1Trigger/CSCTriggerPrimitives/src/CSCMotherboard.cc
    bool patchPattern = true;
    if (patchPattern && new_tp.subsystem() == TriggerPrimitive::kCSC) {
      if (new_tp.getCSCData().pattern == 11 || new_tp.getCSCData().pattern == 12 || new_tp.getCSCData().pattern == 13 || new_tp.getCSCData().pattern == 14) {  // 11, 12, 13, 14 -> 10
        edm::LogWarning("L1T") << "EMTF patching corrupt CSC LCT pattern: changing " << new_tp.getCSCData().pattern << " to 10";
        new_tp.accessCSCData().pattern = 10;
      }
    }

    // Patch the LCT quality number
    // It should be 1-15, see: L1Trigger/CSCTriggerPrimitives/src/CSCMotherboard.cc
    bool patchQuality = true;
    if (patchQuality && new_tp.subsystem() == TriggerPrimitive::kCSC) {
      if (new_tp.getCSCData().quality == 0) {  // 0 -> 1
        edm::LogWarning("L1T") << "EMTF patching corrupt CSC LCT quality: changing " << new_tp.getCSCData().quality << " to 1";
        new_tp.accessCSCData().quality = 1;
      }
    }

    int selected_csc = select_csc(new_tp); // Returns CSC "link" index (0 - 53)

    if (selected_csc >= 0) {
      assert(selected_csc < NUM_CSC_CHAMBERS);
      selected_csc_map[selected_csc].push_back(new_tp);
    }
  }

  // Duplicate CSC muon primitives
  // If there are 2 LCTs in the same chamber with (strip, wire) = (s1, w1) and (s2, w2)
  // make all combinations with (s1, w1), (s2, w1), (s1, w2), (s2, w2)
  if (duplicateTheta_) {
    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_it  = selected_csc_map.begin();
    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_end = selected_csc_map.end();

    for (; map_tp_it != map_tp_end; ++map_tp_it) {
      int selected = map_tp_it->first;
      TriggerPrimitiveCollection& tmp_primitives = map_tp_it->second;  // pass by reference

      if (tmp_primitives.size() >= 4) {
        edm::LogWarning("L1T") << "EMTF found 4 or more CSC LCTs in one chamber: keeping only two";
        tmp_primitives.erase(tmp_primitives.begin() + 4, tmp_primitives.end());  // erase 5th element++
        tmp_primitives.erase(tmp_primitives.begin() + 2);  // erase 3rd element
        tmp_primitives.erase(tmp_primitives.begin() + 1);  // erase 2nd element
      } else if (tmp_primitives.size() == 3) {
        edm::LogWarning("L1T") << "EMTF found 3 CSC LCTs in one chamber: keeping only two";
        tmp_primitives.erase(tmp_primitives.begin() + 2);  // erase 3rd element
      }
      assert(tmp_primitives.size() <= 2);  // at most 2 hits

      if (tmp_primitives.size() == 2) {
        if (
            (tmp_primitives.at(0).getStrip() != tmp_primitives.at(1).getStrip()) &&
            (tmp_primitives.at(0).getWire() != tmp_primitives.at(1).getWire())
        ) {
          // Swap wire numbers
          TriggerPrimitive tp0 = tmp_primitives.at(0);  // (s1,w1)
          TriggerPrimitive tp1 = tmp_primitives.at(1);  // (s2,w2)
          uint16_t tmp_keywire        = tp0.accessCSCData().keywire;
          tp0.accessCSCData().keywire = tp1.accessCSCData().keywire;  // (s1,w2)
          tp1.accessCSCData().keywire = tmp_keywire;                  // (s2,w1)

          tmp_primitives.insert(tmp_primitives.begin()+1, tp1);  // (s2,w1) at 2nd pos
          tmp_primitives.insert(tmp_primitives.begin()+2, tp0);  // (s1,w2) at 3rd pos
        }

        const bool is_csc_me11 = (0 <= selected && selected <= 2) || (9 <= selected && selected <= 11) || (selected == 45);  // ME1/1 sub 1 or ME1/1 sub 2 or ME1/1 from neighbor

        if (bugME11Dupes_ && is_csc_me11) {
          // For ME1/1, always make 4 LCTs without checking strip & wire combination
          if (tmp_primitives.size() == 2) {
            // Swap wire numbers
            TriggerPrimitive tp0 = tmp_primitives.at(0);  // (s1,w1)
            TriggerPrimitive tp1 = tmp_primitives.at(1);  // (s2,w2)
            uint16_t tmp_keywire        = tp0.accessCSCData().keywire;
            tp0.accessCSCData().keywire = tp1.accessCSCData().keywire;  // (s1,w2)
            tp1.accessCSCData().keywire = tmp_keywire;                  // (s2,w1)

            tmp_primitives.insert(tmp_primitives.begin()+1, tp1);  // (s2,w1) at 2nd pos
            tmp_primitives.insert(tmp_primitives.begin()+2, tp0);  // (s1,w2) at 3rd pos
          }
          assert(tmp_primitives.size() == 1 || tmp_primitives.size() == 4);
        }

      }  // end if tmp_primitives.size() == 2
    }  // end loop over selected_csc_map
  }  // end if duplicate theta
}


// _____________________________________________________________________________
// Specialized process() for RPC
template<>
void PrimitiveSelection::process(
    RPCTag tag,
    const TriggerPrimitiveCollection& muon_primitives,
    std::map<int, TriggerPrimitiveCollection>& selected_rpc_map
) const {
  TriggerPrimitiveCollection::const_iterator tp_it  = muon_primitives.begin();
  TriggerPrimitiveCollection::const_iterator tp_end = muon_primitives.end();

  for (; tp_it != tp_end; ++tp_it) {
    int selected_rpc = select_rpc(*tp_it);  // Returns RPC "link" index (0 - 41)

    if (selected_rpc >= 0) {
      assert(selected_rpc < NUM_RPC_CHAMBERS);
      selected_rpc_map[selected_rpc].push_back(*tp_it);
    }
  }

  // Apply truncation as in firmware: keep first 2 clusters, max cluster
  // size = 3 strips.
  // According to Karol Bunkowski, for one chamber (so 3 eta rolls) only up
  // to 2 hits (cluster centres) are produced. First two 'first' clusters are
  // chosen, and only after the cut on the cluster size is applied. So if
  // there are 1 large cluster and 2 small clusters, it is possible that
  // one of the two small clusters is discarded first, and the large cluster
  // then is removed by the cluster size cut, leaving only one cluster.
  bool apply_truncation = true;
  if (apply_truncation) {
    struct {
      typedef TriggerPrimitive value_type;
      bool operator()(const value_type& x) const {
        int sz = x.getRPCData().strip_hi - x.getRPCData().strip_low + 1;

        const RPCDetId& tp_detId = x.detId<RPCDetId>();
        int tp_station     = tp_detId.station();
        int tp_ring        = tp_detId.ring();
        const bool is_irpc = (tp_station == 3 || tp_station == 4) && (tp_ring == 1);
        if (is_irpc)  sz /= 3;  // iRPC strip pitch is 3 times smaller than traditional RPC

        return sz > 3;
      }
    } cluster_size_cut;

    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_it  = selected_rpc_map.begin();
    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_end = selected_rpc_map.end();

    for (; map_tp_it != map_tp_end; ++map_tp_it) {
      //int selected = map_tp_it->first;
      TriggerPrimitiveCollection& tmp_primitives = map_tp_it->second;  // pass by reference

      // Keep the first two clusters
      if (tmp_primitives.size() > 2)
        tmp_primitives.erase(tmp_primitives.begin()+2, tmp_primitives.end());

      // Apply cluster size cut
      tmp_primitives.erase(
          std::remove_if(tmp_primitives.begin(), tmp_primitives.end(), cluster_size_cut),
          tmp_primitives.end()
      );
    }
  }  // end if apply_truncation

  // Map RPC subsector and chamber to CSC chambers
  // Note: RE3/2 & RE3/3 are considered as one chamber; RE4/2 & RE4/3 too.
  bool map_rpc_to_csc = true;
  if (map_rpc_to_csc) {
    std::map<int, TriggerPrimitiveCollection> tmp_selected_rpc_map;

    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_it  = selected_rpc_map.begin();
    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_end = selected_rpc_map.end();

    for (; map_tp_it != map_tp_end; ++map_tp_it) {
      int selected = map_tp_it->first;
      TriggerPrimitiveCollection& tmp_primitives = map_tp_it->second;  // pass by reference

      int rpc_sub = selected / 8;
      int rpc_chm = selected % 8;

      int pc_station = -1;
      int pc_chamber = -1;

      if (rpc_sub != 6) {  // native
        if (rpc_chm == 0) {  // RE1/2
          if (0 <= rpc_sub && rpc_sub < 3) {
            pc_station = 0;
            pc_chamber = 3 + rpc_sub;
          } else if (3 <= rpc_sub && rpc_sub < 6) {
            pc_station = 1;
            pc_chamber = 3 + (rpc_sub - 3);
          }
        } else if (rpc_chm == 1) {  // RE2/2
           pc_station = 2;
           pc_chamber = 3 + rpc_sub;
        } else if (2 <= rpc_chm && rpc_chm <= 3) {  // RE3/2, RE3/3
           pc_station = 3;
           pc_chamber = 3 + rpc_sub;
        } else if (4 <= rpc_chm && rpc_chm <= 5) {  // RE4/2, RE4/3
           pc_station = 4;
           pc_chamber = 3 + rpc_sub;
        } else if (rpc_chm == 6) {  // RE3/1
          pc_station = 3;
          pc_chamber = rpc_sub;
        } else if (rpc_chm == 7) {  // RE4/1
          pc_station = 4;
          pc_chamber = rpc_sub;
        }

      } else {  // neighbor
        pc_station = 5;
        if (rpc_chm == 0) {  // RE1/2
          pc_chamber = 1;
        } else if (rpc_chm == 1) {  // RE2/2
          pc_chamber = 4;
        } else if (2 <= rpc_chm && rpc_chm <= 3) {  // RE3/2, RE3/3
          pc_chamber = 6;
        } else if (4 <= rpc_chm && rpc_chm <= 5) {  // RE4/2, RE4/3
          pc_chamber = 8;
        } else if (rpc_chm == 6) {  // RE3/1
          pc_chamber = 5;
        } else if (rpc_chm == 7) {  // RE4/1
          pc_chamber = 7;
        }
      }

      assert(pc_station != -1 && pc_chamber != -1);

      selected = (pc_station * 9) + pc_chamber;

      bool ignore_this_rpc_chm = false;
      if (rpc_chm == 3 || rpc_chm == 5) { // special case of RE3,4/2 and RE3,4/3 chambers
        // if RE3,4/2 exists, ignore RE3,4/3. In C++, this assumes that the loop
        // over selected_rpc_map will always find RE3,4/2 before RE3,4/3
        if (tmp_selected_rpc_map.find(selected) != tmp_selected_rpc_map.end())
          ignore_this_rpc_chm = true;
      }

      if (ignore_this_rpc_chm) {
        // Set RPC stubs as invalid
        for (auto&& tp : tmp_primitives) {
          tp.accessRPCData().valid = 0;
        }
      }

      if (tmp_selected_rpc_map.find(selected) == tmp_selected_rpc_map.end()) {
        tmp_selected_rpc_map[selected] = tmp_primitives;
      } else {
        tmp_selected_rpc_map[selected].insert(tmp_selected_rpc_map[selected].end(), tmp_primitives.begin(), tmp_primitives.end());
      }
    }  // end loop over selected_rpc_map

    std::swap(selected_rpc_map, tmp_selected_rpc_map);  // replace the original map
  }  // end if map_rpc_to_csc
}


// _____________________________________________________________________________
// Specialized process() for GEM
template<>
void PrimitiveSelection::process(
    GEMTag tag,
    const TriggerPrimitiveCollection& muon_primitives,
    std::map<int, TriggerPrimitiveCollection>& selected_gem_map
) const {
  TriggerPrimitiveCollection::const_iterator tp_it  = muon_primitives.begin();
  TriggerPrimitiveCollection::const_iterator tp_end = muon_primitives.end();

  for (; tp_it != tp_end; ++tp_it) {
    int selected_gem = select_gem(*tp_it);  // Returns GEM "link" index (0 - 53)

    if (selected_gem >= 0) {
      assert(selected_gem < NUM_GEM_CHAMBERS);
      selected_gem_map[selected_gem].push_back(*tp_it);
    }
  }

  // Apply truncation: max cluster size = 8 pads, keep first 8 clusters.
  bool apply_truncation = true;
  if (apply_truncation) {
    struct {
      typedef TriggerPrimitive value_type;
      bool operator()(const value_type& x) const {
        int sz = x.getGEMData().pad_hi - x.getGEMData().pad_low + 1;
        return sz > 8;
      }
    } cluster_size_cut;

    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_it  = selected_gem_map.begin();
    std::map<int, TriggerPrimitiveCollection>::iterator map_tp_end = selected_gem_map.end();

    for (; map_tp_it != map_tp_end; ++map_tp_it) {
      //int selected = map_tp_it->first;
      TriggerPrimitiveCollection& tmp_primitives = map_tp_it->second;  // pass by reference

      // Apply cluster size cut
      tmp_primitives.erase(
          std::remove_if(tmp_primitives.begin(), tmp_primitives.end(), cluster_size_cut),
          tmp_primitives.end()
      );

      // Keep the first 8 clusters
      if (tmp_primitives.size() > 8)
        tmp_primitives.erase(tmp_primitives.begin()+8, tmp_primitives.end());
    }
  }  // end if apply_truncation
}


// _____________________________________________________________________________
// Put the hits from CSC, RPC, GEM together in one collection

// Notes from Alex (2017-03-28):
//
//     The RPC inclusion logic is very simple currently:
//     - each CSC is analyzed for having track stubs in each BX
//     - IF a CSC chamber is missing at least one track stub,
//         AND there is an RPC overlapping with it in phi and theta,
//         AND that RPC has hits,
//       THEN RPC hit is inserted instead of missing CSC stub.
//
//     This is done at the output of coord_delay module, so such
// inserted RPC hits can be matched to patterns by match_ph_segments
// module, just like any CSC stubs. Note that substitution of missing
// CSC stubs with RPC hits happens regardless of what's going on in
// other chambers, regardless of whether a pattern has been detected
// or not, basically regardless of anything. RPCs are treated as a
// supplemental source of stubs for CSCs.

void PrimitiveSelection::merge(
    const std::map<int, TriggerPrimitiveCollection>& selected_csc_map,
    const std::map<int, TriggerPrimitiveCollection>& selected_rpc_map,
    const std::map<int, TriggerPrimitiveCollection>& selected_gem_map,
    std::map<int, TriggerPrimitiveCollection>& selected_prim_map
) const {
  // First, put CSC hits
  std::map<int, TriggerPrimitiveCollection>::const_iterator map_tp_it  = selected_csc_map.begin();
  std::map<int, TriggerPrimitiveCollection>::const_iterator map_tp_end = selected_csc_map.end();

  for (; map_tp_it != map_tp_end; ++map_tp_it) {
    int selected_csc = map_tp_it->first;
    const TriggerPrimitiveCollection& csc_primitives = map_tp_it->second;
    assert(csc_primitives.size() <= 4);  // at most 4 hits, including duplicated hits

    // Insert all CSC hits
    selected_prim_map[selected_csc] = csc_primitives;
  }

  // Second, insert GEM stubs if there is no CSC hits
  map_tp_it  = selected_gem_map.begin();
  map_tp_end = selected_gem_map.end();

  for (; map_tp_it != map_tp_end; ++map_tp_it) {
    int selected_gem = map_tp_it->first;
    const TriggerPrimitiveCollection& gem_primitives = map_tp_it->second;
    if (gem_primitives.empty())  continue;
    assert(gem_primitives.size() <= 8);  // at most 8 hits

    bool found = (selected_prim_map.find(selected_gem) != selected_prim_map.end());
    if (!found) {
      // No CSC hits, insert all GEM hits
      selected_prim_map[selected_gem] = gem_primitives;

    } else {
      // Do nothing
    }
  }

  // Third, insert RPC stubs if there is no CSC/GEM hits
  map_tp_it  = selected_rpc_map.begin();
  map_tp_end = selected_rpc_map.end();

  for (; map_tp_it != map_tp_end; ++map_tp_it) {
    int selected_rpc = map_tp_it->first;
    const TriggerPrimitiveCollection& rpc_primitives = map_tp_it->second;
    if (rpc_primitives.empty())  continue;
    assert(rpc_primitives.size() <= 4);  // at most 4 hits

    bool found = (selected_prim_map.find(selected_rpc) != selected_prim_map.end());
    if (!found) {
      // No CSC/GEM hits, insert all RPC hits
      //selected_prim_map[selected_rpc] = rpc_primitives;

      // No CSC/GEM hits, insert the valid RPC hits
      TriggerPrimitiveCollection tmp_rpc_primitives;
      for (const auto& tp : rpc_primitives) {
        if (tp.getRPCData().valid != 0) {
          tmp_rpc_primitives.push_back(tp);
        }
      }
      assert(tmp_rpc_primitives.size() <= 2);  // at most 2 hits
      selected_prim_map[selected_rpc] = tmp_rpc_primitives;

    } else {
      // Initial FW in 2017; was disabled on June 7.
      // If only one CSC/GEM hit, insert the first RPC hit
      //TriggerPrimitiveCollection& tmp_primitives = selected_prim_map[selected_rpc];  // pass by reference

      //if (tmp_primitives.size() < 2) {
      //  tmp_primitives.push_back(rpc_primitives.front());
      //}
    }
  }
}

void PrimitiveSelection::merge_no_truncate(
    const std::map<int, TriggerPrimitiveCollection>& selected_csc_map,
    const std::map<int, TriggerPrimitiveCollection>& selected_rpc_map,
    const std::map<int, TriggerPrimitiveCollection>& selected_gem_map,
    std::map<int, TriggerPrimitiveCollection>& selected_prim_map
) const {
  // First, put CSC hits
  merge_map_into_map(selected_csc_map, selected_prim_map);

  // Second, insert GEM hits
  merge_map_into_map(selected_gem_map, selected_prim_map);

  // Third, insert RPC hits
  merge_map_into_map(selected_rpc_map, selected_prim_map);
}


// _____________________________________________________________________________
// CSC functions
int PrimitiveSelection::select_csc(const TriggerPrimitive& muon_primitive) const {
  int selected = -1;

  if (muon_primitive.subsystem() == TriggerPrimitive::kCSC) {
    const CSCDetId& tp_detId = muon_primitive.detId<CSCDetId>();
    const CSCData&  tp_data  = muon_primitive.getCSCData();

    int tp_endcap    = tp_detId.endcap();
    int tp_sector    = tp_detId.triggerSector();
    int tp_station   = tp_detId.station();
    int tp_ring      = tp_detId.ring();
    int tp_chamber   = tp_detId.chamber();

    int tp_bx        = tp_data.bx;
    int tp_csc_ID    = tp_data.cscID;

    assert_no_abort(emtf::MIN_ENDCAP <= tp_endcap && tp_endcap <= emtf::MAX_ENDCAP);
    assert_no_abort(emtf::MIN_TRIGSECTOR <= tp_sector && tp_sector <= emtf::MAX_TRIGSECTOR);
    assert_no_abort(1 <= tp_station && tp_station <= 4);
    assert_no_abort(1 <= tp_csc_ID && tp_csc_ID <= 9);
    assert_no_abort(tp_data.strip < 160);
    //assert_no_abort(tp_data.keywire < 112);
    assert_no_abort(tp_data.keywire < 128);
    assert_no_abort(tp_data.valid == true);
    assert_no_abort(tp_data.pattern <= 10);
    assert_no_abort(tp_data.quality > 0);


    // Check using ME1/1a --> ring 4 convention
    if (tp_station == 1 && tp_ring == 1) {
      assert(tp_data.strip < 128);
      assert(1 <= tp_csc_ID && tp_csc_ID <= 3);
    }
    if (tp_station == 1 && tp_ring == 4) {
      assert(tp_data.strip < 128);
      assert(1 <= tp_csc_ID && tp_csc_ID <= 3);
    }

    // station 1 --> subsector 1 or 2
    // station 2,3,4 --> subsector 0
    int tp_subsector = (tp_station != 1) ? 0 : ((tp_chamber%6 > 2) ? 1 : 2);

    // Selection
    if (is_in_bx_csc(tp_bx)) {
      if (is_in_sector_csc(tp_endcap, tp_sector)) {
        selected = get_index_csc(tp_subsector, tp_station, tp_csc_ID, false);
      } else if (is_in_neighbor_sector_csc(tp_endcap, tp_sector, tp_subsector, tp_station, tp_csc_ID)) {
        selected = get_index_csc(tp_subsector, tp_station, tp_csc_ID, true);
      }
    }
  }
  return selected;
}

bool PrimitiveSelection::is_in_sector_csc(int tp_endcap, int tp_sector) const {
  return ((endcap_ == tp_endcap) && (sector_ == tp_sector));
}

bool PrimitiveSelection::is_in_neighbor_sector_csc(int tp_endcap, int tp_sector, int tp_subsector, int tp_station, int tp_csc_ID) const {
  auto get_neighbor = [](int sector) {
    return (sector == 1) ? 6 : sector - 1;
  };

  if (includeNeighbor_) {
    if ((endcap_ == tp_endcap) && (get_neighbor(sector_) == tp_sector)) {
      if (tp_station == 1) {
        if ((tp_subsector == 2) && (tp_csc_ID == 3 || tp_csc_ID == 6 || tp_csc_ID == 9))
          return true;

      } else {
        if (tp_csc_ID == 3 || tp_csc_ID == 9)
          return true;
      }
    }
  }
  return false;
}

bool PrimitiveSelection::is_in_bx_csc(int tp_bx) const {
  tp_bx += bxShiftCSC_;
  return (bx_ == tp_bx);
}

// Returns CSC input "link".  Index used by FW for unique chamber identification.
int PrimitiveSelection::get_index_csc(int tp_subsector, int tp_station, int tp_csc_ID, bool is_neighbor) const {
  int selected = -1;

  if (!is_neighbor) {
    if (tp_station == 1) {  // ME1: 0 - 8, 9 - 17
      selected = (tp_subsector-1) * 9 + (tp_csc_ID-1);
    } else {                // ME2,3,4: 18 - 26, 27 - 35, 36 - 44
      selected = (tp_station) * 9 + (tp_csc_ID-1);
    }

  } else {
    if (tp_station == 1) {  // ME1: 45 - 47
      selected = (5) * 9 + (tp_csc_ID-1)/3;
    } else {                // ME2,3,4: 48 - 53
      selected = (5) * 9 + (tp_station) * 2 - 1 + (tp_csc_ID-1 < 3 ? 0 : 1);
    }
  }
  return selected;
}


// _____________________________________________________________________________
// RPC functions
int PrimitiveSelection::select_rpc(const TriggerPrimitive& muon_primitive) const {
  int selected = -1;

  if (muon_primitive.subsystem() == TriggerPrimitive::kRPC) {
    const RPCDetId& tp_detId = muon_primitive.detId<RPCDetId>();
    const RPCData&  tp_data  = muon_primitive.getRPCData();

    int tp_region    = tp_detId.region();     // 0 for Barrel, +/-1 for +/- Endcap
    int tp_endcap    = (tp_region == -1) ? 2 : tp_region;
    int tp_sector    = tp_detId.sector();     // 1 - 6 (60 degrees in phi, sector 1 begins at -5 deg)
    int tp_subsector = tp_detId.subsector();  // 1 - 6 (10 degrees in phi; staggered in z)
    int tp_station   = tp_detId.station();    // 1 - 4
    int tp_ring      = tp_detId.ring();       // 2 - 3 (increasing theta)
    int tp_roll      = tp_detId.roll();       // 1 - 3 (decreasing theta; aka A - C; space between rolls is 9 - 15 in theta_fp)
    //int tp_layer     = tp_detId.layer();

    int tp_bx        = tp_data.bx;
    int tp_strip     = tp_data.strip;

    const bool is_irpc = (tp_station == 3 || tp_station == 4) && (tp_ring == 1);

    assert_no_abort(tp_region != 0);
    assert_no_abort(emtf::MIN_ENDCAP <= tp_endcap && tp_endcap <= emtf::MAX_ENDCAP);
    assert_no_abort(emtf::MIN_TRIGSECTOR <= tp_sector && tp_sector <= emtf::MAX_TRIGSECTOR);
    assert_no_abort(1 <= tp_subsector && tp_subsector <= 6);
    assert_no_abort(1 <= tp_station && tp_station <= 4);
    assert_no_abort((!is_irpc && 2 <= tp_ring && tp_ring <= 3) || (is_irpc && 1 <= tp_ring && tp_ring <= 3));
    assert_no_abort((!is_irpc && 1 <= tp_roll && tp_roll <= 3) || (is_irpc && 1 <= tp_roll && tp_roll <= 5));
    assert_no_abort((!is_irpc && 1 <= tp_strip && tp_strip <= 32) || (is_irpc && 1 <= tp_strip && tp_strip <= 192));
    assert_no_abort(tp_station > 2 || tp_ring != 3);  // stations 1 and 2 do not receive RPCs from ring 3
    assert_no_abort(tp_data.valid == true);


    // Selection
    if (is_in_bx_rpc(tp_bx)) {
      if (is_in_sector_rpc(tp_endcap, tp_station, tp_ring, tp_sector, tp_subsector)) {
        selected = get_index_rpc(tp_station, tp_ring, tp_subsector, false);
      } else if (is_in_neighbor_sector_rpc(tp_endcap, tp_station, tp_ring, tp_sector, tp_subsector)) {
        selected = get_index_rpc(tp_station, tp_ring, tp_subsector, true);
      }
    }
  }
  return selected;
}

bool PrimitiveSelection::is_in_sector_rpc(int tp_endcap, int tp_station, int tp_ring, int tp_sector, int tp_subsector) const {
  // RPC sector X, subsectors 1-2 corresponds to CSC sector X-1
  // RPC sector X, subsectors 3-6 corresponds to CSC sector X
  auto get_csc_sector = [](int tp_station, int tp_ring, int tp_sector, int tp_subsector) {
    const bool is_irpc = (tp_station == 3 || tp_station == 4) && (tp_ring == 1);
    if (is_irpc) {
      // 20 degree chamber
      int corr = (tp_subsector < 2) ? (tp_sector == 1 ? +5 : -1) : 0;
      return tp_sector + corr;
    } else {
      // 10 degree chamber
      int corr = (tp_subsector < 3) ? (tp_sector == 1 ? +5 : -1) : 0;
      return tp_sector + corr;
    }
  };
  return ((endcap_ == tp_endcap) && (sector_ == get_csc_sector(tp_station, tp_ring, tp_sector, tp_subsector)));
}

bool PrimitiveSelection::is_in_neighbor_sector_rpc(int tp_endcap, int tp_station, int tp_ring, int tp_sector, int tp_subsector) const {
  auto get_csc_neighbor_subsector = [](int tp_station, int tp_ring) {
    const bool is_irpc = (tp_station == 3 || tp_station == 4) && (tp_ring == 1);
    if (is_irpc) {
      // 20 degree chamber
      return 1;
    } else {
      // 10 degree chamber
      return 2;
    }
  };
  return (includeNeighbor_ && (endcap_ == tp_endcap) && (sector_ == tp_sector) && (tp_subsector == get_csc_neighbor_subsector(tp_station, tp_ring)));
}

bool PrimitiveSelection::is_in_bx_rpc(int tp_bx) const {
  tp_bx += bxShiftRPC_;
  return (bx_ == tp_bx);
}

int PrimitiveSelection::get_index_rpc(int tp_station, int tp_ring, int tp_subsector, bool is_neighbor) const {
  int selected = -1;

  // CPPF RX data come in 3 frames x 64 bits, for 7 links. Each 64-bit data
  // carry 2 words of 32 bits. Each word carries phi (11 bits) and theta (5 bits)
  // of 2 segments (x2).
  //
  // Firmware uses 'rpc_sub' as RPC subsector index and 'rpc_chm' as RPC chamber index
  // rpc_sub [0,6] = RPC subsector 3, 4, 5, 6, 1 from neighbor, 2 from neighbor, 2. They correspond to
  //                 CSC sector phi 0-10 deg, 10-20, 20-30, 30-40, 40-50, 50-60, 50-60 from neighbor
  // rpc_chm [0,5] = RPC chamber RE1/2, RE2/2, RE3/2, RE3/3, RE4/2, RE4/3
  //
  int rpc_sub = -1;
  int rpc_chm = -1;

  if (!is_neighbor) {
    rpc_sub = ((tp_subsector + 3) % 6);
  } else {
    rpc_sub = 6;
  }

  if (tp_station <= 2) {
    rpc_chm = (tp_station - 1);
  } else {
    rpc_chm = 2 + (tp_station - 3)*2 + (tp_ring - 2);
  }

  // Numbering for iRPC (20 degree chambers)
  const bool is_irpc = (tp_station == 3 || tp_station == 4) && (tp_ring == 1);
  if (is_irpc) {
    if (!is_neighbor) {
      rpc_sub = ((tp_subsector + 1) % 3);
    } else {
      rpc_sub = 6;
    }

    if (tp_station == 3) {
      rpc_chm = 6;
    } else if (tp_station == 4) {
      rpc_chm = 7;
    }
  }

  assert(rpc_sub != -1 && rpc_chm != -1);

  selected = (rpc_sub * 8) + rpc_chm;
  return selected;
}


// _____________________________________________________________________________
// GEM functions
int PrimitiveSelection::select_gem(const TriggerPrimitive& muon_primitive) const {
  int selected = -1;

  if (muon_primitive.subsystem() == TriggerPrimitive::kGEM) {
    const EMTFGEMDetId& tp_detId = emtf::construct_EMTFGEMDetId(muon_primitive);
    const GEMData&      tp_data  = muon_primitive.getGEMData();

    int tp_region    = tp_detId.region();     // 0 for Barrel, +/-1 for +/- Endcap
    int tp_endcap    = (tp_region == -1) ? 2 : tp_region;
    int tp_station   = tp_detId.station();
    int tp_ring      = tp_detId.ring();
    int tp_roll      = tp_detId.roll();
    int tp_layer     = tp_detId.layer();
    int tp_chamber   = tp_detId.chamber();

    int tp_bx        = tp_data.bx;
    int tp_pad       = tp_data.pad;

    const bool is_me0 = tp_data.isME0;

    // Use CSC trigger sector definitions
    // Code copied from DataFormats/MuonDetId/src/CSCDetId.cc
    auto get_trigger_sector = [](int ring, int station, int chamber) {
      int result = 0;
      if( station > 1 && ring > 1 ) {
        result = ((static_cast<unsigned>(chamber-3) & 0x7f) / 6) + 1; // ch 3-8->1, 9-14->2, ... 1,2 -> 6
      }
      else if( station == 1 && ring != 4 ) {
        result = ((static_cast<unsigned>(chamber-3) & 0x7f) / 6) + 1; // ch 3-8->1, 9-14->2, ... 1,2 -> 6
      }
      else {
        result = ((static_cast<unsigned>(chamber-2) & 0x1f) / 3) + 1; // ch 2-4-> 1, 5-7->2, ...
      }
      return (result <= 6) ? result : 6; // max sector is 6, some calculations give a value greater than six but this is expected.
    };

    // Use CSC trigger "CSC ID" definitions
    // Code copied from DataFormats/MuonDetId/src/CSCDetId.cc
    auto get_trigger_csc_ID = [](int ring, int station, int chamber) {
      int result = 0;
      if( station == 1 ) {
        result = (chamber) % 3 + 1; // 1,2,3
        switch (ring) {
        case 1:
          break;
        case 2:
          result += 3; // 4,5,6
          break;
        case 3:
          result += 6; // 7,8,9
          break;
        case 4:  // ME0
          result = (chamber+1) % 3 + 1; // 1,2,3
          break;
        }
      }
      else {
        if( ring == 1 ) {
          result = (chamber+1) % 3 + 1; // 1,2,3
        }
        else {
          result = (chamber+3) % 6 + 4; // 4,5,6,7,8,9
        }
      }
      return result;
    };

    int tp_sector    = get_trigger_sector(tp_ring, tp_station, tp_chamber);
    int tp_csc_ID    = get_trigger_csc_ID(tp_ring, tp_station, tp_chamber);

    // station 1 --> subsector 1 or 2
    // station 2,3,4 --> subsector 0
    int tp_subsector = (tp_station != 1) ? 0 : ((tp_chamber%6 > 2) ? 1 : 2);
    if (is_me0)  tp_subsector = 2;

    assert_no_abort(emtf::MIN_ENDCAP <= tp_endcap && tp_endcap <= emtf::MAX_ENDCAP);
    assert_no_abort(emtf::MIN_TRIGSECTOR <= tp_sector && tp_sector <= emtf::MAX_TRIGSECTOR);
    assert_no_abort(1 <= tp_station && tp_station <= 2);
    assert_no_abort(tp_ring == 1 || tp_ring == 4);
    assert_no_abort(1 <= tp_roll && tp_roll <= 8);
    assert_no_abort((!is_me0 && 1 <= tp_layer && tp_layer <= 2) || (is_me0 && 1 <= tp_layer && tp_layer <= 6));
    assert_no_abort(1 <= tp_csc_ID && tp_csc_ID <= 9);
    //assert_no_abort(tp_data.pad < 192);
    assert_no_abort((tp_station == 1 && 1 <= tp_pad && tp_pad <= 192) || (tp_station != 1));
    assert_no_abort((tp_station == 2 && 1 <= tp_pad && tp_pad <= 384) || (tp_station != 2));


    // Selection
    if (is_in_bx_gem(tp_bx)) {
      if (is_in_sector_gem(tp_endcap, tp_sector)) {
        selected = get_index_gem(tp_subsector, tp_station, tp_csc_ID, false);
      } else if (is_in_neighbor_sector_gem(tp_endcap, tp_sector, tp_subsector, tp_station, tp_csc_ID)) {
        selected = get_index_gem(tp_subsector, tp_station, tp_csc_ID, true);
      }
    }
  }
  return selected;
}

bool PrimitiveSelection::is_in_sector_gem(int tp_endcap, int tp_sector) const {
  // Identical to the corresponding CSC function
  return is_in_sector_csc(tp_endcap, tp_sector);
}

bool PrimitiveSelection::is_in_neighbor_sector_gem(int tp_endcap, int tp_sector, int tp_subsector, int tp_station, int tp_csc_ID) const {
  // Identical to the corresponding CSC function
  return is_in_neighbor_sector_csc(tp_endcap, tp_sector, tp_subsector, tp_station, tp_csc_ID);
}

bool PrimitiveSelection::is_in_bx_gem(int tp_bx) const {
  tp_bx += bxShiftGEM_;
  return (bx_ == tp_bx);
}

int PrimitiveSelection::get_index_gem(int tp_subsector, int tp_station, int tp_csc_ID, bool is_neighbor) const {
  // Identical to the corresponding CSC function
  return get_index_csc(tp_subsector, tp_station, tp_csc_ID, is_neighbor);
}
