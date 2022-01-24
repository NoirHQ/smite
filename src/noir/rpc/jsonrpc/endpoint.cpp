// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/rpc/jsonrpc/endpoint.h>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

namespace noir::jsonrpc {

  void detail::endpoint_impl::add_handler( const std::string& method_name, request_handler handler ) {
    handlers.emplace( method_name, handler );
  }

  void detail::endpoint_impl::rpc_id( const fc::variant_object& request, response& response ) {
    if( request.contains( "id" ) ) {
      const fc::variant& _id = request[ "id" ];
      int _type = _id.get_type();
      switch( _type ) {
      case fc::variant::int64_type:
      case fc::variant::uint64_type:
      case fc::variant::string_type:
        response.id = request[ "id" ];
        break;
      default:
        response.error = error( error_code::invalid_request, "Only integer value or string is allowed for member \"id\"" );
      }
    }
  }

  void detail::endpoint_impl::rpc_jsonrpc( const fc::variant_object& request, response& response ) {
    if( request.contains( "jsonrpc" ) && request[ "jsonrpc" ].is_string() && request[ "jsonrpc" ].as_string() == "2.0" ) {
      if( request.contains( "method" ) && request[ "method" ].is_string() ) {
        std::string method = request[ "method" ].as_string();
        try {
          fc::variant func_args = request.contains( "params" ) ? request[ "params" ] : fc::variant( "{}" );
          response.result = handlers.at(method)( func_args );
        } catch( fc::assert_exception& e ) {
          response.error = error( error_code::internal_error, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
        } catch( std::out_of_range& e ) {
          response.error = error( error_code::method_not_found, "method not found", fc::variant( method ) );
        }
      } else {
        response.error = error( error_code::invalid_request, "A member \"method\" does not exist" );
      }
    } else {
      response.error = error( error_code::invalid_request, "jsonrpc value is not \"2.0\"" );
    }
  }

  response detail::endpoint_impl::rpc( const fc::variant& message ) {
    response response;
    try {
      const auto& request = message.get_object();
      rpc_id( request, response );
      // This second layer try/catch is to isolate errors that occur after parsing the id so that the id is properly returned.
      try {
        if( !response.error )
          rpc_jsonrpc( request, response );
      } catch( fc::exception& e ) {
        response.error = error( error_code::server_error, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      } catch( std::exception& e ) {
        response.error = error( error_code::server_error, "Unknown error - parsing rpc message failed", fc::variant( e.what() ) );
      } catch( ... ) {
        response.error = error( error_code::server_error, "Unknown error - parsing rpc message failed" );
      }
    } catch( fc::parse_error_exception& e ) {
      response.error = error( error_code::parse_error, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
    } catch( fc::bad_cast_exception& e ) {
      response.error = error( error_code::parse_error, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
    } catch( fc::exception& e ) {
      response.error = error( error_code::server_error, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
    } catch( std::exception& e ) {
      response.error = error( error_code::server_error, "Unknown error - parsing rpc message failed", fc::variant( e.what() ) );
    } catch( ... ) {
      response.error = error( error_code::server_error, "Unknown error - parsing rpc message failed" );
    }
    return response;
  }

  fc::variant detail::endpoint_impl::handle_request( const std::string& message ) {
    try {
      fc::variant v = fc::json::from_string( message );
      if( v.is_array() ) {
        fc::variants messages = v.as<fc::variants>();
        fc::variants responses;
        if( messages.size() ) {
          responses.reserve( messages.size() );
          for( auto& m : messages ) {
            auto response = rpc( m );
            if( m.get_object().contains( "id" ) )
              responses.push_back( response );
          }
          return responses;
        } else {
          // For example: message == "[]"
          response response;
          response.error = error( error_code::server_error, "Array is invalid" );
          return response;
        }
      } else {
        auto response = rpc( v );
        return v.get_object().contains( "id" ) ? response : fc::variant();
      }
    } catch( fc::exception& e ) {
      response response;
      response.error = error( error_code::server_error, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      return response;
    } catch( ... ) {
      response response;
      response.error = error( error_code::server_error, "Unknown exception", fc::variant(
                                                                              fc::unhandled_exception( FC_LOG_MESSAGE( warn, "Unknown Exception" ), std::current_exception() ).to_detail_string() ) );
      return response;
    }
  }

  void endpoint::add_handler( const std::string& method_name, request_handler handler ) {
    ilog("${method} is added", ("method", method_name));
    my->add_handler( method_name, handler );
  }

  fc::variant endpoint::handle_request( const std::string& message ) {
    return my->handle_request( message );
  }

} // namespace noir::jsonrpc
