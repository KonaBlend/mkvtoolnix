/*
   mkvtoolnix - programs for manipulating Matroska files
   Copyright © 2003…2016 Moritz Bunkus

   This file is distributed as part of mkvtoolnix and is distributed under the
   GNU General Public License Version 2, or (at your option) any later version.
   See accompanying file COPYING for details or visit
   http://www.gnu.org/licenses/gpl.html .

   Scope-Bound Resource Management (SBRM) utilities.

   Authors: KonaBlend <kona8lend@gmail.com>
*/

#ifndef MTX_SBRM_H
#define MTX_SBRM_H

namespace mtx { namespace sbrm {

/******************************************************************************
 *
 * result_guard_c is a scope-bound resource manager geared for early-exit
 * programming style. shared_ptr<T> becomes the result_t .
 *
 ******************************************************************************
 *
 * Idiomatic usage:
 *
 *      |  {
 *    a |     result_guard_c<Foo> h([&](bool) {
 *    b |       if (!h.success)
 *    c |         h.result = nullptr;
 *      |     });
 *    d |     h.make_result();
 *      |
 *    e |     h->one = "hello";
 *    i |     ...
 *      |
 *      |
 *      |    if (!other_good_condition)
 *    f |      return h();
 *      |
 *      |    h->two = "world";
 *      |    ...
 *      |
 *    g |    return h(true);
 *      | }
 *
 * a) create a guard on the stack for a new heap object
 *    - h.result is a shared_ptr<Foo> default initialized (empty)
 *    - h.success is a bool default initialized (false)
 *    - h.callback is a void(bool) initialized to lambda body LINES(a..c)
 *
 *    * note: we avoid using keyword auto and assignment initialization
 *      because that removes the ability to reference new variable in
 *      lambda body; i.e. use of auto would emphasize the variable name but
 *      fail compiling:
 *
 *      |  auto h = result_guard_c<Foo>([&](bool) {    // BAD
 *      |    if (!h.success)                           // h not available
 *      |      h.result = nullptr;
 *      |  });
 *
 * b) callback checks if h was never marked as successful
 *
 * c) callback empties result, because in this idiom the goal is to return
 *    empty on failure; i.e. do not let a partially populated Foo escape.
 *
 * d) set result to a new heap default-constructed Foo; equivalent to:
 *
 *      |  h.result = std::make_shared<Foo>();
 *
 * e) use class-member-access-operator to populate result; equivalent to:
 *
 *      |  h.result->one = "hello";
 *
 * f) fail by invoking function-call-operator for return.
 *
 * g) use function-call-operator to mark as successful and return;
 *    equivalent to:
 *
 *      |  h.success = true;
 *      |  return h.result;
 *
 ******************************************************************************
 *
 * The key public data members are exposed for maximum flexibility falling
 * outside idiomatic usage:
 *
 *     success: indicates if function was successful or not. default=false.
 *     result: tracks a shared_ptr result.
 *     callback: invoked upon cleanups of automatics at end-of-scope.
 *
 *
 * The callback_t is a function void(bool explicit_) where explicit_ indicates
 * whether or not the callback was initiated by function-call-operator or
 * by scope-cleanup.
 *
 * In most cases explicit_ may be ignored if the callback operations only
 * involve modifying/clearing result, as result itself (if any) will simply
 * be deleted by shared_ptr<T>.
 *
 * Undefined behaviour results if callback throws exception.
 *
 ******************************************************************************
 */
template<typename T>
class result_guard_c {
  result_guard_c(const result_guard_c &) = delete;
  result_guard_c(result_guard_c &&) = delete;
  result_guard_c &operator=(const result_guard_c &) = delete;
  result_guard_c &operator=(result_guard_c &&) = delete;

public:
  using result_t   = std::shared_ptr<T>;
  using callback_t = std::function<void(bool)>;

  result_guard_c(callback_t callback_ = {})
    : callback(callback_) { }

  ~result_guard_c() {
    invoke_callback(false);
  }

  result_t
  operator ()() {
    invoke_callback(true);
    return this->result;
  }

  result_t
  operator ()(bool success_) {
    this->success = success_;
    invoke_callback(true);
    return this->result;
  }

  T*
  operator ->() {
    return this->result.get();
  }

  T*
  operator *() {
    return this->result.get();
  }

  void
  make_result() {
    this->result = std::make_shared<T>();
  }

  template<typename... Args>
  void
  make_result(Args&&... args) {
    this->result = std::make_shared<T>(std::forward<Args>(args)...);
  }

  bool       success{};
  result_t   result{};
  callback_t callback{};

private:
  void
  invoke_callback(bool explicit_) {
    if (!this->callback)
      return;
    this->callback(explicit_);
    this->callback = nullptr;
  }
};

}} // namespace mtx::sbrm

#endif // MTX_SBRM_H
