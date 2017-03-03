#include "L1Trigger/L1TMuonEndCap/interface/PrimitiveMatching.hh"

#include "helper.hh"  // to_hex, to_binary

namespace {
  static const int bw_fph = 13;  // bit width of ph, full precision
  static const int bpow = 7;     // (1 << bpow) is count of input ranks
  static const int invalid_ph_diff = 0x1ff;  // 511 (9-bit)
}


void PrimitiveMatching::configure(
    int verbose, int endcap, int sector, int bx,
    bool fixZonePhi
) {
  verbose_ = verbose;
  endcap_  = endcap;
  sector_  = sector;
  bx_      = bx;

  fixZonePhi_      = fixZonePhi;
}

void PrimitiveMatching::process(
    const std::deque<EMTFHitCollection>& extended_conv_hits,
    const zone_array<EMTFRoadCollection>& zone_roads,
    zone_array<EMTFTrackCollection>& zone_tracks
) const {
  int num_roads = 0;
  for (const auto& roads : zone_roads)
    num_roads += roads.size();
  bool early_exit = (num_roads == 0);

  if (early_exit)
    return;


  if (verbose_ > 0) {  // debug
    for (const auto& roads : zone_roads) {
      for (const auto& road : roads) {
        std::cout << "pattern on match input: z: " << road.Zone() << " r: " << road.Winner()
            << " ph_num: " << road.Key_zhit() << " ph_q: " << to_hex(road.Quality_code())
            << " ly: " << to_binary(road.Layer_code(), 3) << " str: " << to_binary(road.Straightness(), 3)
            << std::endl;
      }
    }
  }

  // Organize converted hits by (zone, station)
  std::array<EMTFHitCollection, NUM_ZONES*NUM_STATIONS> zs_conv_hits;

  bool use_fs_zone_code = true;  // use zone code as in firmware find_segment module

  std::deque<EMTFHitCollection>::const_iterator ext_conv_hits_it  = extended_conv_hits.begin();
  std::deque<EMTFHitCollection>::const_iterator ext_conv_hits_end = extended_conv_hits.end();

  for (; ext_conv_hits_it != ext_conv_hits_end; ++ext_conv_hits_it) {
    EMTFHitCollection::const_iterator conv_hits_it  = ext_conv_hits_it->begin();
    EMTFHitCollection::const_iterator conv_hits_end = ext_conv_hits_it->end();

    for (; conv_hits_it != conv_hits_end; ++conv_hits_it) {

      // Can we do this at some later stage? Check RPC vs. track timing ... - AWB 18.11.16
      //if (conv_hits_it->Subsystem() == TriggerPrimitive::kRPC && bx_ != conv_hits_it->BX())
      //  continue;  // Only use RPC clusters in the same BX as the track

      int istation = conv_hits_it->Station()-1;
      int zone_code = conv_hits_it->Zone_code();  // decide based on original zone code
      if (use_fs_zone_code)
        zone_code = conv_hits_it->FS_zone_code();  // decide based on new zone code

      // A hit can go into multiple zones
      for (int izone = 0; izone < NUM_ZONES; ++izone) {
        if (zone_roads.at(izone).size() > 0) {

          if (zone_code & (1<<izone)) {
            const int zs = (izone*NUM_STATIONS) + istation;
            zs_conv_hits.at(zs).push_back(*conv_hits_it);

            // Update fs_history encoded in fs_segment
            // This update only goes into the hits associated to a track, it does not affect the original hit collection
            int fs_history = bx_ - (conv_hits_it->BX());  // 0 for current BX, 1 for previous BX, 2 for BX before that
	    int fs_segment = zs_conv_hits.at(zs).back().FS_segment();
	    fs_segment |= ((fs_history & 0x3)<<4);
            zs_conv_hits.at(zs).back().set_fs_segment( fs_segment );

            // Update bt_history encoded in bt_segment
            // This update only goes into the hits associated to a track, it does not affect the original hit collection
            int bt_history = fs_history;
	    int bt_segment = zs_conv_hits.at(zs).back().BT_segment();
	    bt_segment |= ((bt_history & 0x3)<<5);
            zs_conv_hits.at(zs).back().set_bt_segment ( bt_segment );
          }
        }
      }
      
    }  // end loop over conv_hits
  }  // end loop over extended_conv_hits

  if (verbose_ > 1) {  // debug
    for (int izone = 0; izone < NUM_ZONES; ++izone) {
      for (int istation = 0; istation < NUM_STATIONS; ++istation) {
        const int zs = (izone*NUM_STATIONS) + istation;
        for (const auto& conv_hit : zs_conv_hits.at(zs)) {
          std::cout << "z: " << izone << " st: " << istation+1 << " cscid: " << conv_hit.CSC_ID()
              << " ph_zone_phi: " << conv_hit.Zone_hit() << " ph_low_prec: " << (conv_hit.Zone_hit()<<5)
              << " ph_high_prec: " << conv_hit.Phi_fp() << " ph_high_low_diff: " << (conv_hit.Phi_fp() - (conv_hit.Zone_hit()<<5))
              << std::endl;
        }
      }
    }
  }

  // Keep the best phi difference for every road by (zone, station)
  std::array<std::vector<hit_sort_pair_t>, NUM_ZONES*NUM_STATIONS> zs_phi_differences;

  // Get the best-matching hits by comparing phi difference between
  // pattern and segment
  for (int izone = 0; izone < NUM_ZONES; ++izone) {
    for (int istation = 0; istation < NUM_STATIONS; ++istation) {
      const int zs = (izone*NUM_STATIONS) + istation;

      // This leaves zone_roads.at(izone) and zs_conv_hits.at(zs) unchanged
      // zs_phi_differences.at(zs) gets filled with a pair of <phi_diff, conv_hit> for the
      // conv_hit with the lowest phi_diff from the pattern in this station and zone
      process_single_zone_station(
          istation + 1,
          zone_roads.at(izone),
          zs_conv_hits.at(zs),
          zs_phi_differences.at(zs)
      );

      assert(zone_roads.at(izone).size() == zs_phi_differences.at(zs).size());
    }  // end loop over stations
  }  // end loop over zones

  if (verbose_ > 1) {  // debug
    for (int izone = 0; izone < NUM_ZONES; ++izone) {
      const auto& roads = zone_roads.at(izone);
      for (unsigned iroad = 0; iroad < roads.size(); ++iroad) {
        const auto& road = roads.at(iroad);
        for (int istation = 0; istation < NUM_STATIONS; ++istation) {
          const int zs = (izone*NUM_STATIONS) + istation;
          int ph_diff = zs_phi_differences.at(zs).at(iroad).first;
          std::cout << "find seg: z: " << road.Zone() << " r: " << road.Winner()
              << " st: " << istation << " ph_diff: " << ph_diff
              << std::endl;
        }
      }
    }
  }


  // Build all tracks in each zone
  for (int izone = 0; izone < NUM_ZONES; ++izone) {
    const EMTFRoadCollection& roads = zone_roads.at(izone);

    for (unsigned iroad = 0; iroad < roads.size(); ++iroad) {
      const EMTFRoad& road = roads.at(iroad);

      // Create a track
      EMTFTrack track;
      track.set_endcap     ( road.Endcap() );
      track.set_sector     ( road.Sector() );
      track.set_sector_idx ( road.Sector_idx() );
      track.set_bx         ( road.BX() );
      track.set_zone       ( road.Zone() );
      track.set_rank       ( road.Quality_code() );
      track.set_winner     ( road.Winner() );

      track.clear_Hits();

      // Insert hits
      for (int istation = 0; istation < NUM_STATIONS; ++istation) {
        const int zs = (izone*NUM_STATIONS) + istation;

        const EMTFHitCollection& conv_hits = zs_conv_hits.at(zs);
        int       ph_diff      = zs_phi_differences.at(zs).at(iroad).first;
        hit_ptr_t conv_hit_ptr = zs_phi_differences.at(zs).at(iroad).second;

        if (ph_diff != invalid_ph_diff) {
          // Inserts the conv_hit with the lowest phi_diff, as well as its duplicate
          // (same strip and phi, different wire and theta), if a duplicate exists
          insert_hits(conv_hit_ptr, conv_hits, track);
        }
      }

      if (fixZonePhi_) {
        assert(track.Hits().size() > 0);
      }

      // Output track
      zone_tracks.at(izone).push_back(track);

    }  // end loop over roads
  }  // end loop over zones

  if (verbose_ > 0) {  // debug
    for (const auto& tracks : zone_tracks) {
      for (const auto& track : tracks) {
        for (const auto& hit : track.Hits()) {
          std::cout << "match seg: z: " << track.Zone() << " pat: " << track.Winner() <<  " st: " << hit.Station()
              << " vi: " << to_binary(0b1, 2) << " hi: " << ((hit.FS_segment()>>4) & 0x3)
              << " ci: " << ((hit.FS_segment()>>1) & 0x7) << " si: " << (hit.FS_segment() & 0x1)
              << " ph: " << hit.Phi_fp() << " th: " << hit.Theta_fp()
              << std::endl;
        }
      }
    }
  }

}

