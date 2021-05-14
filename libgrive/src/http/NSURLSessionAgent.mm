#include "NSURLSessionAgent.hh"

#include <Foundation/Foundation.h>

#include "Error.hh"
#include "Header.hh"
#include "util/log/Log.hh"

namespace gr {
namespace http {

struct NSURLSessionAgent::Impl {
	NSURLSession *session;
	std::string location;
	bool error;
	std::string error_headers;
	std::string error_data;
	DataStream *dest;
	u64_t total_download, total_upload;
};

NSURLSessionAgent::NSURLSessionAgent()
    : Agent(), m_pimpl( new Impl ), m_pb( 0 ) {
	m_pimpl->session = NSURLSession.sharedSession;
}

NSURLSessionAgent::~NSURLSessionAgent() {
}

void NSURLSessionAgent::Init() {
	m_pimpl->error = false;
	m_pimpl->error_headers = "";
	m_pimpl->error_data = "";
	m_pimpl->dest = NULL;
	m_pimpl->total_download = m_pimpl->total_upload = 0;
}

ResponseLog *NSURLSessionAgent::GetLog() const {
	return m_log.get();
}

void NSURLSessionAgent::SetLog( ResponseLog *log ) {
	m_log.reset( log );
}

void NSURLSessionAgent::SetProgressReporter( Progress *progress ) {
	m_pb = progress;
}

long NSURLSessionAgent::Request( const std::string &method,
                                 const std::string &url,
                                 SeekStream *in,
                                 DataStream *dest,
                                 const Header &hdr,
                                 u64_t downloadFileBytes ) {
	Trace( "HTTP %1% \"%2%\"", method, url );

	Init();
	m_pimpl->total_download = downloadFileBytes;
	m_pimpl->dest = dest;

	NSURL *nsurl =
	    [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
	NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:nsurl];
	req.HTTPMethod = [NSString stringWithUTF8String:method.c_str()];

	const auto colon = std::string( ":" );
	for ( auto it = hdr.begin(); it != hdr.end(); it++ ) {
		const auto index = it->find_first_of( colon );
		[req setValue:[[NSString
		                  stringWithUTF8String:it->substr( index + 1 ).c_str()]
		                  stringByTrimmingCharactersInSet:
		                      NSCharacterSet.whitespaceAndNewlineCharacterSet]
		    forHTTPHeaderField:
		        [[NSString stringWithUTF8String:it->substr( 0, index ).c_str()]
		            stringByTrimmingCharactersInSet:
		                NSCharacterSet.whitespaceAndNewlineCharacterSet]];
	}

	if ( in ) {
		char *ptr = static_cast<char *>( malloc( in->Size() ) );
		std::size_t n = in->Read( ptr, in->Size() );
		assert( n == in->Size() );
		req.HTTPBody = [NSData dataWithBytes:ptr
		                              length:( NSUInteger )in->Size()];
	}

	__block NSInteger statusCode = 0;
	dispatch_semaphore_t sema = dispatch_semaphore_create( 0 );
	[[m_pimpl->session
	    dataTaskWithRequest:req
	      completionHandler:^(
	          NSData *data, NSURLResponse *response, NSError *error ) {
		      m_pimpl->total_download += data.length;
		      if ( error ) {
			      m_pimpl->error = true;
			      m_pimpl->error_data.append(
			          error.localizedDescription.UTF8String,
			          error.localizedDescription.length );
			      m_pimpl->error_headers = hdr.Str();
			      return;
		      }
		      statusCode = ( ( NSHTTPURLResponse * )response ).statusCode;
		      if ( statusCode < 200 || statusCode > 299 ) {
			      m_pimpl->error = true;
			      if ( data.bytes != nil ) {
				      m_pimpl->error_data.append(
				          static_cast<const char *>( data.bytes ) );
			      }
		      } else {
			      m_pimpl->dest->Write(
			          static_cast<const char *>( data.bytes ), data.length );
		      }
		      dispatch_semaphore_signal( sema );
	      }] resume];
	dispatch_semaphore_wait( sema, dispatch_time( DISPATCH_TIME_NOW, 30e9 ) );

	if ( m_pimpl->error ) {
		BOOST_THROW_EXCEPTION( Error()
		                       << Url( url ) << HttpRequestHeaders( hdr )
		                       << HttpResponseCode( statusCode )
		                       << HttpResponseText( m_pimpl->error_data ) );
	}

	return statusCode;
}

std::string NSURLSessionAgent::LastError() const {
	return m_pimpl->error_data;
}

std::string NSURLSessionAgent::LastErrorHeaders() const {
	return m_pimpl->error_headers;
}

std::string NSURLSessionAgent::RedirLocation() const {
	return m_pimpl->location;
}

std::string NSURLSessionAgent::Escape( const std::string &str ) {
	return str;
}

std::string NSURLSessionAgent::Unescape( const std::string &str ) {
	return str;
}

}
}
