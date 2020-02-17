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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
   USA.
*/

#pragma once

#include "Agent.hh"

#include <memory>
#include <string>

namespace gr {

class DataStream;

namespace http {

/*!	\brief	agent to provide HTTP access

    This class provides functions to send HTTP request in many methods (e.g.
   get, post and put). Normally the HTTP response is returned in a Receivable.
*/
class NSURLSessionAgent : public Agent {
public:
	NSURLSessionAgent();
	~NSURLSessionAgent();
	ResponseLog *GetLog() const;
	void SetLog( ResponseLog *log );
	void SetProgressReporter( Progress *progress );
	long Request( const std::string &method,
	              const std::string &url,
	              SeekStream *in,
	              DataStream *dest,
	              const Header &hdr,
	              u64_t downloadFileBytes = 0 );
	std::string LastError() const;
	std::string LastErrorHeaders() const;
	std::string RedirLocation() const;
	std::string Escape( const std::string &str );
	std::string Unescape( const std::string &str );

private:
	void Init();

private:
	struct Impl;
	std::unique_ptr<Impl> m_pimpl;
	std::unique_ptr<ResponseLog> m_log;
	Progress *m_pb;
};

} // namespace http
} // namespace gr
