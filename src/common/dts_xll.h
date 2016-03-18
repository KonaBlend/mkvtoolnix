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

#ifndef MTX_COMMON_DTS_XLL_H
#define MTX_COMMON_DTS_XLL_H

#include "common/common_pch.h"
#include "common/bit_cursor.h"
#include "common/translation.h"

namespace mtx { namespace dts {

struct xll_header_t;
struct xll_header_common_t;
struct xll_header_channel_set_t;
struct xll_header_navi_t;
struct xll_header_x_t;

using xll_header_cptr             = std::shared_ptr<xll_header_t>;
using xll_header_common_cptr      = std::shared_ptr<xll_header_common_t>;
using xll_header_channel_set_cptr = std::shared_ptr<xll_header_channel_set_t>;
using xll_header_navi_cptr        = std::shared_ptr<xll_header_navi_t>;
using xll_header_x_cptr           = std::shared_ptr<xll_header_x_t>;

/******************************************************************************
 *
 * Decode XLL header.
 *
 * Decode XLL header returns a stored xll_header_t on success and nullptr on
 * failure.
 *
 * xll_header->dtsx is true or false depending if DTS:X was detected
 */
xll_header_cptr decode_xll_header(bit_reader_c &, header_t::substream_asset_t const &);

struct xll_header_t {
  xll_header_t(const xll_header_t &) = delete;
  xll_header_t(xll_header_t &&) = delete;
  xll_header_t &operator=(const xll_header_t &) = delete;
  xll_header_t &operator=(xll_header_t &&) = delete;

  using channel_sets_t = std::vector<xll_header_channel_set_cptr>;

  xll_header_t() = default;

  void dump(header_t::substream_asset_t const &) const;

  xll_header_common_cptr common{};
  channel_sets_t         channel_sets{};
  xll_header_navi_cptr   navi{};
  xll_header_x_cptr      dtsx{};
};

struct xll_header_common_t {
  xll_header_common_t(const xll_header_common_t &) = delete;
  xll_header_common_t(xll_header_common_t &&) = delete;
  xll_header_common_t &operator=(const xll_header_common_t &) = delete;
  xll_header_common_t &operator=(xll_header_common_t &&) = delete;

  xll_header_common_t() = default;

  void dump() const;

  uint32_t sync_word{};             // SYNCXLL
  uint8_t  version{};               // nVersion
  uint16_t header_size{};           // nHeaderSize
  uint8_t  frame_size_bits{};       // nBits4FrameFsize
  uint32_t frame_size{};            // nLLFrameSize
  uint8_t  num_channel_sets{};      // numChSetsInFrame
  uint8_t  num_segments{};          // nSegmentsInFrame
  uint32_t samples_per_segment{};   // nSmplInSeg
  uint8_t  navi_data_size_bits{};   // nBits4SSize
  uint8_t  band_data_crc{};         // nBandDataCRCEn
  bool     scalable_lsb{};          // bScalableLSBs
  uint8_t  channel_mask_bits{};     // nBits4ChMask
  uint8_t  num_fixed_lsb_width{};   // nuFixedLSBWidth
  uint16_t crc16{};                 // nCRC16Header
  bool     crc16_valid{};
};

struct xll_header_channel_set_t {
  xll_header_channel_set_t(const xll_header_channel_set_t &) = delete;
  xll_header_channel_set_t(xll_header_channel_set_t &&) = delete;
  xll_header_channel_set_t &operator=(const xll_header_channel_set_t &) = delete;
  xll_header_channel_set_t &operator=(xll_header_channel_set_t &&) = delete;

  xll_header_channel_set_t() = default;

  void dump(header_t::substream_asset_t const &) const;

  uint16_t  header_size{};                          // m_nChSetHeaderSize
  uint8_t   num_channels{};                         // m_nChSetLLChannel
  uint16_t  residual_channel_encode{};              // m_nResidualChEncode
  uint8_t   bit_resolution{};                       // m_nBitResolution
  uint8_t   bit_width{};                            // m_nBitWidth
  uint8_t   frequency_index{};                      // sFreqIndex
  uint32_t  frequency{};                            // m_nFs
  uint8_t   ifactor_index{};                        // nFsInterpolate
  uint8_t   interpolation_factor{};
  uint8_t   replacement_set{};                      // NReplacementSet
  bool      active_replace_set{};                   // bActiveReplaceSet

  bool      primary_channel_set{};                  // bPrimaryChSet
  bool      downmix_coeffs_present{};               // bDownmixCoeffCodeEmbedded
  bool      downmix_embedded{};                     // bDownmixEmbedded
  uint8_t   downmix_type{};                         // nLLDownmixType
  bool      hierarchical_channel_set{};             // BHierChSet
  bool      channel_mask_enabled{};                 // bChMaskEnabled
  uint32_t  channel_mask{};                         // nChMask

  bool      mapping_coefficient_present{};          // bMappingCoeffsPresent
  uint8_t   chan_to_speaker_mapping_coeff_bits{};   // nBitsCh2SpkrCoef

  struct loudspeaker_config_t {
    uint32_t active_channel_mask{};     // pnActiveChannelMask
    uint8_t  num_speakers{};            // pnNumSpeakers
    bool     speaker_mask_enabled{};    // bSpkrMaskEnabled
    uint32_t speaker_mask{};            // nSpkrMask
  };
  using loudspeaker_configs_t = std::vector<loudspeaker_config_t>;

  loudspeaker_configs_t loudspeaker_configs;        // nNumSpeakerConfigs

  bool      extra_frequency_bands{};                // bXtraFreqBands
  uint8_t   num_frequency_bands{};                  // m_nNumFreqBands
  uint16_t  crc16{};                                // nCRC16SubHeader
  bool      crc16_valid{};

  struct downmix_table_t {
    uint8_t nchan;                  // c++14 TODO: add default member initializer
    translatable_string_c descr;    // c++14 TODO: add default member initializer
  };

  static downmix_table_t s_downmix_table[8];
};

struct xll_header_navi_t {
  xll_header_navi_t(const xll_header_navi_t &) = delete;
  xll_header_navi_t(xll_header_navi_t &&) = delete;
  xll_header_navi_t &operator=(const xll_header_navi_t &) = delete;
  xll_header_navi_t &operator=(xll_header_navi_t &&) = delete;

  xll_header_navi_t() = default;

  void dump() const;

  uint64_t total_band_data_size{};
  uint16_t crc16{};
  bool     crc16_valid{};
};

struct xll_header_x_t {
  xll_header_x_t(const xll_header_x_t &) = delete;
  xll_header_x_t(xll_header_x_t &&) = delete;
  xll_header_x_t &operator=(const xll_header_x_t &) = delete;
  xll_header_x_t &operator=(xll_header_x_t &&) = delete;

  xll_header_x_t() = default;

  void dump() const;

  uint32_t sync_word{};
};

}} // namespace mtx::dts

#endif // MTX_COMMON_DTS_XLL_H
