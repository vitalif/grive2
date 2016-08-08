/*
	grive: an GPL program to sync a local directory with Google Drive
	Copyright (C) 2012  Wan Wai Ho

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation version 2
	of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "CurlAgent.hh"

#include "Error.hh"
#include "Header.hh"

#include "util/log/Log.hh"
#include "util/DataStream.hh"
#include "util/File.hh"

#include <boost/throw_exception.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <streambuf>
#include <iostream>

#include <signal.h>
#include <math.h>

namespace {

using namespace gr::http ;
using namespace gr ;

std::size_t ReadFileCallback( void *ptr, std::size_t size, std::size_t nmemb, SeekStream *file )
{
	assert( ptr != 0 ) ;
	assert( file != 0 ) ;

	if ( size*nmemb > 0 )
		return file->Read( static_cast<char*>(ptr), size*nmemb ) ;

	return 0 ;
}

} // end of local namespace

namespace gr { namespace http {

struct CurlAgent::Impl
{
	CURL			*curl ;
	std::string		location ;
	bool			error ;
	std::string		error_headers ;
	std::string		error_data ;
	DataStream		*dest ;
} ;

static struct curl_slist* SetHeader( CURL* handle, const Header& hdr );

CurlAgent::CurlAgent() : Agent(),
	m_pimpl( new Impl )
{
	m_pimpl->curl = ::curl_easy_init();
}

void CurlAgent::Init()
{
	::curl_easy_reset( m_pimpl->curl ) ;
	::curl_easy_setopt( m_pimpl->curl, CURLOPT_SSL_VERIFYPEER,	0L ) ;
	::curl_easy_setopt( m_pimpl->curl, CURLOPT_SSL_VERIFYHOST,	0L ) ;
	::curl_easy_setopt( m_pimpl->curl, CURLOPT_HEADERFUNCTION,	&CurlAgent::HeaderCallback ) ;
	::curl_easy_setopt( m_pimpl->curl, CURLOPT_HEADERDATA,		this ) ;
	::curl_easy_setopt( m_pimpl->curl, CURLOPT_HEADER,			0L ) ;
	if ( mMaxUpload > 0 )
		::curl_easy_setopt( m_pimpl->curl, CURLOPT_MAX_SEND_SPEED_LARGE, mMaxUpload ) ;
	if ( mMaxDownload > 0 )
		::curl_easy_setopt( m_pimpl->curl, CURLOPT_MAX_RECV_SPEED_LARGE, mMaxDownload ) ;
	m_pimpl->error = false;
	m_pimpl->error_headers = "";
	m_pimpl->error_data = "";
	m_pimpl->dest = NULL;
}

CurlAgent::~CurlAgent()
{
	::curl_easy_cleanup( m_pimpl->curl );
}

ResponseLog* CurlAgent::GetLog() const
{
	return m_log.get();
}

void CurlAgent::SetLog(ResponseLog *log)
{
	m_log.reset( log );
}

std::size_t CurlAgent::HeaderCallback( void *ptr, size_t size, size_t nmemb, CurlAgent *pthis )
{
	char *str = static_cast<char*>(ptr) ;
	std::string line( str, str + size*nmemb ) ;
	
	// Check for error (HTTP 400 and above)
	if ( line.substr( 0, 5 ) == "HTTP/" && line[9] >= '4' )
		pthis->m_pimpl->error = true;
	
	if ( pthis->m_pimpl->error )
		pthis->m_pimpl->error_headers += line;
	
	if ( pthis->m_log.get() )
		pthis->m_log->Write( str, size*nmemb );
	
	static const std::string loc = "Location: " ;
	std::size_t pos = line.find( loc ) ;
	if ( pos != line.npos )
	{
		std::size_t end_pos = line.find( "\r\n", pos ) ;
		pthis->m_pimpl->location = line.substr( loc.size(), end_pos - loc.size() ) ;
	}
	
	return size*nmemb ;
}

std::size_t CurlAgent::Receive( void* ptr, size_t size, size_t nmemb, CurlAgent *pthis )
{
	assert( pthis != 0 ) ;

	if ( pthis->m_log.get() )
		pthis->m_log->Write( (const char*)ptr, size*nmemb );

	if (pthis->totalDownlaodSize > 0) {
		pthis->downloadedBytes += (curl_off_t)size*nmemb;
		CurlAgent::progress_callback(pthis, pthis->totalDownlaodSize, pthis->downloadedBytes, 0L, 0L);
	}

	if ( pthis->m_pimpl->error && pthis->m_pimpl->error_data.size() < 65536 )
	{
		// Do not feed error responses to destination stream
		pthis->m_pimpl->error_data.append( static_cast<char*>(ptr), size * nmemb ) ;
		return size * nmemb ;
	}
	return pthis->m_pimpl->dest->Write( static_cast<char*>(ptr), size * nmemb ) ;
}


std::string CurlAgent::CalculateByteSize(curl_off_t bytes, bool withSuffix) {
	long double KB = bytes / 1024;
	long double MB = KB / 1024;
	long double GB = MB / 1024;
	std::string res;
	std::string suffix;

	std::ostringstream ss;
	ss << std::fixed << std::setprecision(2);

	if (GB > 1) {
		ss << GB;
		suffix = "GB";
	}
	else if (MB > 1) {
		ss << MB;
		suffix = "MB";
	} else {
		ss << KB;
		suffix = "KB";
	}

	res = ss.str() + (withSuffix ? suffix : "");

	return res;
}


int CurlAgent::progress_callback(void *ptr,   curl_off_t TotalDownloadSize,   curl_off_t finishedDownloadSize,   curl_off_t TotalToUpload,   curl_off_t NowUploaded) {
	curl_off_t processed = (TotalDownloadSize > TotalToUpload) ? finishedDownloadSize : NowUploaded;
	curl_off_t total = (TotalDownloadSize > TotalToUpload) ? TotalDownloadSize : TotalToUpload;

	if (total <= 0.0)
        return 0;

	//libcurl seems to process more bytes then the actual file size :)
	if (processed > total)
		processed = total;

    int totaldotz = 100;
    double fraction = (float)processed / total;

    if ((fraction*100) < 100.0)
    	((CurlAgent*)ptr)->hundredpercentDone = false;

    if (!((CurlAgent*)ptr)->hundredpercentDone) {
    	printf("\33[2K\r");	//delete previous output line
    	int dotz = round(fraction * totaldotz);

		int count=0;
		printf("  [%3.0f%%] [", fraction*100);

		for (; count < dotz-1; count++) {
			printf("=");
		}

		printf(">");

		for (; count < totaldotz-1; count++) {
			printf(" ");
		}

		printf("] ");

		printf("%s/%s", CalculateByteSize(processed, false).c_str(), CalculateByteSize(total, true).c_str());

		printf("\r");

		if ((fraction*100) >= 100.0) {
			((CurlAgent*)ptr)->hundredpercentDone = true;
			printf("\n");
		}

		fflush(stdout);
    }

    return 0;
}


long CurlAgent::ExecCurl(
	const std::string&	url,
	DataStream			*dest,
	const http::Header&	hdr )
{
	CURL *curl = m_pimpl->curl ;
	assert( curl != 0 ) ;

	char error[CURL_ERROR_SIZE] = {} ;
	::curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, 	error ) ;
	::curl_easy_setopt(curl, CURLOPT_URL, 			url.c_str());
	::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,	&CurlAgent::Receive ) ;
	::curl_easy_setopt(curl, CURLOPT_WRITEDATA,		this ) ;
	m_pimpl->dest = dest ;

	struct curl_slist *slist = SetHeader( m_pimpl->curl, hdr ) ;

	if (progressBar) {
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
	}


	CURLcode curl_code = ::curl_easy_perform(curl);

	curl_slist_free_all(slist);

	// get the HTTP response code
	long http_code = 0;
	::curl_easy_getinfo(curl,	CURLINFO_RESPONSE_CODE, &http_code);
	Trace( "HTTP response %1%", http_code ) ;

	// reset the curl buffer to prevent it from touching our "error" buffer
	::curl_easy_setopt(curl,	CURLOPT_ERRORBUFFER, 	0 ) ;

	m_pimpl->dest = NULL;

	// only throw for libcurl errors
	if ( curl_code != CURLE_OK )
	{
		BOOST_THROW_EXCEPTION(
			Error()
				<< CurlCode( curl_code )
				<< Url( url )
				<< CurlErrMsg( error )
				<< HttpRequestHeaders( hdr )
		) ;
	}

	return http_code ;
}

long CurlAgent::Request(
	const std::string&	method,
	const std::string&	url,
	SeekStream			*in,
	DataStream			*dest,
	const Header&		hdr,
	const long			downloadFileBytes)
{

	Trace("HTTP %1% \"%2%\"", method, url ) ;

	Init() ;
	CURL *curl = m_pimpl->curl ;
	progressBar = false;
	totalDownlaodSize = 0;

	// set common options
	::curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str() );
	if ( in )
	{
		::curl_easy_setopt(curl, CURLOPT_UPLOAD,			1L ) ;
		::curl_easy_setopt(curl, CURLOPT_READFUNCTION,		&ReadFileCallback ) ;
		::curl_easy_setopt(curl, CURLOPT_READDATA ,			in ) ;
		::curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, 	static_cast<curl_off_t>( in->Size() ) ) ;

		if (url.compare("https://accounts.google.com/o/oauth2/token"))
			progressBar = true;
		else
			progressBar = false;
	} else {
		if (!boost::starts_with(url, "https://www.googleapis.com/")) {
			progressBar = true;
			totalDownlaodSize = downloadFileBytes;
		} else
			progressBar = false;
	}

	return ExecCurl( url, dest, hdr ) ;
}

static struct curl_slist* SetHeader( CURL *handle, const Header& hdr )
{
	// set headers
	struct curl_slist *curl_hdr = 0 ;
    for ( Header::iterator i = hdr.begin() ; i != hdr.end() ; ++i )
		curl_hdr = curl_slist_append( curl_hdr, i->c_str() ) ;
	
	::curl_easy_setopt( handle, CURLOPT_HTTPHEADER, curl_hdr ) ;
	return curl_hdr;
}

std::string CurlAgent::LastError() const
{
	return m_pimpl->error_data ;
}

std::string CurlAgent::LastErrorHeaders() const
{
	return m_pimpl->error_headers ;
}

std::string CurlAgent::RedirLocation() const
{
	return m_pimpl->location ;
}

std::string CurlAgent::Escape( const std::string& str )
{
	CURL *curl = m_pimpl->curl ;
	
	char *tmp = curl_easy_escape( curl, str.c_str(), str.size() ) ;
	std::string result = tmp ;
	curl_free( tmp ) ;
	
	return result ;
}

std::string CurlAgent::Unescape( const std::string& str )
{
	CURL *curl = m_pimpl->curl ;
	
	int r ;
	char *tmp = curl_easy_unescape( curl, str.c_str(), str.size(), &r ) ;
	std::string result = tmp ;
	curl_free( tmp ) ;
	
	return result ;
}

} } // end of namespace
