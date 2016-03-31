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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/ArangoServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestAgencyHandler.h"

#include "Agency/Agent.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/Logger.h"

using namespace arangodb;

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern ArangoServer* ArangoInstance;

RestAgencyHandler::RestAgencyHandler(HttpRequest* request, Agent* agent)
    : RestBaseHandler(request), _agent(agent) {
}

bool RestAgencyHandler::isDirect() const { return false; }

inline HttpHandler::status_t RestAgencyHandler::reportErrorEmptyRequest () {
  LOG_TOPIC(WARN, Logger::AGENCY) << "Empty request to public agency interface.";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::reportTooManySuffices () {
  LOG_TOPIC(WARN, Logger::AGENCY)
    << "Too many suffixes. Agency public interface takes one path.";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::reportUnknownMethod () {
  LOG_TOPIC(WARN, Logger::AGENCY)
    << "Public REST interface has no method " << _request->suffix()[0];
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

void RestAgencyHandler::redirectRequest (id_t leaderId) {
  std::string rendpoint = _agent->config().end_points.at(leaderId);
  rendpoint = rendpoint.substr(6,rendpoint.size()-6);
  rendpoint = std::string("http://" + rendpoint + _request->requestPath());
  createResponse(HttpResponse::TEMPORARY_REDIRECT);
  _response->setHeader("Location", rendpoint);
}

inline HttpHandler::status_t RestAgencyHandler::handleWrite () {
  arangodb::velocypack::Options options; // TODO: User not wait. 
  if (_request->requestType() == GeneralRequest::RequestType::POST) {

    query_t query;

    try {
      query = _request->toVelocyPack(&options);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
      Builder body;
      body.openObject();
      body.add("message", VPackValue(e.what()));
      body.close();
      generateResult(HttpResponse::BAD,body.slice());
      return HttpHandler::status_t(HANDLER_DONE);
    }

    write_ret_t ret = _agent->write (query);
    
    if (ret.accepted) { // We're leading and handling the request

      std::string const& call_mode =_request->header("x-arangodb-agency-mode");
      size_t errors = 0;
      Builder body;
      body.openObject();
      
      if (call_mode!="noWait") {

        // Note success/error
        body.add("results", VPackValue(VPackValueType::Array));
        for (auto const& index : ret.indices) {
          body.add(VPackValue(index));
        }
        body.close();

        // Wait for commit of highest except if it is 0?
        if (call_mode=="waitForCommitted") {
          index_t max_index =
            *std::max_element(ret.indices.begin(),ret.indices.end());
          if (max_index>0) {
            _agent->waitFor (max_index);
          }
        }
        
      }
      
      body.close();
      
      if (errors > 0) { // Some/all requests failed
        generateResult(HttpResponse::PRECONDITION_FAILED,body.slice());
      } else {          // All good 
        generateResult(HttpResponse::OK, body.slice());
      }
    } else {            // Redirect to leader
      redirectRequest(ret.redirect);
    }
  } else {              // Unknown method
    generateError(HttpResponse::METHOD_NOT_ALLOWED,405);
  }
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::handleRead () {
  arangodb::velocypack::Options options;
  if (_request->requestType() == GeneralRequest::RequestType::POST) {
    query_t query;
    try {
      query = _request->toVelocyPack(&options);
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, Logger::AGENCY) << e.what();
      generateError(HttpResponse::BAD,400);
      return HttpHandler::status_t(HANDLER_DONE);
    }
    read_ret_t ret = _agent->read (query);

    if (ret.accepted) { // I am leading
      generateResult(HttpResponse::OK, ret.result->slice());
    } else {            // Redirect to leader
      redirectRequest(ret.redirect);
      return HttpHandler::status_t(HANDLER_DONE);
    }
  } else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED,405);
    return HttpHandler::status_t(HANDLER_DONE);
  }
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyHandler::handleConfig() {
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  body.add("term", Value(_agent->term()));
  body.add("leaderId", Value(_agent->leaderID()));
  body.add("configuration", _agent->config().toBuilder()->slice());
  body.close();
  generateResult(HttpResponse::OK, body.slice());
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyHandler::handleState() {
  Builder body;
  body.add(VPackValue(VPackValueType::Array));
  for (auto const& i: _agent->state().get()) {
    body.add(VPackValue(VPackValueType::Object));
    body.add("index", VPackValue(i.index));
    body.add("term", VPackValue(i.term));
    body.add("leader", VPackValue(i.leaderId));
    body.add("query", VPackSlice(i.entry->data()));
    body.close();
  }
  body.close();
  generateResult(HttpResponse::OK, body.slice());
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::reportMethodNotAllowed () {
  generateError(HttpResponse::METHOD_NOT_ALLOWED,405);
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyHandler::execute() {
  try {
    if (_request->suffix().size() == 0) {         // Empty request
      return reportErrorEmptyRequest();
    } else if (_request->suffix().size() > 1) {   // path size >= 2
      return reportTooManySuffices();
    } else {
        if (_request->suffix()[0] == "write") {
        return handleWrite();
      } else if (_request->suffix()[0] == "read") {
        return handleRead();
      } else if (_request->suffix()[0] == "config") {
        if (_request->requestType() != GeneralRequest::RequestType::GET) {
          return reportMethodNotAllowed();
        }
        return handleConfig();
        } else if (_request->suffix()[0] == "state") {
        if (_request->requestType() != GeneralRequest::RequestType::GET) {
          return reportMethodNotAllowed();
        }
        return handleState();
        } else {
        return reportUnknownMethod();
        }
    }
  } catch (...) {
    // Ignore this error
  }
  return HttpHandler::status_t(HANDLER_DONE);
}
