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

#include "RestVocbaseBaseHandler.h"
#include "Basics/conversions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/document-collection.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief agency public path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::AGENCY_PATH = "/_api/agency";

////////////////////////////////////////////////////////////////////////////////
/// @brief agency private path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::AGENCY_PRIV_PATH = "/_api/agency_priv";

////////////////////////////////////////////////////////////////////////////////
/// @brief batch path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::BATCH_PATH = "/_api/batch";

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::CURSOR_PATH = "/_api/cursor";

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::DOCUMENT_PATH = "/_api/document";

////////////////////////////////////////////////////////////////////////////////
/// @brief edges path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::EDGES_PATH = "/_api/edges";

////////////////////////////////////////////////////////////////////////////////
/// @brief export path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::EXPORT_PATH = "/_api/export";

////////////////////////////////////////////////////////////////////////////////
/// @brief documents import path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::IMPORT_PATH = "/_api/import";

////////////////////////////////////////////////////////////////////////////////
/// @brief replication path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::REPLICATION_PATH =
    "/_api/replication";

////////////////////////////////////////////////////////////////////////////////
/// @brief simple query all path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH =
    "/_api/simple/all";

////////////////////////////////////////////////////////////////////////////////
/// @brief document batch lookup path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH =
    "/_api/simple/lookup-by-keys";

////////////////////////////////////////////////////////////////////////////////
/// @brief document batch remove path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH =
    "/_api/simple/remove-by-keys";

////////////////////////////////////////////////////////////////////////////////
/// @brief upload path
////////////////////////////////////////////////////////////////////////////////

std::string const RestVocbaseBaseHandler::UPLOAD_PATH = "/_api/upload";

RestVocbaseBaseHandler::RestVocbaseBaseHandler(HttpRequest* request)
    : RestBaseHandler(request),
      _context(static_cast<VocbaseContext*>(request->requestContext())),
      _vocbase(_context->getVocbase()),
      _nolockHeaderSet(nullptr) {}

RestVocbaseBaseHandler::~RestVocbaseBaseHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
/// optionally url-encodes
////////////////////////////////////////////////////////////////////////////////