void PrimitiveMatching::process_single_zone_station(
    int station,
    const EMTFRoadCollection& roads,
    const EMTFHitCollection& conv_hits,
    std::vector<hit_sort_pair_t>& phi_differences
) const {
  // max phi difference between pattern and segment
  // This doesn't depend on the pattern straightness - any hit within the max phi difference may match
  int max_ph_diff = (station == 1) ? 15 : 7;
  //int bw_ph_diff = (station == 1) ? 5 : 4; // ph difference bit width
  //int invalid_ph_diff = (station == 1) ? 31 : 15;  // invalid difference

  if (fixZonePhi_) {
    if (station == 1) {
      max_ph_diff = 496;  // width of pattern in ME1 + rounding error 15*32+16
      //bw_ph_diff = 9;
      //invalid_ph_diff = 0x1ff;
    } else if (station == 2) {
      //max_ph_diff = 16;   // just rounding error for ME2 (pattern must match ME2 hit phi if there was one)
      //max_ph_diff = 32;   // allow neighbor phi bit
      max_ph_diff = 240;  // same as ME3,4
      //bw_ph_diff = 5;
      //invalid_ph_diff = 0x1f;
    } else {
      max_ph_diff = 240;  // width of pattern in ME3,4 + rounding error 7*32+16
      //bw_ph_diff = 8;
      //invalid_ph_diff = 0xff;
    }
  }

  auto abs_diff = [](int a, int b) { return std::abs(a-b); };

  EMTFRoadCollection::const_iterator roads_it  = roads.begin();
  EMTFRoadCollection::const_iterator roads_end = roads.end();

  for (; roads_it != roads_end; ++roads_it) {
    int ph_pat = roads_it->Key_zhit();     // pattern key phi value
    int ph_q   = roads_it->Quality_code(); // pattern quality code
    assert(ph_pat >= 0 && ph_q > 0);

    if (fixZonePhi_) {
      ph_pat <<= 5;  // add missing 5 lower bits to pattern phi
    }

    std::vector<hit_sort_pair_t> tmp_phi_differences;

    EMTFHitCollection::const_iterator conv_hits_it  = conv_hits.begin();
    EMTFHitCollection::const_iterator conv_hits_end = conv_hits.end();

    for (; conv_hits_it != conv_hits_end; ++conv_hits_it) {
      int ph_seg     = conv_hits_it->Phi_fp();     // ph from segments
      int ph_seg_red = ph_seg >> (bw_fph-bpow-1);  // remove unused low bits
      assert(ph_seg >= 0);

      if (fixZonePhi_) {
        ph_seg_red = ph_seg;  // use full-precision phi
      }

      // Get abs phi difference
      int ph_diff = abs_diff(ph_pat, ph_seg_red);
      if (ph_diff > max_ph_diff)
        ph_diff = invalid_ph_diff;  // difference is too high, cannot be the same pattern

      if (ph_diff != invalid_ph_diff)
        tmp_phi_differences.push_back(std::make_pair(ph_diff, conv_hits_it));  // make a key-value pair
    }

    if (!tmp_phi_differences.empty()) {
      // Find best phi difference
      sort_ph_diff(tmp_phi_differences);

      // Store the best phi difference
      phi_differences.push_back(tmp_phi_differences.front());

    } else {
      // No segment found
      phi_differences.push_back(std::make_pair(invalid_ph_diff, conv_hits_end));  // make a key-value pair
    }

  }  // end loop over roads
}

