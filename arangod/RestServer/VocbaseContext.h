////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_SERVER_VOCBASE_CONTEXT_H
#define ARANGOD_REST_SERVER_VOCBASE_CONTEXT_H 1

#include "Basics/Common.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Rest/RequestContext.h"

struct TRI_server_t;
struct TRI_vocbase_t;

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB VocbaseContext
////////////////////////////////////////////////////////////////////////////////

class VocbaseContext : public arangodb::RequestContext {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief defines a sid
  //////////////////////////////////////////////////////////////////////////////

  static void createSid(std::string const& database, std::string const& sid,
                        std::string const& username);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clears all sid entries for a database
  //////////////////////////////////////////////////////////////////////////////

  static void clearSid(std::string const& database);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clears a sid
  //////////////////////////////////////////////////////////////////////////////

  static void clearSid(std::string const& database, std::string const& sid);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief gets the last access time
  //////////////////////////////////////////////////////////////////////////////

  static double accessSid(std::string const& database, std::string const& sid);

 public:
  VocbaseContext(HttpRequest*, TRI_server_t*, TRI_vocbase_t*);

  ~VocbaseContext();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get vocbase of context
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* getVocbase() const { return _vocbase; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not to use special cluster authentication
  //////////////////////////////////////////////////////////////////////////////

  bool useClusterAuthentication() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return authentication realm
  //////////////////////////////////////////////////////////////////////////////

  std::string realm() const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the authentication
  //////////////////////////////////////////////////////////////////////////////

  rest::HttpResponse::HttpResponseCode authenticate() override final;

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock SessionTimeout
  ////////////////////////////////////////////////////////////////////////////////

  static double ServerSessionTtl;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the server
  //////////////////////////////////////////////////////////////////////////////

  TRI_server_t* TRI_UNUSED _server;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the vocbase
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* _vocbase;
};
}

#endif
