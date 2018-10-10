////////////////////////////////////////////////////////////////////////////////
//
// netStream.h (Network Stream Header)
// For establishing and managing network connections
//
////////////////////////////////////////////////////////////////////////////////


// Defines
#ifndef netStream_h
#define netStream_h
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#define NSP_BUFFER_RECEIVE_MAX 1024
#define SIN_FAMILY AF_INET
// netStream Open Types
#define NSOT_CONNECT "NSOT_CONNECT" //Try only to connect, close stream if failed 
#define NSOT_LISTEN "NSOT_LISTEN" //Wait only for incoming connections
#define NSOT_CONNECT_LISTEN "NSOT_CONNECT_LISTEN" //First try to connect, if failed wait for connection
// Peer Status
#define NSPS_INACTIVE "NSPS_INACTIVE"
#define NSPS_CONNECTED "NSPS_CONNECTED"
#define NSPS_CONNECTING "NSPS_CONNECTING"
#define NSPS_WAITING "NSPS_WAITING"
#define NSPS_DISCONNECTED "NSPS_DISCONNECTED"
#define NSPS_DESTRUCTED "NSPS_DESTRUCTED"
#endif

// Includes
#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <cstdio>
#include <WinSock2.h>
#include <vector>
#include <thread>
#include <chrono>
#include <map>
#include <sstream>
#include <istream>

// netStream Debug Feedback Function
void report(std::string p_msg)
{
	std::cout << std::this_thread::get_id() << " => " << p_msg << std::endl;
}
void report(char* p_msg)
{
	std::cout << std::this_thread::get_id() << " => " << p_msg << std::endl;
}
void report(int p_msg)
{
	std::cout << std::this_thread::get_id() << " => " << p_msg << std::endl;
}

class netStream
{
	public:
		class peer
		{
			public:
				// Constructor
				peer::peer(bool p_nsp_debug)
				{
					NSP_DEBUG = p_nsp_debug;
					NSPS = NSPS_INACTIVE;
					nsp_can_broadcast = true;
					nsp_interval_broadcast_check = 1000;
					nsp_interval_heartbeat = 5000;
				}
				peer::~peer()
				{
					closesocket(nsp_socket_peer);
					NSPS = NSPS_DESTRUCTED;
				}
				// NSP Settings
				bool NSP_DEBUG;				// netStream Peer Debug Mode
				char* NSPS;					// netStream Peer Status
				sockaddr_in nsp_info_peer;	// netStream Peer Socket infos
				SOCKET nsp_socket_peer;		// netStream Peer Socket
				bool nsp_can_broadcast;		// netStream Peer Broadcast privileges
				int nsp_interval_heartbeat;
				int nsp_interval_broadcast_check;	//netStream nsp_stream_receive check interval
				char nsp_buffer_receive[NSP_BUFFER_RECEIVE_MAX];
				char* nsp_buffer_send;
				long nsp_temp_input;
				long nsp_temp_output;
				int nsp_statistics_received = 0;
				int nsp_statistics_sent = 0;
				int nsp_statistics_lost = 0;
				std::chrono::steady_clock::time_point nsp_time_heartbeat_sent;
				std::chrono::steady_clock::time_point nsp_time_heartbeat_response;
				std::vector<std::string> nsp_queue_payload;
				std::map<char*, char*> nsp_vmap;

				// Main Functions
				void take()
				{
					NSP_DEBUG ? report("{ nsp_take }") : NSP_DEBUG;
					NSPS = NSPS_CONNECTED;
					std::thread nsp_thread_stream_receive(&peer::nsp_stream_receive, this);
					nsp_thread_stream_receive.detach();
					std::thread nsp_thread_heartbeat(&peer::nsp_routine_heartbeat, this);
					nsp_thread_heartbeat.detach();
				}

