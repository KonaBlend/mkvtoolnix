/*
   mkvtoolnix - programs for manipulating Matroska files
   Copyright © 2003…2016 Moritz Bunkus

   This file is distributed as part of mkvtoolnix and is distributed under the
   GNU General Public License Version 2, or (at your option) any later version.
   See accompanying file COPYING for details or visit
   http://www.gnu.org/licenses/gpl.html .

   DTS XLL decode support

   Authors: KonaBlend <kona8lend@gmail.com>
*/

#include "common/common_pch.h"
#include "common/bswap.h"
#include "common/checksums/crc.h"
#include "common/dts.h"
#include "common/dts_xll.h"
#include "common/format.h"
#include "common/sbrm.h"

#define dformat(...) mxdebug(format::format(__VA_ARGS__))
#define dfield(...)  mxdebug(table.field(__VA_ARGS__))

namespace mtx { namespace dts { namespace {

xll_header_common_cptr      decode_xll_header_common(bit_reader_c &,
                                                     header_t::substream_asset_t const &);
xll_header_channel_set_cptr decode_xll_header_channel_set(bit_reader_c &,
                                                     header_t::substream_asset_t const &,
                                                     xll_header_t const &);
xll_header_navi_cptr        decode_xll_header_navi(bit_reader_c &,
                                                   xll_header_t const &);
xll_header_x_cptr           decode_xll_header_x(bit_reader_c &);

static debugging_option_c s_debug{"dts_xll"};

/******************************************************************************
 *
 * Calculate 16-bit CRC-CCITT checksum with bit_reader.
 *
 * If restore_bit_position is true, position will be restored. Otherwise,
 * position will be immediately after checksum data.
 */
uint16_t
bc_crc16(bit_reader_c &bc,
         int data_offset,
         int data_size,
         uint16_t &crc16,
         bool restore_bit_position) {
  auto const saved_bit_pos = bc.get_bit_position();
  auto buf = memory_c::alloc(data_size);
  bc.set_bit_position(data_offset*8);
  bc.get_bytes(buf->get_buffer(), buf->get_size());
  crc16 = bc.get_bits(16);
  auto ret_crc16 = mtx::bswap_16(mtx::checksum::calculate_as_uint(mtx::checksum::algorithm_e::crc16_ccitt,
                                                                  buf->get_buffer(),
                                                                  buf->get_size(),
                                                                  0xffff));
  if (restore_bit_position)
    bc.set_bit_position(saved_bit_pos);
  return ret_crc16;
}

} // namespace (anonymous)

/*****************************************************************************/

xll_header_channel_set_t::downmix_table_t xll_header_channel_set_t::s_downmix_table[8] = {
  { 1, YT("1/0") },
  { 2, YT("Lo/Ro") },
  { 2, YT("Lt/Rt") },
  { 3, YT("3/0") },
  { 3, YT("2/1") },
  { 4, YT("2/2") },
  { 4, YT("3/1") },
  { 0, YT("unused") },
};

/*****************************************************************************/

xll_header_cptr
decode_xll_header(bit_reader_c &bc, header_t::substream_asset_t const &asset) {
  bc.set_bit_position((asset.xll_offset + asset.xll_sync_offset) * 8);
  sbrm::result_guard_c<xll_header_t> h([&, pos=bc.get_bit_position()](bool){
    if (h.success)
      return;
    bc.set_bit_position(pos);
    h.result = nullptr;
  });
  h.make_result();

  // when sync not present simply return success with empty result
  if (!asset.xll_sync_present)
    return h(true);

  h->common = decode_xll_header_common(bc, asset);
  if (!h->common)
    return h();

  for (int i = 0; i < h->common->num_channel_sets; ++i) {
    auto cs = decode_xll_header_channel_set(bc, asset, **h);
    if (!cs)
      return h();
    h->channel_sets.push_back(cs);
  }

  h->navi = decode_xll_header_navi(bc, **h);
  if (!h->navi)
    return h();

  // skip band data
  bc.skip_bits(h->navi->total_band_data_size*8);

  // presence is optional
  h->dtsx = decode_xll_header_x(bc);

  if (s_debug)
    h->dump(asset);

  return h(true);
}

/*****************************************************************************/

namespace {

xll_header_common_cptr
decode_xll_header_common(bit_reader_c &bc, header_t::substream_asset_t const &asset) {
  auto const begin_bit_pos = bc.get_bit_position();
  sbrm::result_guard_c<xll_header_common_t> h([&](bool){
    if (h.success)
      return;
    bc.set_bit_position(begin_bit_pos);
    h.result = nullptr;
  });
  h.make_result();

  h->sync_word = bc.get_bits(32);
  if (h->sync_word != static_cast<uint32_t>(sync_word_e::xll))
    return h();

  h->version = bc.get_bits(4) + 1;
  h->header_size = bc.get_bits(8) + 1;

  auto crc16_tmp = bc_crc16(bc, (begin_bit_pos + 32)/8, h->header_size - 6, h->crc16, true);
  h->crc16_valid = crc16_tmp == h->crc16;
  if (!h->crc16_valid)
    return h();

  h->frame_size_bits = bc.get_bits(5) + 1;
  h->frame_size = bc.get_bits(h->frame_size_bits) + 1;
  h->num_channel_sets = bc.get_bits(4) + 1;
  h->num_segments = 1 << bc.get_bits(4);
  h->samples_per_segment = 1 << bc.get_bits(4);
  h->navi_data_size_bits = bc.get_bits(5) + 1;
  h->band_data_crc = bc.get_bits(2);
  h->scalable_lsb = bc.get_bits(1);
  h->channel_mask_bits = bc.get_bits(5) + 1;
  if (h->scalable_lsb)
    h->num_fixed_lsb_width = bc.get_bits(4);

  bc.set_bit_position(begin_bit_pos + h->header_size*8); // end of common header
  return h(true);
}

/*****************************************************************************/

xll_header_channel_set_cptr
decode_xll_header_channel_set(bit_reader_c &bc,
                              header_t::substream_asset_t const &asset,
                              xll_header_t const &xll) {
  auto const begin_bit_pos = bc.get_bit_position();
  sbrm::result_guard_c<xll_header_channel_set_t> h([&](bool){
    if (h.success)
      return;
    bc.set_bit_position(begin_bit_pos);
    h.result = nullptr;
  });
  h.make_result();

  h->header_size = bc.get_bits(10) + 1;

  auto crc16_tmp = bc_crc16(bc, begin_bit_pos/8, h->header_size - 2, h->crc16, true);
  h->crc16_valid = crc16_tmp == h->crc16;
  if (!h->crc16_valid)
    return h();

  h->num_channels = bc.get_bits(4) + 1;
  h->residual_channel_encode = bc.get_bits(h->num_channels);
  h->bit_resolution = bc.get_bits(5) + 1;
  h->bit_width = bc.get_bits(5) + 1;
  h->frequency_index = bc.get_bits(4);

  static uint32_t const s_frequency_table[16] = {
    8000, 16000, 32000, 64000, 128000,
    22050, 44100, 88200, 176400, 352800,
    12000, 24000, 48000, 96000, 192000, 384000,
  };
  h->frequency = s_frequency_table[h->frequency_index];

  h->ifactor_index = bc.get_bits(2);

  static uint8_t const s_ifactor_table[4] = { 1, 2, 4, 8 };
  h->interpolation_factor = s_ifactor_table[h->ifactor_index];

  h->replacement_set = bc.get_bits(2);

  if (h->replacement_set != 0)
    h->active_replace_set = bc.get_bits(1) == 1;

  if (asset.one_to_one_map_channel_to_speaker) {
    h->primary_channel_set = bc.get_bits(1) == 1;

    h->downmix_coeffs_present = bc.get_bits(1) == 1;
    if (h->downmix_coeffs_present) {
      h->downmix_embedded = bc.get_bits(1) == 1;
      if (h->primary_channel_set)
        h->downmix_type = bc.get_bits(3);
    }

    h->hierarchical_channel_set = bc.get_bits(1) == 1;

    if (h->downmix_coeffs_present) {
      int n;
      int m;
      if (h->primary_channel_set) {
        n = h->num_channels;
        m = xll_header_channel_set_t::s_downmix_table[h->downmix_type].nchan;
      } else {
        n = h->num_channels + 1;
        m = 0;
        for (auto cs : xll.channel_sets) {
          if (cs->hierarchical_channel_set)
            m += cs->num_channels;
        }
      }
      bc.skip_bits(n*m*9);
    }

    h->channel_mask_enabled = bc.get_bits(1) == 1;
    if (h->channel_mask_enabled) {
      h->channel_mask = bc.get_bits(xll.common->channel_mask_bits);
    } else {
      for (int i = 0; i < h->num_channels; ++i) {
        bc.skip_bits(9);
        bc.skip_bits(9);
        bc.skip_bits(7);
      }
    }
  } else {
    h->primary_channel_set = true;
    h->downmix_coeffs_present = false;
    h->hierarchical_channel_set = true;

    h->mapping_coefficient_present = bc.get_bits(1) == 1;
    if (h->mapping_coefficient_present) {
      h->chan_to_speaker_mapping_coeff_bits = 6 + 2 * bc.get_bits(3);

      uint8_t const num_loudspeaker_configs = bc.get_bits(2) + 1;
      h->loudspeaker_configs.resize(num_loudspeaker_configs);
      for (auto &lc : h->loudspeaker_configs) {
        lc.active_channel_mask = bc.get_bits(h->num_channels);
        lc.num_speakers = bc.get_bits(6) + 1;
        lc.speaker_mask_enabled = bc.get_bits(1) == 1;
        if (lc.speaker_mask_enabled)
          lc.speaker_mask = bc.get_bits(xll.common->channel_mask_bits);

        for (auto speaker_idx = 0; speaker_idx < num_loudspeaker_configs; ++speaker_idx) {
          if (!lc.speaker_mask_enabled)
            bc.skip_bits(25);
          for (auto channel_idx = 0; channel_idx < h->num_channels; ++channel_idx) {
            if (lc.active_channel_mask & (1 << channel_idx))
              bc.skip_bits(h->chan_to_speaker_mapping_coeff_bits);
          }
        }
      }
    }
  }

  if (h->frequency > 96000) {
    h->extra_frequency_bands = bc.get_bits(1) == 1;

    if (h->extra_frequency_bands)
      h->num_frequency_bands = 4;
    else
      h->num_frequency_bands = 2;
  } else {
    h->num_frequency_bands = 1;
  }

  bc.set_bit_position(begin_bit_pos + h->header_size*8); // end of channel-set header
  return h(true);
}

/*****************************************************************************/

xll_header_navi_cptr
decode_xll_header_navi(bit_reader_c &bc,
                       xll_header_t const &xll) {
  auto const begin_bit_pos = bc.get_bit_position();
  sbrm::result_guard_c<xll_header_navi_t> h([&](bool){
    if (h.success)
      return;
    bc.set_bit_position(begin_bit_pos);
    h.result = nullptr;
  });
  h.make_result();

  int num_bands = 0;
  for (auto cs : xll.channel_sets) {
    if (cs->num_frequency_bands > num_bands)
      num_bands = cs->num_frequency_bands;
  }

  h->total_band_data_size = 0;
  for (int band = 0; band < num_bands; ++band) {
    for (int seg = 0; seg < xll.common->num_segments; ++seg) {
      int csi = 0;
      for (auto cs : xll.channel_sets) {
        if (cs->num_frequency_bands <= band)
          continue;
        auto frame_size = bc.get_bits(xll.common->navi_data_size_bits) + 1;
        h->total_band_data_size += frame_size;
        csi++;
      }
    }
  }

  bc.byte_align();
  auto crc16_tmp = bc_crc16(bc, begin_bit_pos/8, (bc.get_bit_position() - begin_bit_pos)/8, h->crc16, false);
  h->crc16_valid = crc16_tmp == h->crc16;
  if (!h->crc16_valid)
    return h();

  return h(true);
}

/*****************************************************************************/

xll_header_x_cptr
decode_xll_header_x(bit_reader_c &bc) {
  sbrm::result_guard_c<xll_header_x_t> h([&, pos=bc.get_bit_position()](bool){
    if (h.success)
      return;
    bc.set_bit_position(pos);
    h.result = nullptr;
  });
  h.make_result();

  // align at 4-byte
  if (!bc.bit_align(32))
    return h();

  if (bc.get_remaining_bits() < 32)
    return h();

  h->sync_word = bc.get_bits(32);
  if (h->sync_word != static_cast<uint32_t>(sync_word_e::x))
    return h();

  return h(true);
}

} // namespace (anonymous)

/*****************************************************************************/

void
xll_header_t::dump(header_t::substream_asset_t const &asset) const {
  dformat("DTS XLL HEADER\n");
  dformat("  COMMON\n");
  common->dump();

  uint8_t csi{};
  for (auto &cs : channel_sets) {
    dformat("  CHANNEL SET[%1%]\n", csi);
    cs->dump(asset);
    ++csi;
  }

  dformat("  NAVI\n");
  navi->dump();

  if (dtsx) {
    dformat("  DTS:X\n");
    dtsx->dump();
  }
}

void
xll_header_common_t::dump() const {
  format::table_t table{4, 19};
  dfield("sync_word", "%1$#010x", sync_word);
  dfield("version", "%1%", version);
  dfield("header_size", "%1%", header_size);
  dfield("frame_size_bits", "%1%", frame_size_bits);
  dfield("frame_size", "%1%", frame_size);
  dfield("num_channel_sets", "%1%", num_channel_sets);
  dfield("num_segments", "%1%", num_segments);
  dfield("samples_per_segment", "%1%", samples_per_segment);
  dfield("navi_data_size_bits", "%1%", navi_data_size_bits);
  dfield("band_data_crc", "%1%", band_data_crc);
  dfield("scalable_lsb", "%1%", scalable_lsb);
  dfield("channel_mask_bits", "%1%", channel_mask_bits);
  if (scalable_lsb)
    dfield("num_fixed_lsb_width", "%1%", num_fixed_lsb_width);
  dfield("crc16", "%1$#06x (%2%)", crc16, (crc16_valid ? "valid" : "invalid"));
}

void
xll_header_channel_set_t::dump(header_t::substream_asset_t const &asset) const {
  format::table_t table{4, 35};
  dfield("header_size", "%1%", header_size);
  dfield("num_channels", "%1%", num_channels);
  dfield("residual_channel_encode", "%1$#06x", residual_channel_encode);
  dfield("bit_resolution", "%1%", bit_resolution);
  dfield("bit_width", "%1%", bit_width);
  dfield("frequency_index", "%1%", frequency_index);
  dfield("frequency", "%1%", frequency);
  dfield("ifactor_index", "%1%", ifactor_index);
  dfield("interpolation_factor", "%1%", interpolation_factor);
  dfield("replacement_set", "%1%", replacement_set);
  if (replacement_set != 0)
    dfield("active_replace_set", "%1%", active_replace_set);

  dfield("(one_to_one_map_channel_to_speaker)", "%1%", asset.one_to_one_map_channel_to_speaker);
  if (asset.one_to_one_map_channel_to_speaker) {
    dfield("primary_channel_set", "%1%", primary_channel_set);
    dfield("downmix_coeffs_present", "%1%", downmix_coeffs_present);
    if (downmix_coeffs_present) {
      dfield("downmix_embedded", "%1%", downmix_embedded);
      if (primary_channel_set)
        dfield("downmix_type", "%1% (%2%)", downmix_type, s_downmix_table[downmix_type].descr);
    }
    dfield("hierarchical_channel_set", "%1%", hierarchical_channel_set);
    dfield("channel_mask_enabled", "%1%", channel_mask_enabled);
    if (channel_mask_enabled)
      dfield("channel_mask", "%1$#06x", channel_mask);
  } else {
    dfield("primary_channel_set", "%1%", primary_channel_set);
    dfield("downmix_coeffs_present", "%1%", downmix_coeffs_present);
    dfield("hierarchical_channel_set", "%1%", hierarchical_channel_set);
    dfield("mapping_coefficient_present", "%1%", mapping_coefficient_present);
    if (mapping_coefficient_present) {
      dfield("chan_to_speaker_mapping_coeff_bits", "%1$#04x", chan_to_speaker_mapping_coeff_bits);
      dfield("num_loudspeaker_configs", "%1%", loudspeaker_configs.size());
      uint8_t lci{};
      table.indent += 2;
      for (auto &lc : loudspeaker_configs) {
        dformat(    "loudspeaker_config[%1%]\n", lci);
        dfield("active_channel_mask", "%1$#06x", lc.active_channel_mask);
        dfield("num_speakers", "%1%", lc.num_speakers);
        dfield("speaker_mask_enabled", "%1%", lc.speaker_mask_enabled);
        if (lc.speaker_mask_enabled)
          dfield("speaker_mask", "%1$#08x", lc.speaker_mask);
      }
      table.indent -= 2;
    }
  }

  dfield("extra_frequency_bands", "%1%", extra_frequency_bands);
  dfield("num_frequency_bands", "%1%", num_frequency_bands);
  dfield("crc16", "%1$#06x (%2%)", crc16, (crc16_valid ? "valid" : "invalid"));
}

void
xll_header_navi_t::dump() const {
  format::table_t table{4, 21};
  dfield("total_band_data_size", "%1%", total_band_data_size);
  dfield("crc16", "%1$#04x (%2%)", crc16, (crc16_valid ? "valid" : "invalid"));
}

void
xll_header_x_t::dump() const {
  format::table_t table{4, 0};
  dfield("sync_word", "%1$#010x", sync_word);
}

}} // namespace mtx::dts
