#include <iostream>
#include <map>
#include <cstdint>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>

typedef websocketpp::server<websocketpp::config::asio> ws_server;

#define WSP_LOG 1

typedef struct proxy_config
{
  const char *ndnd_addr;
  int max_clients;
  uint16_t proxy_port; 
} proxy_config;

// Global configuration
proxy_config conf;

class wsproxy_client
{
public:
  typedef boost::asio::local::stream_protocol::socket unix_socket;
  
  wsproxy_client (boost::asio::io_service *io_srvc, ws_server* wss,
		  websocketpp::connection_hdl hdl)
    : m_ux_sock (*io_srvc), m_wss (*wss), m_ws_hdl (hdl), m_closed (false)
  {
    // Setup Unix socket connection to ndnd
    boost::asio::local::stream_protocol::endpoint local_ndnd (conf.ndnd_addr);

#ifdef WSP_LOG
    std::cout << "wsproxy_client::ctor: trying to connect to " << conf.ndnd_addr << std::endl;
#endif
    m_ux_sock.connect(local_ndnd);
#ifdef WSP_LOG
    std::cout << "wsproxy_client::ctor: connected to local ndnd" << std::endl;
#endif
    m_ux_sock.async_read_some(boost::asio::buffer (m_buf), boost::bind(&wsproxy_client::on_unix_message, this, _1, _2));
  }

  // ~wsproxy_client ()
  // {
  //   std::cout << "wsproxy_client::dtor: called." << std::endl;
  // }

  void send (const std::string& msg)
  {
    m_ux_sock.async_send(boost::asio::buffer (msg), boost::bind(&wsproxy_client::on_unix_sent, this, _1, _2));
  }

  void set_closed ()
  {
    m_closed = true;
  }
  
private:
  void on_unix_message (const boost::system::error_code &ec, std::size_t bytes_transferred)
  {
    if (!ec)
      {
	std::string msg (m_buf.data (), bytes_transferred);
#ifdef WSP_LOG
	std::cout << "wsproxy_client::on_unix_message: " << msg << std::endl;
#endif
	m_wss.send(m_ws_hdl, msg, websocketpp::frame::opcode::binary);
	m_ux_sock.async_read_some(boost::asio::buffer (m_buf), boost::bind(&wsproxy_client::on_unix_message, this, _1, _2));
      }
    else
      {
#ifdef WSP_LOG
	std::cout << "wsproxy_client::on_unix_message: unix socket error" << std::endl;
#endif
	if (m_closed == false)
	  {
	    websocketpp::lib::error_code ecode;
	    m_wss.close (m_ws_hdl, websocketpp::close::status::normal, "ndnd close", ecode);
	  }
      }
  }

  void on_unix_sent (const boost::system::error_code &ec, std::size_t bytes_transferred)
  {
    // Dummy callback
  }

  unix_socket m_ux_sock;
  ws_server& m_wss;
  websocketpp::connection_hdl m_ws_hdl;
  boost::array<char, 8192> m_buf;
  bool m_closed;
};

class wsproxy_server
{
public:
  wsproxy_server ()
  {
    // Setup WebSocket server
    m_web_sock.clear_access_channels(websocketpp::log::alevel::all);
    m_web_sock.clear_error_channels(websocketpp::log::alevel::all);

    m_web_sock.set_message_handler (boost::bind(&wsproxy_server::on_ws_message, this, _1, _2));
    m_web_sock.set_open_handler (boost::bind(&wsproxy_server::on_ws_open, this, _1));
    m_web_sock.set_close_handler (boost::bind(&wsproxy_server::on_ws_close, this, _1));
    m_web_sock.init_asio (&m_io_srvc);
    m_web_sock.listen (conf.proxy_port);
    m_web_sock.start_accept ();
  }
  
  void run ()
  {
#ifdef WSP_LOG
    std::cout << "wsproxy_server::run: start asio service" << std::endl;
#endif
    m_io_srvc.run ();
  }
  
private:
  void on_ws_message (websocketpp::connection_hdl hdl, ws_server::message_ptr msg)
  {
#ifdef WSP_LOG
    std::cout << "wsproxy_server::on_ws_message: " << msg->get_payload () << std::endl;
#endif
    auto it = m_clist.find (hdl);
    if (it == m_clist.end ())
      {
	std::cout << "wsproxy_server::on_ws_message: unknown ws client" << std::endl;
      }
    auto wcp = it->second;
    wcp->send (msg->get_payload ());
  }

