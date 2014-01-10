#include <iostream>
#include <map>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>

typedef websocketpp::server<websocketpp::config::asio> ws_server;

#define WSP_LOG 1

class wsproxy_client
{
public:
  typedef boost::asio::local::stream_protocol::socket unix_socket;
  
  wsproxy_client (boost::asio::io_service *io_srvc, ws_server* wss,
		  websocketpp::connection_hdl hdl)
    : m_ux_sock (*io_srvc), m_wss (*wss), m_ws_hdl (hdl)
  {
    // Setup Unix socket connection to ndnd
    boost::asio::local::stream_protocol::endpoint local_ndnd ("/tmp/.ndnd.sock");

#ifdef WSP_LOG
    std::cout << "wsproxy_client::ctor: trying to connect to local ndnd." << std::endl;
#endif
    m_ux_sock.connect(local_ndnd);
#ifdef WSP_LOG
    std::cout << "wsproxy_client::ctor: connected to local ndnd." << std::endl;
#endif
    m_ux_sock.async_read_some(boost::asio::buffer (m_buf), boost::bind(&wsproxy_client::on_unix_message, this, _1, _2));
  }

  void send (const std::string& msg)
  {
    boost::asio::write (m_ux_sock, boost::asio::buffer (msg));
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
	m_wss.close (m_ws_hdl, websocketpp::close::status::normal, "ndnd close");
      }
  }

  ws_server& m_wss;
  websocketpp::connection_hdl m_ws_hdl;
  unix_socket m_ux_sock;
  boost::array<char, 8192> m_buf;
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
    m_web_sock.listen (9696);
    m_web_sock.start_accept ();
  }
  
  void run ()
  {
#ifdef WSP_LOG
    std::cout << "wsproxy_server::run: start asio service." << std::endl;
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
	std::cout << "wsproxy_server::on_ws_message: unknown ws client." << std::endl;
      }
    wsproxy_client* wcp = it->second;
    wcp->send (msg->get_payload ());
  }

  void on_ws_open (websocketpp::connection_hdl hdl)
  {
    // New WebSocket client accepted
#ifdef WSP_LOG
    std::cout << "wsproxy_server::on_ws_open: create new client." << std::endl;
#endif
    m_clist[hdl] = new wsproxy_client(&m_io_srvc, &m_web_sock, hdl);
  }

  void on_ws_close(websocketpp::connection_hdl hdl)
  {
#ifdef WSP_LOG
    std::cout << "wsproxy_server::on_ws_close: remove client." << std::endl;
#endif
    auto it = m_clist.find (hdl);
    if (it != m_clist.end ())
      {
	wsproxy_client* wcp = it->second;
	delete wcp;
	m_clist.erase(it);
      }
  }

  typedef std::map<websocketpp::connection_hdl, wsproxy_client*, std::owner_less<websocketpp::connection_hdl> > client_list;
  
  boost::asio::io_service m_io_srvc;
  ws_server m_web_sock;
  client_list m_clist;

};



int main ()
{
  wsproxy_server pserver;
  pserver.run ();
}