				void nsp_stream_receive()
				{
					NSP_DEBUG ? report("{ nsp_stream_receive }") : NSP_DEBUG;
					while (!nsp_can_broadcast)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(nsp_interval_broadcast_check));
						NSP_DEBUG ? report("{ BROADCAST DISABLED }") : NSP_DEBUG;
					}
					// Receiving routine
					while (NSPS == NSPS_CONNECTED)
					{
						memset(&nsp_buffer_receive, 0, sizeof(nsp_buffer_receive));
						nsp_temp_input = recv(nsp_socket_peer, nsp_buffer_receive, sizeof(nsp_buffer_receive), 0);
						if (nsp_temp_input == SOCKET_ERROR)
						{
							NSP_DEBUG ? report("- SOCK_PEER") : NSP_DEBUG;
							nsp_socket_peer = closesocket(nsp_socket_peer);
							NSPS = NSPS_DISCONNECTED;
						}
						else
						{
							nsp_statistics_received++;
							nsp_handle_receive(nsp_buffer_receive, nsp_temp_output);
						}
					}
					NSP_DEBUG ? report(NSPS) : NSP_DEBUG;
				}

				void nsp_broadcast(std::string p_payload)
				{
					NSP_DEBUG ? report("{ nsp_broadcast }") : NSP_DEBUG;
					if (NSPS == NSPS_CONNECTED)
					{
						nsp_temp_output = send(nsp_socket_peer, p_payload.c_str(), strlen(p_payload.c_str()), 0);
						nsp_temp_output != SOCKET_ERROR ? nsp_statistics_sent++ : nsp_statistics_lost++;
					}
				}

				void nsp_routine_heartbeat()
				{
					NSP_DEBUG ? report("{ nsp_routine_heartbeat }") : NSP_DEBUG;
					while (NSPS == NSPS_CONNECTED)
					{
						nsp_time_heartbeat_sent = std::chrono::steady_clock::now();
						nsp_broadcast("0xHB");
						std::this_thread::sleep_for(std::chrono::milliseconds(nsp_interval_heartbeat));
					}
				}

				void nsp_handle_receive(char* p_payload, int p_size)
				{
					NSP_DEBUG ? report("{ nsp_handle_receive }") : NSP_DEBUG;
					std::string payload = p_payload;
					// Handle Payload parts
					if (payload == "0xHB")
					{
						nsp_broadcast("0xHX");
					}
					else if (payload == "0xHX")
					{
						nsp_time_heartbeat_response = std::chrono::steady_clock::now();
					}
					else
					{
						// Add Payload to the BEGINNING of the payload list as newest element
						nsp_queue_payload.insert(nsp_queue_payload.begin(), payload);
					}
				}

				// Get Next Payload in direction Old -> New
				std::string nsp_queue_payload_next()
				{
					if (nsp_queue_payload.size() > 0)
					{
						return nsp_queue_payload.back();
					}
					else 
					{
						return "";
					}
				}

				// Clean Queue from oldest payload (Used after nsp_queue_payload_next())
				void nsp_queue_payload_pop()
				{
					if (nsp_queue_payload.size() > 0)
					{
						nsp_queue_payload.pop_back();
					}
				}

				int nsp_get_ping()
				{
					NSP_DEBUG ? report("{ ns_get_ping} ") : NSP_DEBUG;
					//std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(nsp_time_heartbeat_response - nsp_time_heartbeat_sent).count() << std::endl;
					return std::chrono::duration_cast<std::chrono::milliseconds>(nsp_time_heartbeat_response - nsp_time_heartbeat_sent).count();
				}
		};

		// Constructor
		netStream::netStream(char* p_NSOT, bool p_NS_DEBUG)
		{
			NS_DEBUG = p_NS_DEBUG;
			NSOT = p_NSOT;
		}
		// NS Settings
		bool NS_DEBUG;			// netStream Debug Mode
		char* NSOT;				// netStream Open Type
		// NS Settings Variables
		char* ns_ip_listen;
		int ns_port_listen;
		char* ns_ip_connect;
		int ns_port_connect;
		// Variables
		long ns_temp_input;
		long ns_temp_output;
		// Sockets
		SOCKET ns_socket_listen;
		// Peer Pool
		std::vector<peer*> ns_pool_peers;

		// Settings Functions
		void set_listen_data(char* p_ns_ip_listen, int p_ns_port_listen)
		{
			ns_ip_listen = p_ns_ip_listen;
			ns_port_listen = p_ns_port_listen;
		}
		void set_connect_data(char* p_ns_ip_connect, int p_ns_port_connect)
		{
			ns_ip_connect = p_ns_ip_connect;
			ns_port_connect = p_ns_port_connect;
		}

		// Main Functions
		void open()
		{
			NS_DEBUG ? report("{ ns_open }") : NS_DEBUG;
			if (NSOT == NSOT_CONNECT || NSOT == NSOT_CONNECT_LISTEN)
			{
				std::thread ns_thread_open(&netStream::ns_routine_connect, this);
				ns_thread_open.detach();
			}
			else if (NSOT == NSOT_LISTEN)
			{
				std::thread ns_thread_open(&netStream::ns_routine_listen, this);
				ns_thread_open.detach();
			}
		}

		void ns_routine_connect()
		{
			// Initialize
			NS_DEBUG ? report(" { ns_routine_connect }") : NS_DEBUG;
			ns_pool_peers.push_back(new peer(NS_DEBUG));
			ns_pool_peers.back()->NSPS = NSPS_CONNECTING;
			WSAData wsaData;
			ns_temp_output = WSAStartup(MAKEWORD(2, 0), &wsaData);
			NS_DEBUG ? ns_temp_output == 0 ? report("+ WSA") : report("- WSA") : NS_DEBUG;
			ns_pool_peers.back()->nsp_socket_peer = socket(AF_INET, SOCK_STREAM, 0);
			NS_DEBUG ? ns_pool_peers.back()->nsp_socket_peer != INVALID_SOCKET ? report("+ SOCK_PEER") : report("- SOCK_PEER") : NS_DEBUG;
			sockaddr_in ns_info_connect;
			ns_info_connect.sin_addr.s_addr = inet_addr(ns_ip_connect);
			ns_info_connect.sin_family = SIN_FAMILY;
			ns_info_connect.sin_port = htons(ns_port_connect);
			int ns_info_connect_length = sizeof(ns_info_connect);

			// Establish
			ns_temp_output = connect(ns_pool_peers.back()->nsp_socket_peer, (struct sockaddr*)&ns_info_connect, ns_info_connect_length);
			if (ns_temp_output != SOCKET_ERROR)
			{
				ns_pool_peers.back()->NSPS = NSPS_CONNECTED;
				NS_DEBUG ? report("+ CONNECTION") : NS_DEBUG;
				ns_pool_peers.back()->take();
			}
			else
			{
				NS_DEBUG ? report("- CONNECTION") : NS_DEBUG;
				if (NSOT == NSOT_CONNECT_LISTEN)
				{
					NS_DEBUG ? report("{ NSOT CAUSES TASK CHANGE TO LISTEN }") : NS_DEBUG;
					ns_routine_listen();
				}
				else
				{
					NS_DEBUG ? report("{ NSOT CAUSES TASK TO STOP }") : NS_DEBUG;
				}
			}
		}

		void ns_routine_listen()
		{
			// Initialize
			NS_DEBUG ? report("{ ns_listen }") : NS_DEBUG;
			WSADATA wsaData;
			ns_temp_input = WSAStartup(MAKEWORD(2, 0), &wsaData);
			NS_DEBUG ? ns_temp_input == 0 ? report("+ WSA") : report("- WSA") : NS_DEBUG;
			ns_socket_listen = socket(AF_INET, SOCK_STREAM, 0);
			NS_DEBUG ? ns_socket_listen != INVALID_SOCKET ? report("+ SOCK_LISTEN") : report("- SOCK_LISTEN") : NS_DEBUG;
			sockaddr_in ns_info_listen;
			ns_info_listen.sin_addr.s_addr = inet_addr(ns_ip_listen);
			ns_info_listen.sin_family = SIN_FAMILY;
			ns_info_listen.sin_port = htons(ns_port_listen);
			int ns_info_listen_length = sizeof(ns_info_listen);

			// Bind Socket
			ns_temp_input = bind(ns_socket_listen, (struct sockaddr*)&ns_info_listen, ns_info_listen_length);
			NS_DEBUG ? ns_temp_input != INVALID_SOCKET ? report("+ SOCK_BIND") : report("- SOCK_BIND") : NS_DEBUG;

			// Listen
			ns_temp_input = listen(ns_socket_listen, 1);
			NS_DEBUG ? ns_temp_input != SOCKET_ERROR ? report("+ LISTEN") : report("- LISTEN") : NS_DEBUG;
			while (true)
			{
				try
				{
					// Prepare Peer
					ns_pool_peers.push_back(new peer(NS_DEBUG));

					// Start to Listen
					ns_pool_peers.back()->NSPS = NSPS_WAITING;

					// Handling incoming peer
					int ns_info_peer_length = sizeof(ns_pool_peers.back()->nsp_info_peer);
					ns_pool_peers.back()->nsp_socket_peer = accept(ns_socket_listen, (struct sockaddr*)&ns_pool_peers.back()->nsp_info_peer, &ns_info_peer_length);
					if (ns_pool_peers.back()->nsp_socket_peer != SOCKET_ERROR)
					{
						NS_DEBUG ? report("+ ACCEPT, SEND TO PEER CLASS") : NS_DEBUG;
						ns_pool_peers.back()->take();
					}
				}
				catch (...)
				{
					
				}
				//std::this_thread::sleep_for(std::chrono::milliseconds(20)); // This hits up the Iterator fuck error?
			}
		}
		
};