  void on_ws_open (websocketpp::connection_hdl hdl)
  {
    // New WebSocket client accepted    
    if (m_clist.size () >= conf.max_clients)
      {
	std::cout << "wsproxy_server::on_ws_open: max # of clients exceeded" << std::endl;
	websocketpp::lib::error_code ecode;
	m_web_sock.close (hdl, websocketpp::close::status::normal, "max clients", ecode);
	return;	
      }

#ifdef WSP_LOG
    std::cout << "wsproxy_server::on_ws_open: create new client" << std::endl;
#endif    
    std::shared_ptr<wsproxy_client> wcp;
    try {
      wcp = std::make_shared<wsproxy_client>(&m_io_srvc, &m_web_sock, hdl);
    }
    catch (boost::system::system_error e) {
      std::cout << "wsproxy_server::on_ws_open: error on " << e.what () << std::endl;
      websocketpp::lib::error_code ecode;
      m_web_sock.close (hdl, websocketpp::close::status::normal, "ndnd dead", ecode);
      return;
    }
    m_clist[hdl] = wcp;
#ifdef WSP_LOG
    std::cout << "wsproxy_server::on_ws_open: num of client is " << m_clist.size () << std::endl;
#endif
  }

  void on_ws_close(websocketpp::connection_hdl hdl)
  {
    auto it = m_clist.find (hdl);
    if (it != m_clist.end ())
      {
#ifdef WSP_LOG
    std::cout << "wsproxy_server::on_ws_close: remove client" << std::endl;
#endif
	it->second->set_closed ();
	m_clist.erase(it);
      }
  }

  typedef std::map<websocketpp::connection_hdl, std::shared_ptr<wsproxy_client>, std::owner_less<websocketpp::connection_hdl> > client_list;
  
  boost::asio::io_service m_io_srvc;
  ws_server m_web_sock;
  client_list m_clist;

};

void init_config (proxy_config *conf)
{
  conf->ndnd_addr = "/tmp/.ndnd.sock";
  conf->max_clients = 50;
  conf->proxy_port = 9696;
}

void usage ()
{
  std::cout << "Usage: ws_proxy [options]\n"
	    << "Supported options:\n"
	    << "    -c [ndnd_addr]: specify the unix socket address for local ndnd\n"
	    << "                    by default, it is /tmp/.ndnd.sock\n"
	    << "    -m [max_clients]: specify the number of maximum concurrent clients\n"
	    << "                      by default, it is 50\n"
	    << "    -p [proxy_port]: specify the port number for the proxy\n"
	    << "                     by default, it is 9696\n";
  exit (1);
}

void parse_arguments (proxy_config *conf, int argc, char* argv[])
{
  int i = 1;
  while (i < argc)
    {
      if (strcmp (argv[i], "-c") == 0)
	{
	  i++;
	  if (i >= argc)
	    usage ();
	  
	  conf->ndnd_addr = argv[i];
	  i++;
	}
      else if (strcmp (argv[i], "-m") == 0)
	{
	  i++;
	  if (i >= argc)
	    usage ();
	  
	  conf->max_clients = atoi (argv[i]);
	  i++;
	}
      else if (strcmp (argv[i], "-p") == 0)
	{
	  i++;
	  if (i >= argc)
	    usage ();
	  
	  conf->proxy_port = (uint16_t) atoi (argv[i]);
	  i++;
	}
      else
	{
	  // Fail and exit upon unknown options
	  usage ();
	}
    }
}

int main (int argc, char* argv[])
{
  // Load default configuration
  init_config (&conf);
  
  // Parse command line arguments
  if (argc > 1)
    {
      parse_arguments (&conf, argc, argv);
    }

  std::cout << "main: conf.ndnd_addr = " << conf.ndnd_addr << std::endl
	    << "      conf.max_clients = " << conf.max_clients << std::endl
	    << "      conf.proxy_port = " << conf.proxy_port << std::endl;

  // Start service
  wsproxy_server pserver;
  pserver.run ();
}
