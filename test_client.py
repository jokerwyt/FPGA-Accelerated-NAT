from scapy.all import Ether, IP, TCP, sendp


# veth2-1 f2:3e:82:54:05:c5
# veth1-2 b6:9d:50:bf:43:b2

ethernet_frame = Ether(src='f2:3e:82:54:05:c5', dst='b6:9d:50:bf:43:b2')

ip_packet = IP(src='111.111.111.111', dst='222.222.222.222')

tcp_packet = TCP(sport=1234, dport=80, flags='S', seq=1000)

packet = ethernet_frame / ip_packet / tcp_packet

sendp(packet, iface='veth2-1')
