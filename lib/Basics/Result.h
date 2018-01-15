////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_H
#define ARANGODB_BASICS_RESULT_H 1

#include "Basics/Common.h"

#include <boost/any.hpp>

namespace arangodb {
class Result {
 public:
  Result() : _errorNumber{TRI_ERROR_NO_ERROR}, _errorMessage{}, _value{} {}

  Result(int errorNumber)
      : _errorNumber{errorNumber},
        _errorMessage{(TRI_ERROR_NO_ERROR == _errorNumber)
                          ? ""
                          : TRI_errno_string(errorNumber)},
        _value{} {}

  Result(int errorNumber, std::string const& errorMessage)
      : _errorNumber{errorNumber}, _errorMessage{errorMessage}, _value{} {}

  Result(int errorNumber, std::string&& errorMessage)
      : _errorNumber{errorNumber},
        _errorMessage{std::move(errorMessage)},
        _value{} {}

  // copy
  Result(Result const& other)
      : _errorNumber{other._errorNumber},
        _errorMessage{other._errorMessage},
        _value{other._value} {}

  Result& operator=(Result const& other) {
    _errorNumber = other._errorNumber;
    _errorMessage = other._errorMessage;
    _value = other._value;
    return *this;
  }

  // move
  Result(Result&& other) noexcept
      : _errorNumber{other._errorNumber},
        _errorMessage{std::move(other._errorMessage)},
        _value{std::move(other._value)} {}

  Result& operator=(Result&& other) noexcept {
    _errorNumber = other._errorNumber;
    _errorMessage = std::move(other._errorMessage);
    _value = std::move(other._value);
    return *this;
  }

  virtual ~Result() {}

 public:
  bool ok() const { return _errorNumber == TRI_ERROR_NO_ERROR; }
  bool fail() const { return !ok(); }

  int errorNumber() const { return _errorNumber; }
  bool is(int errorNumber) const { return _errorNumber == errorNumber; }
  bool isNot(int errorNumber) const { return !is(errorNumber); }

  Result& reset(int errorNumber = TRI_ERROR_NO_ERROR) {
    _errorNumber = errorNumber;

    if (errorNumber != TRI_ERROR_NO_ERROR) {
      _errorMessage = TRI_errno_string(errorNumber);
    } else {
      _errorMessage.clear();
    }

    _value = boost::any();

    return *this;
  }

  Result& reset(int errorNumber, std::string const& errorMessage) {
    _errorNumber = errorNumber;
    _errorMessage = errorMessage;
    _value = boost::any();
    return *this;
  }

  Result& reset(int errorNumber, std::string&& errorMessage) noexcept {
    _errorNumber = errorNumber;
    _errorMessage = std::move(errorMessage);
    _value = boost::any();
    return *this;
  }

  Result& reset(Result const& other) {
    _errorNumber = other._errorNumber;
    _errorMessage = other._errorMessage;
    _value = other._value;
    return *this;
  }

  Result& reset(Result&& other) noexcept {
    _errorNumber = other._errorNumber;
    _errorMessage = std::move(other._errorMessage);
    _value = std::move(other._value);
    return *this;
  }

  // the default implementation is const, but sub-classes might
  // really do more work to compute.

  virtual std::string errorMessage() const& { return _errorMessage; }
  virtual std::string errorMessage() && { return std::move(_errorMessage); }

  template <typename T>
  bool hasValue() const noexcept {
    return !_value.empty() && (typeid(T) != _value.type());
  }

  template <typename T>
  T value() const noexcept(hasValue<T>()) {
    return boost::any_cast<T>(_value);
  }

  template <typename T>
  T const& value() const noexcept(hasValue<T>()) {
    return boost::any_cast<T const&>(_value);
  }

  template <typename T>
  T&& value() const noexcept(hasValue<T>()) {
    return boost::any_cast<T&&>(std::move(_value));
  }

 protected:
  int _errorNumber;
  std::string _errorMessage;
  boost::any _value;
};
}  // namespace arangodb

#endif
