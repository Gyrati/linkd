// NSConnect.cpp : Definiert den Einstiegspunkt f�r die Konsolenanwendung.
//

#include "stdafx.h"
#include "../../../network_adapter_netstream_header/network_adapter_netstream_header/netStream.h"


int main()
{
	netStream ns = netStream(NSOT_CONNECT, false);
	ns.set_connect_data("127.0.0.1", 2232);
	ns.open();
	std::string bf = "1";
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	while (true)
	{
		std::cin >> bf;
		ns.ns_pool_peers.back()->nsp_broadcast(bf);
		while (ns.ns_pool_peers.back()->nsp_queue_payload_next().size() == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		pkhlol
		//ns.ns_pool_peers.back()->nsp_broadcast("This is a test where Whitespace is not divided when received by server");
		// In this example, the payload is divided by Whitespace. THIS DOES NOT HAPPEN IF SENT DATA != CIN
	}
    return 0;
}

