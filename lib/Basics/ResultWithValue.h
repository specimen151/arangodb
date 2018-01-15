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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_WITH_VALUE_H
#define ARANGODB_BASICS_RESULT_WITH_VALUE_H 1

#include "Basics/Result.h"

namespace arangodb {
template <typename T>
class ResultWithValue : public Result {
 public:
  // copy value

  explicit ResultWithValue(T const& value) : Result(), _value{value} {}

  explicit ResultWithValue(T const& value, int errorNumber)
      : Result(errorNumber), _value{value} {}

  explicit ResultWithValue(T const& value, int errorNumber,
                           std::string const& errorMessage)
      : Result(errorNumber, errorMessage), _value{value} {}

  explicit ResultWithValue(T const& value, int errorNumber,
                           std::string&& errorMessage)
      : Result(errorNumber, std::move(errorMessage)), _value{value} {}

  ResultWithValue& reset(T const& value, int errorNumber = TRI_ERROR_NO_ERROR) {
    Result::reset(errorNumber);
      _value = value;
      return *this;
    }

    ResultWithValue& reset(T const& value, int errorNumber, std::string const& errorMessage) {
      Result::reset(errorNumber, errorMessage);
      _value = value;
      return *this;
    }

    ResultWithValue& reset(T const& value, int errorNumber, std::string&& errorMessage) noexcept {
      Result::reset(errorNumber, std::move(errorMessage));
      _value = value;
      return *this;
    }

  // move value

  explicit ResultWithValue(T&& value) : Result(), _value{std::move(value)} {}

  explicit ResultWithValue(T&& value, int errorNumber)
      : Result(errorNumber), _value{std::move(value)} {}

  explicit ResultWithValue(T&& value, int errorNumber,
                           std::string const& errorMessage)
      : Result(errorNumber, errorMessage), _value{std::move(value)} {}

  explicit ResultWithValue(T&& value, int errorNumber,
                           std::string&& errorMessage)
      : Result(errorNumber, std::move(errorMessage)),
        _value{std::move(value)} {}

  virtual ~ResultWithValue() {}

  ResultWithValue& reset(T&& value, int errorNumber = TRI_ERROR_NO_ERROR) {
    Result::reset(errorNumber);
    _value = std::move(value);
    return *this;
  }

      ResultWithValue& reset(T&& value, int errorNumber, std::string const& errorMessage) {
        Result::reset(errorNumber, errorMessage);
        _value = value;
        return *this;
      }

      ResultWithValue& reset(T&& value, int errorNumber, std::string&& errorMessage) noexcept {
        Result::reset(errorNumber, std::move(errorMessage));
        _value = value;
        return *this;
      }

};
}  // namespace arangodb

#endif
