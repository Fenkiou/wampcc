#include "XXX/kernel.h"

#include "XXX/io_loop.h"
#include "XXX/event_loop.h"

#include <iostream>

#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/time.h>
#include <stdio.h>


namespace XXX {

/* Constructor */
kernel::kernel(config __conf, logger nlog)
  : m_config(__conf),
    __logger(nlog),
    m_io_loop( new io_loop(*this) ),
    m_evl( new event_loop(__logger) )
{
}

/* Destructor */
kernel::~kernel()
{
  // TODO: dont think this is the best way to shutdown.  Should start by trying
  // to close all the sessions.
  m_io_loop->stop();
  m_evl->stop();

  m_evl.reset();
}

void kernel::start()
{
  /* USER thread */

  m_io_loop->start(); // returns immediately
}


io_loop*  kernel::get_io()
{
  return m_io_loop.get();
}

event_loop* kernel::get_event_loop()
{
  return m_evl.get();
}

int logger::levels_upto(Level l)
{
  int r(0);
  for (int i = 1; i <= l; i <<= 1) r |= i;
  return r;
}


class stdout_logger
{
public:

  stdout_logger(std::ostream& __os,
                bool __incsource)
    : m_stream(__os),
      m_incsource(__incsource)
  {
  }

  void write(logger::Level, const std::string&, const char*, int);

private:

  stdout_logger(const stdout_logger&) = delete;
  stdout_logger& operator=(const stdout_logger&) = delete;

  std::ostream&             m_stream;
  bool                      m_incsource;
  std::mutex                m_mutex;
};


logger logger::stdlog(std::ostream& ostr,
                        int level_mask,
                        bool inc_src)
{
  auto sp = std::make_shared<stdout_logger>(ostr, inc_src);

  logger my_logger;

  my_logger.wants_level = [level_mask](logger::Level l)
    {
      return l bitand level_mask;
    };

  my_logger.write = [level_mask, sp](logger::Level l,
                                     const std::string& msg,
                                     const char* file, int ln)
    {
      if (l bitand level_mask) sp->write(l, msg, file, ln);
    };

  return my_logger;
}


static const char* level_str(logger::Level l)
{
  switch(l)
  {
    case logger::eError : return "ERROR";
    case logger::eWarn  : return "WARN";
    case logger::eInfo  : return "INFO";
    case logger::eDebug : return "DEBUG";
    default : return "UNKNOWN";
  }
}


void stdout_logger::stdout_logger::write(logger::Level l,
                                         const std::string& s,
                                         const char* file,
                                         int ln)
{
  int tid = syscall(SYS_gettid);

  // get current time
  timeval now;
  struct timezone * const tz = NULL; /* not used on Linux */
  gettimeofday(&now, tz);

  // break time down into parts
  struct tm parts;
  localtime_r(&now.tv_sec, &parts);

  // build timestamp
  char timestamp[30];
  snprintf(timestamp,
           sizeof(timestamp),
           "%02d%02d%02d-%02d:%02d:%02d.%06lu ",
           parts.tm_year + 1900,
           parts.tm_mon + 1,
           parts.tm_mday,
           parts.tm_hour,parts.tm_min,parts.tm_sec,now.tv_usec);


  std::lock_guard<std::mutex> lock( m_mutex );
  m_stream << timestamp;
  m_stream << tid << " ";
  m_stream << level_str(l) << " : ";
  m_stream << s;

  if (m_incsource && file)
  {
    m_stream << " (" << file << ":" << ln << ") ";
  }
  m_stream << std::endl;
}


} // namespace XXX