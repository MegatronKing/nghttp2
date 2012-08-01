/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "shrpx_config.h"

#include <pwd.h>
#include <netdb.h>

#include <cstring>
#include <cerrno>
#include <limits>
#include <fstream>
#include <iostream>

#include "util.h"

using namespace spdylay;

namespace shrpx {

const char SHRPX_OPT_PRIVATE_KEY_FILE[] = "private-key-file";
const char SHRPX_OPT_CERTIFICATE_FILE[] = "certificate-file";

const char SHRPX_OPT_BACKEND[] = "backend";
const char SHRPX_OPT_FRONTEND[] = "frontend";
const char SHRPX_OPT_WORKERS[] = "workers";
const char
SHRPX_OPT_SPDY_MAX_CONCURRENT_STREAMS[] = "spdy-max-concurrent-streams";
const char SHRPX_OPT_LOG_LEVEL[] = "log-level";
const char SHRPX_OPT_DAEMON[] = "daemon";
const char SHRPX_OPT_SPDY_PROXY[] = "spdy-proxy";
const char SHRPX_OPT_ADD_X_FORWARDED_FOR[] = "add-x-forwarded-for";
const char
SHRPX_OPT_FRONTEND_SPDY_READ_TIMEOUT[] = "frontend-spdy-read-timeout";
const char SHRPX_OPT_FRONTEND_READ_TIMEOUT[] = "frontend-read-timeout";
const char SHRPX_OPT_FRONTEND_WRITE_TIMEOUT[] = "frontend-write-timeout";
const char SHRPX_OPT_BACKEND_READ_TIMEOUT[] = "backend-read-timeout";
const char SHRPX_OPT_BACKEND_WRITE_TIMEOUT[] = "backend-write-timeout";
const char SHRPX_OPT_ACCESSLOG[] = "accesslog";
const char
SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT[] = "backend-keep-alive-timeout";
const char SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS[] = "frontend-spdy-window-bits";
const char SHRPX_OPT_PID_FILE[] = "pid-file";
const char SHRPX_OPT_USER[] = "user";

Config::Config()
  : verbose(false),
    daemon(false),
    host(0),
    port(0),
    private_key_file(0),
    cert_file(0),
    verify_client(false),
    server_name(0),
    downstream_host(0),
    downstream_port(0),
    downstream_hostport(0),
    downstream_addrlen(0),
    num_worker(0),
    spdy_max_concurrent_streams(0),
    spdy_proxy(false),
    add_x_forwarded_for(false),
    accesslog(false),
    spdy_upstream_window_bits(0),
    pid_file(0),
    uid(0),
    gid(0),
    conf_path(0)
{}

namespace {
Config *config = 0;
} // namespace

const Config* get_config()
{
  return config;
}

Config* mod_config()
{
  return config;
}

void create_config()
{
  config = new Config();
}

namespace {
int split_host_port(char *host, size_t hostlen, uint16_t *port_ptr,
                    const char *hostport)
{
  // host and port in |hostport| is separated by single ','.
  const char *p = strchr(hostport, ',');
  if(!p) {
    std::cerr << "Invalid host, port: " << hostport << std::endl;
    return -1;
  }
  size_t len = p-hostport;
  if(hostlen < len+1) {
    std::cerr << "Hostname too long: " << hostport << std::endl;
    return -1;
  }
  memcpy(host, hostport, len);
  host[len] = '\0';

  errno = 0;
  unsigned long d = strtoul(p+1, 0, 10);
  if(errno == 0 && 1 <= d && d <= std::numeric_limits<uint16_t>::max()) {
    *port_ptr = d;
    return 0;
  } else {
    std::cerr << "Port is invalid: " << p+1 << std::endl;
    return -1;
  }
}
} // namespace

void set_config_str(char **destp, const char *val)
{
  if(*destp) {
    free(*destp);
  }
  *destp = strdup(val);
}

int parse_config(const char *opt, const char *optarg)
{
  char host[NI_MAXHOST];
  uint16_t port;
  if(util::strieq(opt, SHRPX_OPT_BACKEND)) {
    if(split_host_port(host, sizeof(host), &port, optarg) == -1) {
      return -1;
    } else {
      set_config_str(&mod_config()->downstream_host, host);
      mod_config()->downstream_port = port;
    }
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND)) {
    if(split_host_port(host, sizeof(host), &port, optarg) == -1) {
      return -1;
    } else {
      set_config_str(&mod_config()->host, host);
      mod_config()->port = port;
    }
  } else if(util::strieq(opt, SHRPX_OPT_WORKERS)) {
    mod_config()->num_worker = strtol(optarg, 0, 10);
  } else if(util::strieq(opt, SHRPX_OPT_SPDY_MAX_CONCURRENT_STREAMS)) {
    mod_config()->spdy_max_concurrent_streams = strtol(optarg, 0, 10);
  } else if(util::strieq(opt, SHRPX_OPT_LOG_LEVEL)) {
    if(Log::set_severity_level_by_name(optarg) == -1) {
      std::cerr << "Invalid severity level: " << optarg << std::endl;
      return -1;
    }
  } else if(util::strieq(opt, SHRPX_OPT_DAEMON)) {
    mod_config()->daemon = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_SPDY_PROXY)) {
    mod_config()->spdy_proxy = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_ADD_X_FORWARDED_FOR)) {
    mod_config()->add_x_forwarded_for = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_SPDY_READ_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->spdy_upstream_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_READ_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->upstream_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_WRITE_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->upstream_write_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_READ_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->downstream_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_WRITE_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->downstream_write_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_ACCESSLOG)) {
    mod_config()->accesslog = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->downstream_idle_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS)) {
    errno = 0;
    unsigned long int n = strtoul(optarg, 0, 10);
    if(errno == 0 && n < 31) {
      mod_config()->spdy_upstream_window_bits = n;
    } else {
      std::cerr << "-w: specify the integer in the range [0, 30], inclusive"
                << std::endl;
      return -1;
    }
  } else if(util::strieq(opt, SHRPX_OPT_PID_FILE)) {
    set_config_str(&mod_config()->pid_file, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_USER)) {
    passwd *pwd = getpwnam(optarg);
    if(pwd == 0) {
      std::cerr << "--user: failed to get uid from " << optarg
                << ": " << strerror(errno) << std::endl;
      return -1;
    }
    mod_config()->uid = pwd->pw_uid;
    mod_config()->gid = pwd->pw_gid;
  } else if(util::strieq(opt, SHRPX_OPT_PRIVATE_KEY_FILE)) {
    set_config_str(&mod_config()->private_key_file, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_CERTIFICATE_FILE)) {
    set_config_str(&mod_config()->cert_file, optarg);
  } else if(util::strieq(opt, "conf")) {
    std::cerr << "conf is ignored" << std::endl;
  } else {
    std::cerr << "Unknown option: " << opt << std::endl;
    return -1;
  }
  return 0;
}

int load_config(const char *filename)
{
  std::ifstream in(filename, std::ios::binary);
  if(!in) {
    std::cerr << "Could not open config file " << filename << std::endl;
    return -1;
  }
  std::string line;
  int linenum = 0;
  while(std::getline(in, line)) {
    ++linenum;
    if(line.empty() || line[0] == '#') {
      continue;
    }
    size_t i;
    size_t size = line.size();
    for(i = 0; i < size && line[i] != '='; ++i);
    if(i == size) {
      std::cerr << "Bad configuration format at line " << linenum << std::endl;
      return -1;
    }
    line[i] = '\0';
    const char *s = line.c_str();
    if(parse_config(s, s+i+1) == -1) {
      return -1;
    }
  }
  return 0;
}

} // namespace shrpx
