from scapy.all import Ether, IP, TCP, sendp

ethernet_frame = Ether(src='你的MAC地址', dst='目标MAC地址')

ip_packet = IP(src='源IP地址', dst='目标IP地址')

tcp_packet = TCP(sport=1234, dport=80, flags='S', seq=1000)

packet = ethernet_frame / ip_packet / tcp_packet

sendp(packet, iface='enp2s0d1')