void PrimitiveMatching::sort_ph_diff(
    std::vector<hit_sort_pair_t>& phi_differences
) const {
  // Sort by key, but preserving the original order in case of a tie
  struct {
    typedef hit_sort_pair_t value_type;
    bool operator()(const value_type& lhs, const value_type& rhs) const {
      // If different types, prefer CSC over RPC; else prefer the closer hit in dPhi
      if (lhs.second->Subsystem() != rhs.second->Subsystem())
        return (lhs.second->Subsystem() == TriggerPrimitive::kCSC);
      else
        return std::make_pair(lhs.first, lhs.second->FS_segment()) < std::make_pair(rhs.first, rhs.second->FS_segment());
    }
  } less_ph_diff_cmp;

  std::stable_sort(phi_differences.begin(), phi_differences.end(), less_ph_diff_cmp);
}

void PrimitiveMatching::insert_hits(
    hit_ptr_t conv_hit_ptr, const EMTFHitCollection& conv_hits,
    EMTFTrack& track
) const {
  EMTFHitCollection::const_iterator conv_hits_it  = conv_hits.begin();
  EMTFHitCollection::const_iterator conv_hits_end = conv_hits.end();

  // Find all possible duplicated hits, insert them
  for (; conv_hits_it != conv_hits_end; ++conv_hits_it) {
    const EMTFHit& conv_hit_i = *conv_hits_it;
    const EMTFHit& conv_hit_j = *conv_hit_ptr;

    // All these must match: [bx_history][station][chamber][segment]
    if (
      (conv_hit_i.Subsystem()  == conv_hit_j.Subsystem()) &&
      (conv_hit_i.PC_station() == conv_hit_j.PC_station()) &&
      (conv_hit_i.PC_chamber() == conv_hit_j.PC_chamber()) &&
      (conv_hit_i.Ring()       == conv_hit_j.Ring()) &&  // because of ME1/1
      (conv_hit_i.Strip()      == conv_hit_j.Strip()) &&
      //(conv_hit_i.Wire()       == conv_hit_j.Wire()) &&
      (conv_hit_i.Pattern()    == conv_hit_j.Pattern()) &&
      (conv_hit_i.BX()         == conv_hit_j.BX()) &&
      (conv_hit_i.Strip_low()  == conv_hit_j.Strip_low()) && // For RPC clusters
      (conv_hit_i.Strip_hi()   == conv_hit_j.Strip_hi()) &&  // For RPC clusters
      //(conv_hit_i.Roll()       == conv_hit_j.Roll()) &&
      true
    ) {
      // All duplicates with the same strip but different wire must have same phi_fp
      assert(conv_hit_i.Phi_fp() == conv_hit_j.Phi_fp());

      track.push_Hit( conv_hit_i );
    }
  }

  // Sort by station
  struct {
    typedef EMTFHit value_type;
    bool operator()(const value_type& lhs, const value_type& rhs) const {
      return lhs.Station() < rhs.Station();
    }
  } less_station_cmp;

  EMTFHitCollection tmp_hits = track.Hits();
  std::stable_sort(tmp_hits.begin(), tmp_hits.end(), less_station_cmp);
  track.set_Hits( tmp_hits );
  tmp_hits.clear();

}
