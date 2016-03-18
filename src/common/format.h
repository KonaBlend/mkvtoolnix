/*
   mkvtoolnix - programs for manipulating Matroska files
   Copyright © 2003…2016 Moritz Bunkus

   This file is distributed as part of mkvtoolnix and is distributed under the
   GNU General Public License Version 2, or (at your option) any later version.
   See accompanying file COPYING for details or visit
   http://www.gnu.org/licenses/gpl.html .

   boost::format utilities.

   Authors: KonaBlend <kona8lend@gmail.com>
*/

#ifndef MTX_FORMAT_H
#define MTX_FORMAT_H

namespace mtx { namespace format {

/******************************************************************************
 *
 * Debug/boost::format helper routines.
 *
 * - more terse
 * - move from operator% to , separated args
 * - use int8_t/uint8_t as integers
 *
 * Consider simplifying with fold-expressions when C++17 is engaged.
 */

void format_each(boost::format &);

template<typename T>
struct as_raw {
  T value{};
};

template<typename Arg0, typename... Args>
void
format_each(boost::format &bf, Arg0 const &arg0, Args... args) {
  bf % arg0;
  format_each(bf, args...);
}

template<typename ART, typename... Args>
void
format_each(boost::format &bf, as_raw<ART> const &arg0, Args... args) {
  bf % arg0.value;
  format_each(bf, args...);
}

template<typename... Args>
void
format_each(boost::format &bf, bool const &arg0, Args... args) {
  bf % (arg0 ? "true" : "false");
  format_each(bf, args...);
}

template<typename... Args>
void
format_each(boost::format &bf, int8_t const &arg0, Args... args) {
  bf % static_cast<unsigned>(arg0);
  format_each(bf, args...);
}

template<typename... Args>
void
format_each(boost::format &bf, uint8_t const &arg0, Args... args) {
  bf % static_cast<signed>(arg0);
  format_each(bf, args...);
}

template<typename... Args>
boost::format
format(const char *spec, Args... args) try {
  boost::format bf(spec);
  format_each(bf, args...);
  return bf;
} catch (boost::io::format_error &e) {
  return boost::format("%1%\n") % e.what();
}

struct table_t {
  uint8_t indent;           // c++14 TODO: add default member initializer
  uint8_t field_width;      // c++14 TODO: add default member initializer

  template<typename... Args>
  boost::format
  field(std::string name, const char *spec, Args... args) try {
    boost::format bf(spec);
    format_each(bf, args...);
    auto pad = name.length() > field_width ? 0 : field_width - name.length();
    return boost::format("%1%%2%:%3%  %4%\n") % std::string(indent,' ') % name % std::string(pad,' ') % bf.str();
  } catch (boost::io::format_error &e) {
    return boost::format("%1%\n") % e.what();
  }
};

}} // namespace mtx::format

#endif // MTX_FORMAT_H