std::string RestVocbaseBaseHandler::assembleDocumentId(
    std::string const& collectionName, std::string const& key, bool urlEncode) {
  if (urlEncode) {
    return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR +
           StringUtils::urlEncode(key);
  }
  return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Generate a result for successful save
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateSaved(
    arangodb::OperationResult const& result, std::string const& collectionName,
    TRI_col_type_e type,
    VPackOptions const* options) {
  if (result.wasSynchronous) {
    createResponse(rest::HttpResponse::CREATED);
  } else {
    createResponse(rest::HttpResponse::ACCEPTED);
  }
  generate20x(result, collectionName, type, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Generate a result for successful delete
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDeleted(
    arangodb::OperationResult const& result, std::string const& collectionName,
    TRI_col_type_e type, VPackOptions const* options) {
  if (result.wasSynchronous) {
    createResponse(rest::HttpResponse::OK);
  } else {
    createResponse(rest::HttpResponse::ACCEPTED);
  }
  generate20x(result, collectionName, type, options);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection needs to be created on the fly
///
/// this method will check the "createCollection" attribute of the request. if
/// it is set to true, it will verify that the named collection actually exists.
/// if the collection does not yet exist, it will create it on the fly.
/// if the "createCollection" attribute is not set or set to false, nothing will
/// happen, and the collection name will not be checked
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::checkCreateCollection(std::string const& name,
                                                   TRI_col_type_e type) {
  bool found;
  std::string const& valueStr = _request->value("createCollection", found);

  if (!found) {
    // "createCollection" parameter not specified
    return true;
  }

  if (!StringUtils::boolean(valueStr)) {
    // "createCollection" parameter specified, but with non-true value
    return true;
  }

  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isDBServer()) {
    // create-collection is not supported in a cluster
    generateTransactionError(name, TRI_ERROR_CLUSTER_UNSUPPORTED, "");
    return false;
  }

  TRI_vocbase_col_t* collection =
      TRI_FindCollectionByNameOrCreateVocBase(_vocbase, name.c_str(), type);

  if (collection == nullptr) {
    generateTransactionError(name, TRI_errno(), "");
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 20x response
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generate20x(
    arangodb::OperationResult const& result,
    std::string const& collectionName,
    TRI_col_type_e type,
    VPackOptions const* options) {

  VPackSlice slice = result.slice();
  TRI_ASSERT(slice.isObject() || slice.isArray());
  _response->setContentType("application/json; charset=utf-8");
  if (slice.isObject()) {
    _response->setHeader("etag", 4, "\"" + slice.get(TRI_VOC_ATTRIBUTE_REV).copyString() + "\"");
    // pre 1.4 location headers withdrawn for >= 3.0
    std::string escapedHandle(assembleDocumentId(
        collectionName, slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString(), true));
    _response->setHeader("location", 8,
                         std::string("/_db/" + _request->databaseName() +
                                     DOCUMENT_PATH + "/" + escapedHandle));
  }
  VPackStringBufferAdapter buffer(_response->body().stringBuffer());
  VPackDumper dumper(&buffer, options);
  try {
    dumper.dump(slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotImplemented(std::string const& path) {
  generateError(HttpResponse::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                "'" + path + "' not implemented");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates forbidden
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateForbidden() {
  generateError(HttpResponse::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                "operation forbidden");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generatePreconditionFailed(
    VPackSlice const& slice) {

  createResponse(HttpResponse::PRECONDITION_FAILED);
  _response->setContentType("application/json; charset=utf-8");
  if (slice.isObject()) {  // single document case
    std::string const rev = VelocyPackHelper::getStringValue(slice, TRI_VOC_ATTRIBUTE_REV, "");
    _response->setHeader("etag", 4, "\"" + rev + "\"");
  }
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add("error", VPackValue(true));
    builder.add(
        "code",
        VPackValue(static_cast<int32_t>(HttpResponse::PRECONDITION_FAILED)));
    builder.add("errorNum", VPackValue(TRI_ERROR_ARANGO_CONFLICT));
    builder.add("errorMessage", VPackValue("precondition failed"));
    if (slice.isObject()) {
      builder.add(TRI_VOC_ATTRIBUTE_ID, slice.get(TRI_VOC_ATTRIBUTE_ID));
      builder.add(TRI_VOC_ATTRIBUTE_KEY, slice.get(TRI_VOC_ATTRIBUTE_KEY));
      builder.add(TRI_VOC_ATTRIBUTE_REV, slice.get(TRI_VOC_ATTRIBUTE_REV));
    } else {
      builder.add("result", slice);
    }
  }

  VPackStringBufferAdapter buffer(_response->body().stringBuffer());
  auto transactionContext(StandaloneTransactionContext::Create(_vocbase));
  VPackDumper dumper(&buffer, transactionContext->getVPackOptions());

  try {
    dumper.dump(builder.slice());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generatePreconditionFailed(
    std::string const& collectionName, std::string const& key, TRI_voc_rid_t rev) {

  VPackBuilder builder;
  builder.openObject();
  builder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(assembleDocumentId(collectionName, key, false)));
  builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(std::to_string(rev)));
  builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(key));
  builder.close();

  generatePreconditionFailed(builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified(TRI_voc_rid_t rid) {
  std::string const&& rev = StringUtils::itoa(rid);

  createResponse(HttpResponse::NOT_MODIFIED);
  _response->setHeader("etag", 4, "\"" + rev + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument(VPackSlice const& document,
                                              bool generateBody,
                                              VPackOptions const* options) {
  std::string rev;
  if (document.isObject()) {
    rev = VelocyPackHelper::getStringValue(document, TRI_VOC_ATTRIBUTE_REV, "");
  }

  // and generate a response
  createResponse(HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  if (!rev.empty()) {
    _response->setHeader("etag", 4, "\"" + rev + "\"");
  }

  if (generateBody) {
    VPackStringBufferAdapter buffer(_response->body().stringBuffer());
    VPackDumper dumper(&buffer, options);
    try {
      dumper.dump(document);
    } catch (...) {
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "cannot generate output");
    }
  } else {
    // TODO can we optimize this?
    // Just dump some where else to find real length
    StringBuffer tmp(TRI_UNKNOWN_MEM_ZONE);
    // convert object to string
    VPackStringBufferAdapter buffer(tmp.stringBuffer());
    VPackDumper dumper(&buffer, options);
    try {
      dumper.dump(document);
    } catch (...) {
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "cannot generate output");
    }
    _response->headResponse(tmp.length());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError(
    std::string const& collectionName, int res, std::string const& key,
    TRI_voc_rid_t rev) {
  switch (res) {
    case TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND:
      if (collectionName.empty()) {
        // no collection name specified
        generateError(HttpResponse::BAD, res, "no collection name specified");
      } else {
        // collection name specified but collection not found
        generateError(HttpResponse::NOT_FOUND, res,
                      "collection '" + collectionName + "' not found");
      }
      return;

    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(HttpResponse::FORBIDDEN, res, "collection is read-only");
      return;
    
    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(HttpResponse::CONFLICT, res,
                    "cannot create document, unique constraint violated");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
      generateError(HttpResponse::BAD, res, "invalid document key");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD:
      generateError(HttpResponse::BAD, res, "invalid document handle");
      return;

    case TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE:
      generateError(HttpResponse::BAD, res, "invalid edge attribute");
      return;

    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
      generateError(HttpResponse::SERVER_ERROR, res, "out of keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
      generateError(HttpResponse::BAD, res,
                    "collection does not allow using user-defined keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateDocumentNotFound(collectionName, key);
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
      generateError(HttpResponse::BAD, res);
      return;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(collectionName,
                                 key.empty() ? "unknown" : key, rev);
      return;

    case TRI_ERROR_CLUSTER_SHARD_GONE:
      generateError(HttpResponse::SERVER_ERROR, res,
                    "coordinator: no responsible shard found");
      return;

    case TRI_ERROR_CLUSTER_TIMEOUT:
      generateError(HttpResponse::SERVER_ERROR, res);
      return;
    
    case TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE:
      generateError(HttpResponse::SERVICE_UNAVAILABLE, res, "A required backend was not available");
      return;

    case TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES:
    case TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY: {
      generateError(HttpResponse::BAD, res);
      return;
    }

    case TRI_ERROR_CLUSTER_UNSUPPORTED: {
      generateError(HttpResponse::NOT_IMPLEMENTED, res);
      return;
    }
    
    case TRI_ERROR_FORBIDDEN: {
      generateError(HttpResponse::FORBIDDEN, res);
      return;
    }
        
    case TRI_ERROR_OUT_OF_MEMORY: 
    case TRI_ERROR_LOCK_TIMEOUT: 
    case TRI_ERROR_DEBUG:
    case TRI_ERROR_LOCKED:
    case TRI_ERROR_DEADLOCK: {
      generateError(HttpResponse::SERVER_ERROR, res);
      return;
    }

    default:
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "failed with error: " + std::string(TRI_errno_string(res)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError(
    OperationResult const& result) {
  switch (result.code) {
    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(HttpResponse::FORBIDDEN, result.code,
                    "collection is read-only");
      return;
    
    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(HttpResponse::CONFLICT, result.code,
                    "cannot create document, unique constraint violated");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
      generateError(HttpResponse::BAD, result.code, "invalid document key");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD:
      generateError(HttpResponse::BAD, result.code, "invalid document handle");
      return;

    case TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE:
      generateError(HttpResponse::BAD, result.code, "invalid edge attribute");
      return;

    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
      generateError(HttpResponse::SERVER_ERROR, result.code, "out of keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
      generateError(HttpResponse::BAD, result.code,
                    "collection does not allow using user-defined keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateError(rest::HttpResponse::NOT_FOUND,
                    TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
      generateError(HttpResponse::BAD, result.code);
      return;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(result.slice());
      return;

    case TRI_ERROR_CLUSTER_SHARD_GONE:
      generateError(HttpResponse::SERVER_ERROR, result.code,
                    "coordinator: no responsible shard found");
      return;

    case TRI_ERROR_CLUSTER_TIMEOUT:
      generateError(HttpResponse::SERVER_ERROR, result.code);
      return;

    case TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES:
    case TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY: {
      generateError(HttpResponse::BAD, result.code);
      return;
    }

    case TRI_ERROR_CLUSTER_UNSUPPORTED: {
      generateError(HttpResponse::NOT_IMPLEMENTED, result.code);
      return;
    }
    
    case TRI_ERROR_FORBIDDEN: {
      generateError(HttpResponse::FORBIDDEN, result.code);
      return;
    }
        
    case TRI_ERROR_OUT_OF_MEMORY: 
    case TRI_ERROR_LOCK_TIMEOUT: 
    case TRI_ERROR_DEBUG:
    case TRI_ERROR_LOCKED:
    case TRI_ERROR_DEADLOCK: {
      generateError(HttpResponse::SERVER_ERROR, result.code);
      return;
    }

    default:
      generateError(
          HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
          "failed with error: " + std::string(TRI_errno_string(result.code)));
  }
}



////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t RestVocbaseBaseHandler::extractRevision(char const* header,
                                                      char const* parameter,
                                                      bool& isValid) {
  isValid = true;
  bool found;
  std::string const& etag = _request->header(header, found);

  if (found) {
    char const* s = etag.c_str();
    char const* e = s + etag.size();

    while (s < e && (s[0] == ' ' || s[0] == '\t')) {
      ++s;
    }

    if (s < e && (s[0] == '"' || s[0] == '\'')) {
      ++s;
    }

    while (s < e && (e[-1] == ' ' || e[-1] == '\t')) {
      --e;
    }

    if (s < e && (e[-1] == '"' || e[-1] == '\'')) {
      --e;
    }

    TRI_voc_rid_t rid = 0;

    try {
      rid = StringUtils::uint64_check(s, e - s);
      isValid = true;
    }
    catch (...) {
      isValid = false;
    }

    return rid;
  }

  if (parameter != nullptr) {
    std::string const& etag2 = _request->value(parameter, found);

    if (found) {
      TRI_voc_rid_t rid = 0;

      try {
        rid = StringUtils::uint64_check(etag2);
        isValid = true;
      }
      catch (...) {
        isValid = false;
      }

      return rid;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a boolean parameter value
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::extractBooleanParameter(char const* name,
                                                     bool def) const {
  bool found;
  std::string const& value = _request->value(name, found);

  if (found) {
    return StringUtils::boolean(value);
  }

  return def;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body as VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestVocbaseBaseHandler::parseVelocyPackBody(
    VPackOptions const* options, bool& success) {
  try {
    success = true;
    return _request->toVelocyPack(options);
  } catch (std::bad_alloc const&) {
    generateOOMError();
  } catch (VPackException const& e) {
    std::string errmsg("Parse error: ");
    errmsg.append(e.what());
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON, errmsg);
  }
  success = false;
  return std::make_shared<VPackBuilder>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepareExecute, to react to X-Arango-Nolock header
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::prepareExecute() {
  RestBaseHandler::prepareExecute();

  bool found;
  std::string const& shardId = _request->header("x-arango-nolock", found);

  if (found) {
    _nolockHeaderSet = new std::unordered_set<std::string>();
    _nolockHeaderSet->insert(std::string(shardId));
    arangodb::Transaction::_makeNolockHeaders = _nolockHeaderSet;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalizeExecute, to react to X-Arango-Nolock header
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::finalizeExecute() {
  if (_nolockHeaderSet != nullptr) {
    arangodb::Transaction::_makeNolockHeaders = nullptr;
    delete _nolockHeaderSet;
    _nolockHeaderSet = nullptr;
  }

  RestBaseHandler::finalizeExecute();
}
